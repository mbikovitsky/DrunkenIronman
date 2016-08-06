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
	PVOID			pvResourceDirectory;
	ULONG			cbResourceDirectory;
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

	PAGED_CODE();

	if ((NULL == pvImageBase) ||
		(NULL == phCarpenter) ||
		(PASSIVE_LEVEL != KeGetCurrentIrql()))
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
									  &(ptCarpenter->pvResourceDirectory),
									  &(ptCarpenter->cbResourceDirectory));
	if (!NT_SUCCESS(eStatus))
	{
		goto lblCleanup;
	}

	eStatus = MESSAGETABLE_CreateFromResource(ptCarpenter->pvResourceDirectory,
											  ptCarpenter->cbResourceDirectory,
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
VOID
CARPENTER_Destroy(
	_In_	HCARPENTER	hCarpenter
)
{
	PCARPENTER	ptCarpenter	= (PCARPENTER)hCarpenter;

	PAGED_CODE();

	if ((NULL == hCarpenter) ||
		(PASSIVE_LEVEL != KeGetCurrentIrql()))
	{
		goto lblCleanup;
	}

	CLOSE(ptCarpenter->hMessageTable, MESSAGETABLE_Destroy);
	CLOSE(ptCarpenter, ExFreePool);

lblCleanup:
	return;
}
