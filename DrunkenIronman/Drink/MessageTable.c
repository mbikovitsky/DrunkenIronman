/**
 * @file MessageTable.c
 * @author biko
 * @date 2016-07-30
 *
 * MessageTable module implementation.
 */

/** Headers *************************************************************/
#include <ntifs.h>

#include <Common.h>

#include "MessageTable.h"


/** Constants ***********************************************************/

/**
 * Pool tag for allocations made by this module.
 */
#define MESSAGE_TABLE_POOL_TAG (RtlUlongByteSwap('MsgT'))


/** Typedefs ************************************************************/

/**
 * Contains the state of the message table.
 */
typedef struct _MESSAGE_TABLE_CONTEXT
{
	RTL_AVL_TABLE	tTable;
} MESSAGE_TABLE_CONTEXT, *PMESSAGE_TABLE_CONTEXT;
typedef CONST MESSAGE_TABLE_CONTEXT *PCMESSAGE_TABLE_CONTEXT;

/**
 * Structure of a single message table entry.
 */
typedef struct _MESSAGE_TABLE_ENTRY
{
	ULONG	nEntryId;
	BOOLEAN	bUnicode;
	union
	{
		ANSI_STRING		tAnsi;
		UNICODE_STRING	tUnicode;
	} tData;
} MESSAGE_TABLE_ENTRY, *PMESSAGE_TABLE_ENTRY;
typedef CONST MESSAGE_TABLE_ENTRY *PCMESSAGE_TABLE_ENTRY;


/** Functions ***********************************************************/

/**
 * Comparison routine for the AVL tree.
 *
 * @param[in]	ptTable			The tree structure.
 * @param[in]	pvFirstStruct	First structure to compare.
 * @param[in]	pvSecondStruct	Second structure to compare.
 *
 * @returns RTL_GENERIC_COMPARE_RESULTS
 */
STATIC
RTL_GENERIC_COMPARE_RESULTS
messagetable_CompareRoutine(
	_In_	PRTL_AVL_TABLE	ptTable,
	_In_	PVOID			pvFirstStruct,
	_In_	PVOID			pvSecondStruct
)
{
	RTL_GENERIC_COMPARE_RESULTS	eResult			= GenericLessThan;
	PCMESSAGE_TABLE_ENTRY		ptFirstEntry	= (PCMESSAGE_TABLE_ENTRY)pvFirstStruct;
	PCMESSAGE_TABLE_ENTRY		ptSecondEntry	= (PCMESSAGE_TABLE_ENTRY)pvSecondStruct;

	PAGED_CODE();

	ASSERT(NULL != ptTable);
	ASSERT(NULL != pvFirstStruct);
	ASSERT(NULL != pvSecondStruct);

	if (ptFirstEntry->nEntryId < ptSecondEntry->nEntryId)
	{
		eResult = GenericLessThan;
	}
	else if (ptFirstEntry->nEntryId > ptSecondEntry->nEntryId)
	{
		eResult = GenericGreaterThan;
	}
	else
	{
		eResult = GenericEqual;
	}

	return eResult;
}

/**
 * Memory allocation routine for the AVL tree.
 *
 * @param[in]	ptTable	The tree structure.
 * @param[in]	cbSize	Size to allocate.
 *
 * @returns PVOID
 */
STATIC
PVOID
messagetable_AllocateRoutine(
	_In_	PRTL_AVL_TABLE	ptTable,
	_In_	CLONG			cbSize
)
{
	PAGED_CODE();

	ASSERT(NULL != ptTable);
	ASSERT(0 != cbSize);

	return ExAllocatePoolWithTag(PagedPool,
								 cbSize,
								 MESSAGE_TABLE_POOL_TAG);
}

/**
 * Memory deallocation routine for the AVL tree.
 *
 * @param[in]	ptTable		The tree structure.
 * @param[in]	pvBuffer	The buffer to free.
 */
STATIC
VOID
messagetable_FreeRoutine(
	_In_	PRTL_AVL_TABLE	ptTable,
	_In_	PVOID			pvBuffer
)
{
	PAGED_CODE();

	ASSERT(NULL != ptTable);
	ASSERT(NULL != pvBuffer);

	ExFreePool(pvBuffer);
}

NTSTATUS
MESSAGETABLE_Create(
	_Out_	PHMESSAGETABLE	phMessageTable
)
{
	NTSTATUS				eStatus		= STATUS_UNSUCCESSFUL;
	PMESSAGE_TABLE_CONTEXT	ptContext	= NULL;

	PAGED_CODE();

	if (NULL == phMessageTable)
	{
		eStatus = STATUS_INVALID_PARAMETER;
		goto lblCleanup;
	}

	ptContext = ExAllocatePoolWithTag(PagedPool,
									  sizeof(*ptContext),
									  MESSAGE_TABLE_POOL_TAG);
	if (NULL == ptContext)
	{
		eStatus = STATUS_INSUFFICIENT_RESOURCES;
		goto lblCleanup;
	}

	RtlInitializeGenericTableAvl(&(ptContext->tTable),
								 &messagetable_CompareRoutine,
								 &messagetable_AllocateRoutine,
								 &messagetable_FreeRoutine,
								 NULL);

	// Transfer ownership:
	*phMessageTable = (HMESSAGETABLE)ptContext;
	ptContext = NULL;

	eStatus = STATUS_SUCCESS;

lblCleanup:
	CLOSE(ptContext, ExFreePool);

	return eStatus;
}

VOID
MESSAGETABLE_Destroy(
	_In_	HMESSAGETABLE	hMessageTable
)
{
	PMESSAGE_TABLE_CONTEXT	ptContext	= (PMESSAGE_TABLE_CONTEXT)hMessageTable;

	PAGED_CODE();

	if (NULL == hMessageTable)
	{
		goto lblCleanup;
	}

	CLOSE(ptContext, ExFreePool);

lblCleanup:
	return;
}
