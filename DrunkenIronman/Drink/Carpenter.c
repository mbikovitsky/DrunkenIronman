/**
 * @file Carpenter.c
 * @author biko
 * @date 2016-08-06
 *
 * Carpenter module implementation.
 */

/** Headers *************************************************************/
#include <ntifs.h>

#include <Common.h>

#include "Util.h"
#include "ImageParse.h"
#include "MessageTable.h"

#include "Carpenter.h"


/** Constants ***********************************************************/

/**
 * Pool tag for allocations made by this module.
 */
#define CARPENTER_POOL_TAG (RtlUlongByteSwap('Carp'))


/** Typedefs ************************************************************/

/**
 * Contains the state of a patcher instance.
 */
typedef struct _CARPENTER
{
	PVOID			pvInImageMessageTable;
	ULONG			cbInImageMessageTable;
	HMESSAGETABLE	hMessageTable;
} CARPENTER, *PCARPENTER;
typedef CONST CARPENTER *PCCARPENTER;


/** Functions ***********************************************************/

/**
 * Converts a pointer to a resource path component.
 *
 * @param[in]	pvPointer		Pointer to convert.
 * @param[out]	ptPathComponent	Will receive the path component.
 */
STATIC
VOID
carpenter_PointerToPathComponent(
	_In_	ULONG_PTR					pvPointer,
	_Out_	PRESOURCE_PATH_COMPONENT	ptPathComponent
)
{
	PCWSTR	pwszName	= NULL;

	// Determine whether we have an ID or a name.
	// Note that this has nothing to do with alignment,
	// as all valid pointers will be greater than MAXUSHORT.
	if (pvPointer & (~(ULONG_PTR)MAXUSHORT))
	{
		ptPathComponent->bNamed = TRUE;

		pwszName = (PCWSTR)pvPointer;
		RtlInitUnicodeString(&(ptPathComponent->tComponent.usName),
							 pwszName);
	}
	else
	{
		ptPathComponent->bNamed = FALSE;
		ptPathComponent->tComponent.nId = (USHORT)pvPointer;
	}
}

_IRQL_requires_(PASSIVE_LEVEL)
PAGEABLE
NTSTATUS
CARPENTER_Create(
	_In_	PVOID		pvImageBase,
	_In_	ULONG_PTR	pvType,
	_In_	ULONG_PTR	pvName,
	_In_	ULONG_PTR	pvLanguage,
	_Out_	PHCARPENTER	phCarpenter
)
{
	NTSTATUS				eStatus		= STATUS_UNSUCCESSFUL;
	PCARPENTER				ptCarpenter	= NULL;
	RESOURCE_PATH_COMPONENT	atPath[3]	= { 0 };

	ASSERT(PASSIVE_LEVEL == KeGetCurrentIrql());

	if ((NULL == pvImageBase) ||
		(NULL == phCarpenter))
	{
		eStatus = STATUS_INVALID_PARAMETER;
		goto lblCleanup;
	}

	ptCarpenter = (PCARPENTER)ExAllocatePoolWithTag(PagedPool,
													sizeof(*ptCarpenter),
													CARPENTER_POOL_TAG);
	if (NULL == ptCarpenter)
	{
		eStatus = STATUS_INSUFFICIENT_RESOURCES;
		goto lblCleanup;
	}
	RtlSecureZeroMemory(ptCarpenter, sizeof(*ptCarpenter));

	carpenter_PointerToPathComponent(pvType, &(atPath[0]));
	carpenter_PointerToPathComponent(pvName, &(atPath[1]));
	carpenter_PointerToPathComponent(pvLanguage, &(atPath[2]));

	eStatus = IMAGEPARSE_FindResource(pvImageBase,
									  atPath,
									  ARRAYSIZE(atPath),
									  &(ptCarpenter->pvInImageMessageTable),
									  &(ptCarpenter->cbInImageMessageTable));
	if (!NT_SUCCESS(eStatus))
	{
		goto lblCleanup;
	}

	eStatus = MESSAGETABLE_CreateFromResource(ptCarpenter->pvInImageMessageTable,
											  ptCarpenter->cbInImageMessageTable,
											  &(ptCarpenter->hMessageTable));
	if (!NT_SUCCESS(eStatus))
	{
		goto lblCleanup;
	}

	// Transfer ownership:
	*phCarpenter = (HCARPENTER)ptCarpenter;
	ptCarpenter = NULL;

	eStatus = STATUS_SUCCESS;

lblCleanup:
#pragma warning(suppress: 4133)	// warning C4133: 'function': incompatible types - from 'PCARPENTER' to 'HCARPENTER'
	CLOSE(ptCarpenter, CARPENTER_Destroy);

	return eStatus;
}

_IRQL_requires_(PASSIVE_LEVEL)
PAGEABLE
VOID
CARPENTER_Destroy(
	_In_	HCARPENTER	hCarpenter
)
{
	PCARPENTER	ptCarpenter	= (PCARPENTER)hCarpenter;

	ASSERT(PASSIVE_LEVEL == KeGetCurrentIrql());

	if (NULL == hCarpenter)
	{
		goto lblCleanup;
	}

	CLOSE(ptCarpenter->hMessageTable, MESSAGETABLE_Destroy);
	CLOSE(ptCarpenter, ExFreePool);

lblCleanup:
	return;
}

_IRQL_requires_(PASSIVE_LEVEL)
PAGEABLE
NTSTATUS
CARPENTER_StageMessage(
	_In_	HCARPENTER		hCarpenter,
	_In_	ULONG			nMessageId,
	_In_	PCANSI_STRING	psMessage
)
{
	NTSTATUS	eStatus		= STATUS_UNSUCCESSFUL;
	PCARPENTER	ptCarpenter	= (PCARPENTER)hCarpenter;

	ASSERT(PASSIVE_LEVEL == KeGetCurrentIrql());

	if ((NULL == hCarpenter) ||
		(NULL == psMessage))
	{
		eStatus = STATUS_INVALID_PARAMETER;
		goto lblCleanup;
	}

	eStatus = MESSAGETABLE_InsertAnsi(ptCarpenter->hMessageTable,
									  nMessageId,
									  psMessage);

	// Keep last status

lblCleanup:
	return eStatus;
}

_IRQL_requires_(PASSIVE_LEVEL)
PAGEABLE
NTSTATUS
CARPENTER_ApplyPatch(
	_In_	HCARPENTER	hCarpenter
)
{
	NTSTATUS	eStatus				= STATUS_UNSUCCESSFUL;
	PCARPENTER	ptCarpenter			= (PCARPENTER)hCarpenter;
	PVOID		pvNewMessageTable	= NULL;
	SIZE_T		cbNewMessageTable	= 0;
	PMDL		ptMdl				= NULL;
	BOOLEAN		bMdlLocked			= FALSE;
	PVOID		pvNewMapping		= NULL;

	ASSERT(PASSIVE_LEVEL == KeGetCurrentIrql());

	if (NULL == hCarpenter)
	{
		goto lblCleanup;
	}

	eStatus = MESSAGETABLE_Serialize(ptCarpenter->hMessageTable,
									 &pvNewMessageTable,
									 &cbNewMessageTable);
	if (!NT_SUCCESS(eStatus))
	{
		goto lblCleanup;
	}

	// Make sure the new table does not exceed in size
	// the old one.
	if (cbNewMessageTable > ptCarpenter->cbInImageMessageTable)
	{
		eStatus = STATUS_BUFFER_TOO_SMALL;
		goto lblCleanup;
	}

	ptMdl = IoAllocateMdl(ptCarpenter->pvInImageMessageTable,
						  ptCarpenter->cbInImageMessageTable,
						  FALSE,
						  FALSE,
						  NULL);
	if (NULL == ptMdl)
	{
		eStatus = STATUS_INSUFFICIENT_RESOURCES;
		goto lblCleanup;
	}

	__try
	{
		MmProbeAndLockPages(ptMdl, KernelMode, IoReadAccess);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		eStatus = GetExceptionCode();
		goto lblCleanup;
	}
	bMdlLocked = TRUE;

	pvNewMapping = MmGetSystemAddressForMdlSafe(ptMdl, NormalPagePriority);
	if (NULL == pvNewMapping)
	{
		eStatus = STATUS_INSUFFICIENT_RESOURCES;
		goto lblCleanup;
	}

	eStatus = MmProtectMdlSystemAddress(ptMdl, PAGE_READWRITE);
	if (!NT_SUCCESS(eStatus))
	{
		goto lblCleanup;
	}

	// Patch the message table
	RtlMoveMemory(pvNewMapping, pvNewMessageTable, cbNewMessageTable);

	// Zero the rest
	RtlSecureZeroMemory(RtlOffsetToPointer(pvNewMapping, cbNewMessageTable),
						ptCarpenter->cbInImageMessageTable - cbNewMessageTable);

	eStatus = STATUS_SUCCESS;

lblCleanup:
	if (bMdlLocked)
	{
		MmUnlockPages(ptMdl);
		bMdlLocked = FALSE;
	}
	CLOSE(ptMdl, IoFreeMdl);
	CLOSE(pvNewMessageTable, ExFreePool);

	return eStatus;
}
