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

/**
 * Clears a message table entry.
 *
 * @param[in]	ptEntry	Entry to clear.
 *
 * @remark	This does not actually free the entry
 *			itself, only the pointers within.
 */
STATIC
VOID
messagetable_ClearEntry(
	_In_	PMESSAGE_TABLE_ENTRY	ptEntry
)
{
	PAGED_CODE();

	if (NULL == ptEntry)
	{
		goto lblCleanup;
	}

	if (ptEntry->bUnicode)
	{
		CLOSE(ptEntry->tData.tUnicode.Buffer, ExFreePool);
	}
	else
	{
		CLOSE(ptEntry->tData.tAnsi.Buffer, ExFreePool);
	}

	//
	// NOTE: DO NOT ZERO THE STRUCTURE!
	// This function is called before element removal,
	// and so the rest of the structure's members
	// are used by the comparison routine
	// to find the entry to remove.
	//

lblCleanup:
	return;
}

/**
 * Determines whether an ANSI_STRING structure
 * is valid for use in a message table.
 *
 * @param[in]	psString	String to check.
 *
 * @returns BOOLEAN
 */
STATIC
BOOLEAN
messagetable_IsValidAnsiString(
	_In_opt_	PCANSI_STRING	psString
)
{
	BOOLEAN	bIsValid	= FALSE;

	if (NULL == psString)
	{
		goto lblCleanup;
	}

	if (NULL == psString->Buffer)
	{
		goto lblCleanup;
	}

	if ((0 == psString->Length) ||
		(0 == psString->MaximumLength))
	{
		goto lblCleanup;
	}

	if (psString->MaximumLength < psString->Length)
	{
		goto lblCleanup;
	}

	bIsValid = TRUE;

lblCleanup:
	return bIsValid;
}

/**
 * Determines whether a UNICODE_STRING structure
 * is valid for use in a message table.
 *
 * @param[in]	pusString	String to check.
 *
 * @returns BOOLEAN
 */
STATIC
BOOLEAN
messagetable_IsValidUnicodeString(
	_In_opt_	PCUNICODE_STRING	pusString
)
{
	BOOLEAN	bIsValid	= FALSE;

	if (NULL == pusString)
	{
		goto lblCleanup;
	}

	if (NULL == pusString->Buffer)
	{
		goto lblCleanup;
	}

	if ((0 == pusString->Length) ||
		(0 == pusString->MaximumLength))
	{
		goto lblCleanup;
	}

	if (pusString->MaximumLength < pusString->Length)
	{
		goto lblCleanup;
	}

	if ((0 != pusString->Length % 2) ||
		(0 != pusString->MaximumLength % 2))
	{
		goto lblCleanup;
	}

	bIsValid = TRUE;

lblCleanup:
	return bIsValid;
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
	PVOID					pvData		= NULL;

	PAGED_CODE();

	if (NULL == hMessageTable)
	{
		goto lblCleanup;
	}

	for (pvData = RtlEnumerateGenericTableAvl(&(ptContext->tTable), TRUE);
		 NULL != pvData;
		 pvData = RtlEnumerateGenericTableAvl(&(ptContext->tTable), FALSE))
	{
		// Clear the current entry ...
		messagetable_ClearEntry((PMESSAGE_TABLE_ENTRY)pvData);

		// ... and delete it from the tree.
		(VOID)RtlDeleteElementGenericTableAvl(&(ptContext->tTable), pvData);
	}

	ASSERT(RtlIsGenericTableEmptyAvl(&(ptContext->tTable)));

	CLOSE(ptContext, ExFreePool);

lblCleanup:
	return;
}

NTSTATUS
MESSAGETABLE_InsertAnsi(
	_In_	HMESSAGETABLE	hMessageTable,
	_In_	ULONG			nEntryId,
	_In_	PCANSI_STRING	psString
)
{
	NTSTATUS				eStatus				= STATUS_UNSUCCESSFUL;
	PMESSAGE_TABLE_CONTEXT	ptContext			= (PMESSAGE_TABLE_CONTEXT)hMessageTable;
	MESSAGE_TABLE_ENTRY		tEntry				= { 0 };
	PCHAR					pcDuplicateString	= NULL;
	BOOLEAN					bNewElement			= FALSE;
	PMESSAGE_TABLE_ENTRY	ptInserted			= NULL;

	PAGED_CODE();

	if ((NULL == hMessageTable) ||
		(!messagetable_IsValidAnsiString(psString)))
	{
		eStatus = STATUS_INVALID_PARAMETER;
		goto lblCleanup;
	}

	// Allocate memory for a duplicate string.
	pcDuplicateString = ExAllocatePoolWithTag(PagedPool,
											  psString->Length,
											  MESSAGE_TABLE_POOL_TAG);
	if (NULL == pcDuplicateString)
	{
		eStatus = STATUS_INSUFFICIENT_RESOURCES;
		goto lblCleanup;
	}

	// Initialize a new entry.
	RtlMoveMemory(pcDuplicateString,
				  psString->Buffer,
				  psString->Length);
	tEntry.nEntryId = nEntryId;
	tEntry.bUnicode = FALSE;
	tEntry.tData.tAnsi.Buffer = pcDuplicateString;
	tEntry.tData.tAnsi.Length = psString->Length;
	tEntry.tData.tAnsi.MaximumLength = tEntry.tData.tAnsi.Length;

	// Try to insert it into the tree.
	ptInserted =
		(PMESSAGE_TABLE_ENTRY)RtlInsertElementGenericTableAvl(&(ptContext->tTable),
															  &tEntry,
															  sizeof(tEntry),
															  &bNewElement);
	if (NULL == ptInserted)
	{
		eStatus = STATUS_INSUFFICIENT_RESOURCES;
		goto lblCleanup;
	}

	// If an element with the same ID already exists,
	// overwrite it.
	if (!bNewElement)
	{
		ASSERT(ptInserted->nEntryId == tEntry.nEntryId);

		messagetable_ClearEntry(ptInserted);
		RtlMoveMemory(ptInserted, &tEntry, sizeof(*ptInserted));
	}

	// Transfer ownership:
	pcDuplicateString = NULL;

	eStatus = STATUS_SUCCESS;

lblCleanup:
	CLOSE(pcDuplicateString, ExFreePool);

	return eStatus;
}

NTSTATUS
MESSAGETABLE_InsertUnicode(
	_In_	HMESSAGETABLE		hMessageTable,
	_In_	ULONG				nEntryId,
	_In_	PCUNICODE_STRING	pusString
)
{
	NTSTATUS				eStatus				= STATUS_UNSUCCESSFUL;
	PMESSAGE_TABLE_CONTEXT	ptContext			= (PMESSAGE_TABLE_CONTEXT)hMessageTable;
	MESSAGE_TABLE_ENTRY		tEntry				= { 0 };
	PWCHAR					pwcDuplicateString	= NULL;
	BOOLEAN					bNewElement			= FALSE;
	PMESSAGE_TABLE_ENTRY	ptInserted			= NULL;

	PAGED_CODE();

	if ((NULL == hMessageTable) ||
		(!messagetable_IsValidUnicodeString(pusString)))
	{
		eStatus = STATUS_INVALID_PARAMETER;
		goto lblCleanup;
	}

	// Allocate memory for a duplicate string.
	pwcDuplicateString = ExAllocatePoolWithTag(PagedPool,
											   pusString->Length,
											   MESSAGE_TABLE_POOL_TAG);
	if (NULL == pwcDuplicateString)
	{
		eStatus = STATUS_INSUFFICIENT_RESOURCES;
		goto lblCleanup;
	}

	// Initialize a new entry.
	RtlMoveMemory(pwcDuplicateString,
				  pusString->Buffer,
				  pusString->Length);
	tEntry.nEntryId = nEntryId;
	tEntry.bUnicode = TRUE;
	tEntry.tData.tUnicode.Buffer = pwcDuplicateString;
	tEntry.tData.tUnicode.Length = pusString->Length;
	tEntry.tData.tUnicode.MaximumLength = tEntry.tData.tUnicode.Length;

	// Try to insert it into the tree.
	ptInserted =
		(PMESSAGE_TABLE_ENTRY)RtlInsertElementGenericTableAvl(&(ptContext->tTable),
															  &tEntry,
															  sizeof(tEntry),
															  &bNewElement);
	if (NULL == ptInserted)
	{
		eStatus = STATUS_INSUFFICIENT_RESOURCES;
		goto lblCleanup;
	}

	// If an element with the same ID already exists,
	// overwrite it.
	if (!bNewElement)
	{
		ASSERT(ptInserted->nEntryId == tEntry.nEntryId);

		messagetable_ClearEntry(ptInserted);
		RtlMoveMemory(ptInserted, &tEntry, sizeof(*ptInserted));
	}

	// Transfer ownership:
	pwcDuplicateString = NULL;

	eStatus = STATUS_SUCCESS;

lblCleanup:
	CLOSE(pwcDuplicateString, ExFreePool);

	return eStatus;
}
