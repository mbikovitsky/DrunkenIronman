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

#include "VgaDump.h"


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
 * Indicates whether VGA dumping has been initialized.
 */
STATIC BOOLEAN g_bVgaDumpInitialized = FALSE;


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

lblCleanup:
	return;
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

#ifndef DBG
	UNREFERENCED_PARAMETER(ptDeviceObject);
#endif // !DBG

	ASSERT(NULL != ptDeviceObject);
	ASSERT(NULL != ptIrp);
	ASSERT(DISPATCH_LEVEL >= KeGetCurrentIrql());

	ptStackLocation = IoGetCurrentIrpStackLocation(ptIrp);
	ASSERT(NULL != ptStackLocation);

	switch (ptStackLocation->Parameters.DeviceIoControl.IoControlCode)
	{
	case IOCTL_DRINK_BUGSHOT:
		eStatus = VGADUMP_Initialize();
		if (!NT_SUCCESS(eStatus))
		{
			goto lblCleanup;
		}
		g_bVgaDumpInitialized = TRUE;

		break;

	default:
		eStatus = STATUS_INVALID_DEVICE_REQUEST;
		break;
	}

	// Keep last status

lblCleanup:
	COMPLETE_IRP(ptIrp, eStatus, 0, IO_NO_INCREMENT);
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
