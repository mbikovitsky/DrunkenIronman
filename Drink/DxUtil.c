/**
 * @file DxUtil.c
 * @author biko
 * @date 2021-08-31
 *
 * Various utilities for manipulating Dxgkrnl - implementation.
 */

/** Headers *************************************************************/
#include <ntifs.h>
#include <dispmprt.h>
#include <ntintsafe.h>

#include <Common.h>

#include "Util.h"

#include "DxUtil.h"


/** Constants ***********************************************************/

#define DXUTIL_POOL_TAG ('tUxD')

#ifdef _M_X64
#define DRIVER_INITIALIZATION_DATA_OFFSET (0x88)
#elif _M_IX86
#define DRIVER_INITIALIZATION_DATA_OFFSET (0x54)
#else
#error Unsupported architecture
#endif


/** Typedefs ************************************************************/

typedef struct _DRIVER_INFO
{
	HANDLE			hDriver;
	PDRIVER_OBJECT	ptDriver;
	PVOID			pvExtension;
	BOOLEAN			bDisplayDriver;
} DRIVER_INFO, *PDRIVER_INFO;
typedef DRIVER_INFO CONST *PCDRIVER_INFO;


/** Forward Declarations ************************************************/

_IRQL_requires_(PASSIVE_LEVEL)
NTSTATUS
NTSYSAPI
NTAPI
ObOpenObjectByName (
    _In_		POBJECT_ATTRIBUTES	ObjectAttributes,
    _In_		POBJECT_TYPE		ObjectType,
    _In_		KPROCESSOR_MODE		AccessMode,
    _Inout_opt_	PACCESS_STATE		AccessState,
    _In_		ACCESS_MASK			DesiredAccess,
    _Inout_opt_	PVOID				ParseContext,
    _Out_		PHANDLE				Handle
);

EXTERN_C POBJECT_TYPE * IoDriverObjectType;


/** Functions ***********************************************************/

/**
 * @brief Finds the base address of dxgkrnl.sys.
 *
 * @param ppvImageBase	Will receive the image base.
 * @param pcbImageSize	Will receive the image size.
 *
 * @return NTSTATUS
*/

_IRQL_requires_(PASSIVE_LEVEL)
STATIC
PAGEABLE
NTSTATUS
dxutil_FindDxgkrnl(
	_Out_		PVOID *	ppvImageBase,
	_Out_opt_	PULONG	pcbImageSize
)
{
	NTSTATUS					eStatus		= STATUS_UNSUCCESSFUL;
	PAUX_MODULE_EXTENDED_INFO	ptModules	= NULL;
	ULONG						nModules	= 0;
	ULONG						nIndex		= 0;
	ANSI_STRING					sFileName	= { 0 };
	ANSI_STRING					sDxgkrnl	= RTL_CONSTANT_STRING("dxgkrnl.sys");
	PVOID						pvImageBase	= NULL;
	ULONG						cbImageSize	= 0;

	PAGED_CODE();
	NT_ASSERT(PASSIVE_LEVEL == KeGetCurrentIrql());

	NT_ASSERT(NULL != ppvImageBase);

	eStatus = UTIL_QueryModuleInformation(&ptModules, &nModules);
	if (!NT_SUCCESS(eStatus))
	{
		goto lblCleanup;
	}

	for (nIndex = 0; nIndex < nModules; ++nIndex)
	{
		eStatus = UTIL_InitAnsiStringCb((PCHAR)&ptModules[nIndex].FullPathName[ptModules[nIndex].FileNameOffset],
										sizeof(ptModules[nIndex].FullPathName) - ptModules[nIndex].FileNameOffset,
										&sFileName);
		if (!NT_SUCCESS(eStatus))
		{
			// Strange, but let's carry on
			continue;
		}

		if (!RtlEqualString(&sFileName, &sDxgkrnl, TRUE))
		{
			continue;
		}

		pvImageBase = ptModules[nIndex].BasicInfo.ImageBase;
		cbImageSize = ptModules[nIndex].ImageSize;
		break;
	}
	if (NULL == pvImageBase)
	{
		eStatus = STATUS_NOT_FOUND;
		goto lblCleanup;
	}

	*ppvImageBase = pvImageBase;
	if (NULL != pcbImageSize)
	{
		*pcbImageSize = cbImageSize;
	}

	eStatus = STATUS_SUCCESS;

lblCleanup:
	CLOSE(ptModules, ExFreePool);

	return eStatus;
}

/**
 * @brief Frees the buffer returned from dxutil_ProcessAllDrivers.
 *
 * @param ptDriverInfos	The buffer to free.
 * @param nDriverInfos	The number of elements in the buffer.
*/
_IRQL_requires_(PASSIVE_LEVEL)
STATIC
PAGEABLE
VOID
dxutil_FreeDriverInfos(
	PDRIVER_INFO	ptDriverInfos,
	ULONG			nDriverInfos
)
{
	ULONG	nIndex	= 0;

	PAGED_CODE();
	NT_ASSERT(PASSIVE_LEVEL == KeGetCurrentIrql());

	if (NULL == ptDriverInfos)
	{
		goto lblCleanup;
	}

	for (nIndex = 0; nIndex < nDriverInfos; ++nIndex)
	{
		CLOSE(ptDriverInfos[nIndex].ptDriver, ObfDereferenceObject);
		CLOSE(ptDriverInfos[nIndex].hDriver, ZwClose);
	}
	CLOSE(ptDriverInfos, ExFreePool);

lblCleanup:
	return;
}

/**
 * @brief Retrieves information about all loaded drivers in the system.
 *
 * @param pptDriverInfos	Will receive the result buffer.
 * @param pnDriverInfos		Will receive the number of elements in the buffer.
 * @param pnDisplayDrivers	Will receive the number of elements in the buffer that define
							display drivers.
 *
 * @return NTSTATUS
 *
 * @remark The returned buffer is pageable.
 * @remark Free the returned buffer with dxutil_FreeDriverInfos.
*/
_IRQL_requires_(PASSIVE_LEVEL)
STATIC
PAGEABLE
NTSTATUS
dxutil_ProcessAllDrivers(
	_Outptr_result_buffer_(*pnDriverInfos)	PDRIVER_INFO *	pptDriverInfos,
	_Out_										PULONG			pnDriverInfos,
	_Out_										PULONG			pnDisplayDrivers
)
{
	NTSTATUS						eStatus				= STATUS_UNSUCCESSFUL;
	PVOID							pvDxgkrnl			= NULL;
	ULONG							cbDxgkrnl			= 0;
	UNICODE_STRING					usDriverDir			= RTL_CONSTANT_STRING(L"\\Driver");
	OBJECT_ATTRIBUTES				tObjectAttributes	= RTL_INIT_OBJECT_ATTRIBUTES(&usDriverDir, OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE);
	HANDLE							hDriverDir			= NULL;
	POBJECT_DIRECTORY_INFORMATION	ptObjectInfos		= NULL;
	POBJECT_DIRECTORY_INFORMATION	ptCurrentInfo		= NULL;
	ULONG							nDrivers			= 0;
	SIZE_T							cbDriverInfos		= 0;
	PDRIVER_INFO					ptDriverInfos		= NULL;
	ULONG							nIndex				= 0;
	ULONG							nDisplayDrivers		= 0;

	PAGED_CODE();
	NT_ASSERT(PASSIVE_LEVEL == KeGetCurrentIrql());

	NT_ASSERT(NULL != pptDriverInfos);
	NT_ASSERT(NULL != pnDriverInfos);
	NT_ASSERT(NULL != pnDisplayDrivers);

	eStatus = dxutil_FindDxgkrnl(&pvDxgkrnl, &cbDxgkrnl);
	if (!NT_SUCCESS(eStatus))
	{
		goto lblCleanup;
	}

	eStatus = ZwOpenDirectoryObject(&hDriverDir, DIRECTORY_QUERY | DIRECTORY_TRAVERSE, &tObjectAttributes);
	if (!NT_SUCCESS(eStatus))
	{
		goto lblCleanup;
	}

	eStatus = UTIL_GetObjectDirectoryContents(hDriverDir, &ptObjectInfos);
	if (!NT_SUCCESS(eStatus))
	{
		goto lblCleanup;
	}

	nDrivers = 0;
	for (ptCurrentInfo = ptObjectInfos; NULL != ptCurrentInfo->Name.Buffer; ++ptCurrentInfo)
	{
		eStatus = RtlULongAdd(nDrivers, 1, &nDrivers);
		if (!NT_SUCCESS(eStatus))
		{
			goto lblCleanup;
		}
	}

	eStatus = RtlSIZETMult(nDrivers, sizeof(ptDriverInfos[0]), &cbDriverInfos);
	if (!NT_SUCCESS(eStatus))
	{
		goto lblCleanup;
	}

	ptDriverInfos = ExAllocatePoolWithTag(PagedPool, cbDriverInfos, DXUTIL_POOL_TAG);
	if (NULL == ptDriverInfos)
	{
		eStatus = STATUS_INSUFFICIENT_RESOURCES;
		goto lblCleanup;
	}
	RtlZeroMemory(ptDriverInfos, cbDriverInfos);

	nDisplayDrivers = 0;
	for (nIndex = 0; nIndex < nDrivers; ++nIndex)
	{
		InitializeObjectAttributes(&tObjectAttributes,
								   &ptObjectInfos[nIndex].Name,
								   OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE,
								   hDriverDir,
								   NULL);
		eStatus = ObOpenObjectByName(&tObjectAttributes,
									 *IoDriverObjectType,
									 KernelMode,
									 NULL,
									 0,
									 NULL,
									 &ptDriverInfos[nIndex].hDriver);
		if (!NT_SUCCESS(eStatus))
		{
			continue;
		}

		eStatus = ObReferenceObjectByHandle(ptDriverInfos[nIndex].hDriver,
											0,
											NULL,
											KernelMode,
											(PVOID *)&ptDriverInfos[nIndex].ptDriver,
											NULL);
		if (!NT_SUCCESS(eStatus))
		{
			goto lblCleanup;
		}

		// Dxgkrnl uses the driver object address as the ID
		ptDriverInfos[nIndex].pvExtension = IoGetDriverObjectExtension(ptDriverInfos[nIndex].ptDriver, ptDriverInfos[nIndex].ptDriver);
		if (NULL == ptDriverInfos[nIndex].pvExtension)
		{
			continue;
		}

		// Unfortunately, so do other drivers. So we need to filter them out.

#pragma warning(push)
#pragma warning(disable: 28175)		// The 'DriverUnload' member of _DRIVER_OBJECT should not be accessed by a driver
		// Unload routine should be supplied by Dxgkrnl
		if ((PUCHAR)(ptDriverInfos[nIndex].ptDriver->DriverUnload) < (PUCHAR)pvDxgkrnl)
		{
			continue;
		}
		if ((PCHAR)(ptDriverInfos[nIndex].ptDriver->DriverUnload) >= RtlOffsetToPointer(pvDxgkrnl, cbDxgkrnl))
		{
			continue;
		}
#pragma warning(pop)

		ptDriverInfos[nIndex].bDisplayDriver = TRUE;
		++nDisplayDrivers;
	}

	// Transfer ownership:
	*pptDriverInfos = ptDriverInfos;
	ptDriverInfos = NULL;
	*pnDriverInfos = nDrivers;
	*pnDisplayDrivers = nDisplayDrivers;

	eStatus = STATUS_SUCCESS;

lblCleanup:
	CLOSE_TO_VALUE_VARIADIC(ptDriverInfos, dxutil_FreeDriverInfos, NULL, nDrivers);
	CLOSE(ptObjectInfos, ExFreePool);
	CLOSE(hDriverDir, ZwClose);

	return eStatus;
}

_Use_decl_annotations_
PAGEABLE
NTSTATUS
DXUTIL_FindAllDisplayDrivers(
	PDISPLAY_DRIVER *	pptDrivers,
	PULONG				pnDrivers
)
{
	NTSTATUS		eStatus				= STATUS_UNSUCCESSFUL;
	ULONG			nIndex				= 0;
	PDRIVER_INFO	ptDriverInfos		= NULL;
	ULONG			nDriverInfos		= 0;
	ULONG			nDisplayDrivers		= 0;
	PDISPLAY_DRIVER	ptDrivers			= NULL;
	ULONG			nOutputIndex		= 0;

	PAGED_CODE();
	NT_ASSERT(PASSIVE_LEVEL == KeGetCurrentIrql());

	if (NULL == pptDrivers || NULL == pnDrivers)
	{
		eStatus = STATUS_INVALID_PARAMETER;
		goto lblCleanup;
	}

	eStatus = dxutil_ProcessAllDrivers(&ptDriverInfos, &nDriverInfos, &nDisplayDrivers);
	if (!NT_SUCCESS(eStatus))
	{
		goto lblCleanup;
	}
	NT_ASSERT(nDisplayDrivers <= nDriverInfos);

	ptDrivers = ExAllocatePoolWithTag(PagedPool, nDisplayDrivers * sizeof(ptDrivers[0]), DXUTIL_POOL_TAG);
	if (NULL == ptDrivers)
	{
		eStatus = STATUS_INSUFFICIENT_RESOURCES;
		goto lblCleanup;
	}

	// Transfer ownership:

	nOutputIndex = 0;
	for (nIndex = 0; nIndex < nDriverInfos; ++nIndex)
	{
		if (!ptDriverInfos[nIndex].bDisplayDriver)
		{
			continue;
		}

		NT_ASSERT(nOutputIndex < nDisplayDrivers);

		ptDrivers[nOutputIndex].ptInitializationData = (PDRIVER_INITIALIZATION_DATA)RtlOffsetToPointer(ptDriverInfos[nIndex].pvExtension, DRIVER_INITIALIZATION_DATA_OFFSET);
		ptDrivers[nOutputIndex++].ptDriverObject = ptDriverInfos[nIndex].ptDriver;
		ptDriverInfos[nIndex].ptDriver = NULL;
	}
	NT_ASSERT(nOutputIndex == nDisplayDrivers);

	*pptDrivers = ptDrivers;
	ptDrivers = NULL;
	*pnDrivers = nDisplayDrivers;

	eStatus = STATUS_SUCCESS;

lblCleanup:
	CLOSE(ptDrivers, ExFreePool);
	CLOSE_TO_VALUE_VARIADIC(ptDriverInfos, dxutil_FreeDriverInfos, NULL, nDriverInfos);

	return eStatus;
}

_Use_decl_annotations_
PAGEABLE
VOID
DXUTIL_FreeDisplayDrivers(
	PDISPLAY_DRIVER	ptDrivers,
	ULONG			nDrivers
)
{
	ULONG	nIndex	= 0;

	PAGED_CODE();
	NT_ASSERT(PASSIVE_LEVEL == KeGetCurrentIrql());

	if (NULL == ptDrivers)
	{
		goto lblCleanup;
	}

	for (nIndex = 0; nIndex < nDrivers; ++nIndex)
	{
		CLOSE(ptDrivers[nIndex].ptDriverObject, ObfDereferenceObject);
	}
	CLOSE(ptDrivers, ExFreePool);

lblCleanup:
	return;
}
