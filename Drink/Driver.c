/**
 * @file Driver.c
 * @author biko
 * @date 2016-07-29
 *
 * The driver's entry-point.
 */

/** Headers *************************************************************/
#include <ntifs.h>

#include <Common.h>
#include <Drink.h>

#include "Util.h"
#include "VgaDump.h"
#include "Carpenter.h"
#include "QRPatch.h"


/** Macros **************************************************************/

/**
 * Completes an IRP.
 *
 * @param[in]	ptIrp			IRP to complete.
 * @param[in]	eStatus			Status to complete the IRP with
 *								(will be set in the IO_STATUS block).
 * @param[in]	pvInformation	Request-specific information
 *								(will be set in the IO_STATUS block).
 * @param[in]	ePriorityBoost	Priority boost for the issuer thread.
 *
 * @see driver_CompleteRequest.
 */
#define COMPLETE_IRP(ptIrp, eStatus, pvInformation, ePriorityBoost)	\
	CLOSE_TO_VALUE_VARIADIC((ptIrp),								\
							driver_CompleteRequest,					\
							NULL,									\
							(eStatus),								\
							(pvInformation),						\
							(ePriorityBoost))


/** Globals *************************************************************/

/**
 * Full name of the control device.
 */
STATIC CONST UNICODE_STRING g_usControlDeviceName =
	RTL_CONSTANT_STRING(L"\\Device\\" DRINK_DEVICE_NAME);

/**
 * Full name of the control device's symlink.
 */
STATIC CONST UNICODE_STRING g_usControlDeviceSymlink =
	RTL_CONSTANT_STRING(L"\\DosDevices\\Global\\" DRINK_DEVICE_NAME);

/**
 * Synchronizes access to the VGA dump module.
 */
STATIC KSPIN_LOCK g_tVgaDumpLock = { 0 };

/**
 * Indicates whether VGA dumping has been initialized.
 */
_Guarded_by_(g_tVgaDumpLock)
STATIC BOOLEAN g_bVgaDumpInitialized = FALSE;

/**
 * Synchronizes access to the IOCTL_DRINK_VANITY handler.
 */
STATIC KMUTEX g_tVanityLock = { 0 };


/** Forward Declarations ************************************************/

/**
 * Initializes the Auxiliary Kernel-Mode Library.
 *
 * @returns NTSTATUS
 *
 * @remark Adapted from aux_klib.h.
 */
NTSTATUS
NTAPI
AuxKlibInitialize(VOID);


/** Functions ***********************************************************/

/**
 * The driver's unload routine.
 *
 * @param[in]	ptDriverObject	Pointer to the driver object.
 */
_IRQL_requires_(PASSIVE_LEVEL)
PAGEABLE
STATIC
VOID
driver_Unload(
	_In_	PDRIVER_OBJECT	ptDriverObject
)
{
	PAGED_CODE();

	ASSERT(NULL != ptDriverObject);
	ASSERT(PASSIVE_LEVEL == KeGetCurrentIrql());

	if (g_bVgaDumpInitialized)
	{
		VGADUMP_Shutdown();
		g_bVgaDumpInitialized = FALSE;
	}

	// Delete the control device's symlink.
	(VOID)IoDeleteSymbolicLink((PUNICODE_STRING)&g_usControlDeviceSymlink);

	// Delete the control device.
	ASSERT(NULL != ptDriverObject->DeviceObject);
	(VOID)IoDeleteDevice(ptDriverObject->DeviceObject);
	ASSERT(NULL == ptDriverObject->DeviceObject);
}

/**
 * Completes an IRP.
 *
 * @param[in]	ptIrp			IRP to complete.
 * @param[in]	eStatus			Status to complete the IRP with
 *								(will be set in the IO_STATUS block).
 * @param[in]	pvInformation	Request-specific information
 *								(will be set in the IO_STATUS block).
 * @param[in]	ePriorityBoost	Priority boost for the issuer thread.
 *
 * @see COMPLETE_IRP.
 */
_IRQL_requires_max_(DISPATCH_LEVEL)
STATIC
VOID
driver_CompleteRequest(
	_In_	PIRP		ptIrp,
	_In_	NTSTATUS	eStatus,
	_In_	ULONG_PTR	pvInformation,
	_In_	CCHAR		ePriorityBoost
)
{
	ASSERT(NULL != ptIrp);
	ASSERT(DISPATCH_LEVEL >= KeGetCurrentIrql());

	ptIrp->IoStatus.Status = eStatus;
	ptIrp->IoStatus.Information = pvInformation;
	IoCompleteRequest(ptIrp, ePriorityBoost);
	ptIrp = NULL;
}

/**
 * Handler for IRP_MJ_CREATE and IRP_MJ_CLOSE requests.
 *
 * @param[in]	ptDeviceObject	The device object.
 * @param[in]	ptIrp			The IRP.
 *
 * @returns NTSTATUS
 */
_IRQL_requires_max_(DISPATCH_LEVEL)
STATIC
NTSTATUS
driver_DispatchCreateClose(
	_In_	PDEVICE_OBJECT	ptDeviceObject,
	_In_	PIRP			ptIrp
)
{
	NTSTATUS	eStatus	= STATUS_UNSUCCESSFUL;

#ifndef DBG
	UNREFERENCED_PARAMETER(ptDeviceObject);
#endif // !DBG

	ASSERT(NULL != ptDeviceObject);
	ASSERT(NULL != ptIrp);
	ASSERT(DISPATCH_LEVEL >= KeGetCurrentIrql());

	eStatus = STATUS_SUCCESS;

//lblCleanup:
	COMPLETE_IRP(ptIrp, eStatus, 0, IO_NO_INCREMENT);
	return eStatus;
}

/**
 * Handles IOCTL_DRINK_BUGSHOT.
 *
 * @returns NTSTATUS
 */
_IRQL_requires_max_(DISPATCH_LEVEL)
STATIC
NTSTATUS
driver_HandleBugshot(VOID)
{
	NTSTATUS	eStatus		= STATUS_UNSUCCESSFUL;
	KIRQL		eOldIrql	= HIGH_LEVEL;

	ASSERT(DISPATCH_LEVEL >= KeGetCurrentIrql());

	KeAcquireSpinLock(&g_tVgaDumpLock, &eOldIrql);
	{
		if (!g_bVgaDumpInitialized)
		{
			eStatus = VGADUMP_Initialize();
			g_bVgaDumpInitialized = NT_SUCCESS(eStatus);
		}
		else
		{
			eStatus = STATUS_ALREADY_COMMITTED;
		}
	}
	KeReleaseSpinLock(&g_tVgaDumpLock, eOldIrql);

	// Keep last status

//lblCleanup:
	return eStatus;
}

/**
 * Handles IOCTL_DRINK_VANITY.
 *
 * @param[in]	pvInputBuffer	The IOCTLs input buffer.
 * @param[in]	cbInputBuffer	Size of the input buffer, in bytes.
 *
 * @returns NTSTATUS
 *
 * @remark This function returns only on failure.
 */
_IRQL_requires_max_(DISPATCH_LEVEL)
STATIC
NTSTATUS
driver_HandleVanity(
	_In_	PVOID	pvInputBuffer,
	_In_	ULONG	cbInputBuffer
)
{
	NTSTATUS					eStatus			= STATUS_UNSUCCESSFUL;
	BOOLEAN						bLockAcquired	= FALSE;
	ANSI_STRING					sInputString	= { 0 };
	PAUX_MODULE_EXTENDED_INFO	ptModules		= NULL;
	ULONG						nModules		= 0;
	HCARPENTER					hCarpenter		= NULL;

	ASSERT(DISPATCH_LEVEL >= KeGetCurrentIrql());

	// We can't actually run above PASSIVE_LEVEL.
	if ((NULL == pvInputBuffer) ||
		(0 == cbInputBuffer) ||
		(PASSIVE_LEVEL != KeGetCurrentIrql()))
	{
		eStatus = STATUS_INVALID_PARAMETER;
		goto lblCleanup;
	}

	eStatus = KeWaitForSingleObject(&g_tVanityLock,
									Executive,
									KernelMode,
									FALSE,
									NULL);
	if (STATUS_SUCCESS != eStatus)
	{
		// Really shouldn't happen.
		KeBugCheck(eStatus);
	}
	bLockAcquired = TRUE;

	// Prepare the caller-supplied string.
	eStatus = UTIL_InitAnsiStringCb((PCHAR)pvInputBuffer,
									cbInputBuffer,
									&sInputString);
	if (!NT_SUCCESS(eStatus))
	{
		goto lblCleanup;
	}

	// Obtain a list of loaded drivers.
	eStatus = UTIL_QueryModuleInformation(&ptModules, &nModules);
	if (!NT_SUCCESS(eStatus))
	{
		goto lblCleanup;
	}

	// Prepare to patch the message table of ntoskrnl.exe.
	eStatus = CARPENTER_Create(ptModules[0].tBasicInfo.pvImageBase,
							   RT_MESSAGETABLE,
							   1,
							   MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
							   &hCarpenter);
	if (!NT_SUCCESS(eStatus))
	{
		goto lblCleanup;
	}

	// Insert the caller-supplied message.
	eStatus = CARPENTER_StageMessage(hCarpenter,
									 DRIVER_IRQL_NOT_LESS_OR_EQUAL,
									 &sInputString);
	if (!NT_SUCCESS(eStatus))
	{
		goto lblCleanup;
	}

	// Patch!
	eStatus = CARPENTER_ApplyPatch(hCarpenter);
	if (!NT_SUCCESS(eStatus))
	{
		goto lblCleanup;
	}

	// Goodbye.
	(VOID)KeRaiseIrqlToDpcLevel();
	*(volatile UCHAR *)NULL = 0;

	// Unreachable.
	KeBugCheck(MANUALLY_INITIATED_CRASH1);

	// Really unreachable.

lblCleanup:
	CLOSE(hCarpenter, CARPENTER_Destroy);
	CLOSE(ptModules, ExFreePool);
	if (bLockAcquired)
	{
		(VOID)KeReleaseMutex(&g_tVanityLock, FALSE);
		bLockAcquired = FALSE;
	}

	return eStatus;
}

/**
 * Handles IOCTL_DRINK_QR_INFO.
 *
 * @param[out]	pvOutputBuffer	The IOCTLs output buffer.
 * @param[in]	cbOutputBuffer	Size of the output buffer, in bytes.
 * @param[out]	pcbWritten		Will receive the amount of bytes written to the output buffer.
 *
 * @returns NTSTATUS
 */
_IRQL_requires_max_(DISPATCH_LEVEL)
STATIC
NTSTATUS
driver_HandleQrInfo(
	_Out_writes_bytes_to_(cbOutputBuffer, *pcbWritten)	PVOID	pvOutputBuffer,
	_In_												ULONG	cbOutputBuffer,
	_Out_												PULONG	pcbWritten
)
{
	NTSTATUS	eStatus		= STATUS_UNSUCCESSFUL;
	BITMAP_INFO	tBitmapInfo	= { 0 };
	QR_INFO		tQrInfo		= { 0 };

	ASSERT(DISPATCH_LEVEL >= KeGetCurrentIrql());

	NT_ASSERT(NULL != pcbWritten);

	*pcbWritten = 0;

	if (!UTIL_IsWindows10OrGreater())
	{
		eStatus = STATUS_NOT_SUPPORTED;
		goto lblCleanup;
	}

	if (NULL == pvOutputBuffer)
	{
		eStatus = STATUS_INVALID_PARAMETER;
		goto lblCleanup;
	}

	if (cbOutputBuffer < sizeof(tQrInfo))
	{
		eStatus = STATUS_BUFFER_TOO_SMALL;
		goto lblCleanup;
	}

	eStatus = QRPATCH_GetBitmapInfo(&tBitmapInfo);
	if (!NT_SUCCESS(eStatus))
	{
		goto lblCleanup;
	}

	tQrInfo.nWidth = tBitmapInfo.nWidth;
	tQrInfo.nHeight = tBitmapInfo.nHeight;
	tQrInfo.nBitCount = tBitmapInfo.nBitCount;

	RtlMoveMemory(pvOutputBuffer, &tQrInfo, sizeof(tQrInfo));
	*pcbWritten = sizeof(tQrInfo);

	eStatus = STATUS_SUCCESS;

lblCleanup:
	return eStatus;
}

/**
 * Handles IOCTL_DRINK_QR_SET.
 *
 * @param[in]	pvInputBuffer	The IOCTLs input buffer.
 * @param[in]	cbInputBuffer	Size of the input buffer, in bytes.
 *
 * @returns NTSTATUS
 */
_IRQL_requires_max_(DISPATCH_LEVEL)
STATIC
NTSTATUS
driver_HandleQrSet(
	_In_reads_bytes_(cbInputBuffer)	PVOID	pvInputBuffer,
	_In_							ULONG	cbInputBuffer
)
{
	NTSTATUS	eStatus	= STATUS_UNSUCCESSFUL;

	ASSERT(DISPATCH_LEVEL >= KeGetCurrentIrql());

	if (!UTIL_IsWindows10OrGreater())
	{
		eStatus = STATUS_NOT_SUPPORTED;
		goto lblCleanup;
	}

	if (NULL == pvInputBuffer)
	{
		eStatus = STATUS_INVALID_PARAMETER;
		goto lblCleanup;
	}

	eStatus = QRPATCH_SetBitmap(pvInputBuffer, cbInputBuffer);
	if (!NT_SUCCESS(eStatus))
	{
		goto lblCleanup;
	}

	eStatus = STATUS_SUCCESS;

lblCleanup:
	return eStatus;
}

/**
 * Handler for IRP_MJ_DEVICE_CONTROL requests.
 *
 * @param[in]	ptDeviceObject	The device object.
 * @param[in]	ptIrp			The IRP.
 *
 * @returns NTSTATUS
 */
_IRQL_requires_max_(DISPATCH_LEVEL)
STATIC
NTSTATUS
driver_DispatchDeviceControl(
	_In_	PDEVICE_OBJECT	ptDeviceObject,
	_In_	PIRP			ptIrp
)
{
	NTSTATUS			eStatus			= STATUS_UNSUCCESSFUL;
	PIO_STACK_LOCATION	ptStackLocation	= NULL;
	ULONG				cbWritten		= 0;

#ifndef DBG
	UNREFERENCED_PARAMETER(ptDeviceObject);
#endif // !DBG

	NT_ASSERT(NULL != ptDeviceObject);
	NT_ASSERT(NULL != ptIrp);
	NT_ASSERT(DISPATCH_LEVEL >= KeGetCurrentIrql());

	ptStackLocation = IoGetCurrentIrpStackLocation(ptIrp);
	ASSERT(NULL != ptStackLocation);

	switch (ptStackLocation->Parameters.DeviceIoControl.IoControlCode)
	{
	case IOCTL_DRINK_BUGSHOT:
		eStatus = driver_HandleBugshot();
		break;

	case IOCTL_DRINK_VANITY:
		eStatus = driver_HandleVanity(ptIrp->AssociatedIrp.SystemBuffer,
									  ptStackLocation->Parameters.DeviceIoControl.InputBufferLength);
		ASSERT(!NT_SUCCESS(eStatus));	// The above call returns only on failure.
		break;

	case IOCTL_DRINK_QR_INFO:
		eStatus = driver_HandleQrInfo(ptIrp->AssociatedIrp.SystemBuffer,
									  ptStackLocation->Parameters.DeviceIoControl.OutputBufferLength,
									  &cbWritten);
		break;

	case IOCTL_DRINK_QR_SET:
		eStatus = driver_HandleQrSet(ptIrp->AssociatedIrp.SystemBuffer,
									 ptStackLocation->Parameters.DeviceIoControl.InputBufferLength);
		break;

	default:
		eStatus = STATUS_INVALID_DEVICE_REQUEST;
		break;
	}

	// Keep last status

//lblCleanup:
	COMPLETE_IRP(ptIrp, eStatus, cbWritten, IO_NO_INCREMENT);
	return eStatus;
}

/**
 * The driver's entry-point.
 *
 * @param[in]	ptDriverObject	Pointer to the driver object.
 * @param[in]	pusRegistryPath	Pointer to the driver's registry key.
 *								Unreferenced.
 *
 * @returns NTSTATUS
 */
_IRQL_requires_(PASSIVE_LEVEL)
__declspec(code_seg("INIT"))
NTSTATUS
DriverEntry(
	_In_	PDRIVER_OBJECT	ptDriverObject,
	_In_	PUNICODE_STRING	pusRegistryPath
)
{
	NTSTATUS		eStatus			= STATUS_UNSUCCESSFUL;
	PDEVICE_OBJECT	ptControlDevice	= NULL;
	BOOLEAN			bDeleteSymlink	= FALSE;

	PAGED_CODE();

	UNREFERENCED_PARAMETER(pusRegistryPath);

	ASSERT(NULL != ptDriverObject);
	ASSERT(PASSIVE_LEVEL == KeGetCurrentIrql());

	//
	// Initialize the different runtimes.
	//

	ExInitializeDriverRuntime(DrvRtPoolNxOptIn);

	eStatus = AuxKlibInitialize();
	if (!NT_SUCCESS(eStatus))
	{
		goto lblCleanup;
	}

	if (UTIL_IsWindows10OrGreater())
	{
		eStatus = QRPATCH_Initialize();
		if (!NT_SUCCESS(eStatus))
		{
			goto lblCleanup;
		}
	}

	//
	// Initialize some locks
	//
	KeInitializeSpinLock(&g_tVgaDumpLock);
	KeInitializeMutex(&g_tVanityLock, 0);

	//
	// Initialize the driver object.
	//

	ptDriverObject->DriverUnload = &driver_Unload;
	ptDriverObject->MajorFunction[IRP_MJ_CREATE]			= &driver_DispatchCreateClose;
	ptDriverObject->MajorFunction[IRP_MJ_CLOSE]				= &driver_DispatchCreateClose;
	ptDriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL]	= &driver_DispatchDeviceControl;

	//
	// Create the control device
	//

	eStatus = IoCreateDevice(ptDriverObject,
							 0,
							 (PUNICODE_STRING)&g_usControlDeviceName,
							 DRINK_DEVICE_TYPE,
							 0,
							 FALSE,
							 &ptControlDevice);
	if (!NT_SUCCESS(eStatus))
	{
		goto lblCleanup;
	}

	eStatus = IoCreateSymbolicLink((PUNICODE_STRING)&g_usControlDeviceSymlink,
								   (PUNICODE_STRING)&g_usControlDeviceName);
	if (!NT_SUCCESS(eStatus))
	{
		goto lblCleanup;
	}
	bDeleteSymlink = TRUE;

	SetFlag(ptControlDevice->Flags, DO_BUFFERED_IO);
	ClearFlag(ptControlDevice->Flags, DO_DEVICE_INITIALIZING);

	// Transfer ownership:
	ptControlDevice = NULL;
	bDeleteSymlink = FALSE;

	eStatus = STATUS_SUCCESS;

lblCleanup:
	if (bDeleteSymlink)
	{
		(VOID)IoDeleteSymbolicLink((PUNICODE_STRING)&g_usControlDeviceSymlink);
		bDeleteSymlink = FALSE;
	}
	CLOSE(ptControlDevice, IoDeleteDevice);

	return eStatus;
}
