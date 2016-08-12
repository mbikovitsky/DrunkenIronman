/**
 * @file MessageTable.c
 * @author biko
 * @date 2016-07-30
 *
 * MessageTable module implementation.
 */

/** Headers *************************************************************/
#include <ntifs.h>
#include <ntstrsafe.h>
#include <ntintsafe.h>

#include <Common.h>

#include "Util.h"

#include "MessageTable.h"


/** Typedefs ************************************************************/

/**
 * Contains the state of the message table.
 */
typedef struct _MESSAGE_TABLE
{
	ERESOURCE		tLock;
	BOOLEAN			bLockInitialized;

	_Guarded_by_(tLock)
	RTL_AVL_TABLE	tTable;
	BOOLEAN			bTableInitialized;
} MESSAGE_TABLE, *PMESSAGE_TABLE;
typedef CONST MESSAGE_TABLE *PCMESSAGE_TABLE;

/**
 * Contains the error message or message box display text
 * for a message table resource.
 */
typedef struct _MESSAGE_RESOURCE_ENTRY
{
	USHORT	cbLength;
	USHORT	fFlags;
	UCHAR	acText[ANYSIZE_ARRAY];
} MESSAGE_RESOURCE_ENTRY, *PMESSAGE_RESOURCE_ENTRY;
typedef CONST MESSAGE_RESOURCE_ENTRY *PCMESSAGE_RESOURCE_ENTRY;
C_ASSERT(0 == UFIELD_OFFSET(MESSAGE_RESOURCE_ENTRY, acText) % __alignof(WCHAR));

/**
 * Contains information about message strings with identifiers
 * in the range indicated by the nLowId and nHighId members.
 */
typedef struct _MESSAGE_RESOURCE_BLOCK
{
	ULONG	nLowId;
	ULONG	nHighId;
	ULONG	cbOffsetToEntries;
} MESSAGE_RESOURCE_BLOCK, *PMESSAGE_RESOURCE_BLOCK;
typedef CONST MESSAGE_RESOURCE_BLOCK *PCMESSAGE_RESOURCE_BLOCK;

/**
 * Contains information about formatted text for display as an
 * error message or in a message box in a message table resource.
 */
typedef struct _MESSAGE_RESOURCE_DATA
{
	ULONG					nBlocks;
	MESSAGE_RESOURCE_BLOCK	atBlocks[ANYSIZE_ARRAY];
} MESSAGE_RESOURCE_DATA, *PMESSAGE_RESOURCE_DATA;
typedef CONST MESSAGE_RESOURCE_DATA *PCMESSAGE_RESOURCE_DATA;
C_ASSERT(__alignof(MESSAGE_RESOURCE_DATA) >= __alignof(MESSAGE_RESOURCE_ENTRY));

/**
 * Context for the counting callback.
 *
 * @see messagetable_CountingCallback.
 */
typedef struct _COUNTING_CALLBACK_CONTEXT
{
	// Total blocks required to serialize the table.
	ULONG	nBlocks;

	// Total bytes required to serialize the strings in the table.
	SIZE_T	cbTotalStrings;
} COUNTING_CALLBACK_CONTEXT, *PCOUNTING_CALLBACK_CONTEXT;
typedef CONST COUNTING_CALLBACK_CONTEXT *PCCOUNTING_CALLBACK_CONTEXT;

/**
 * Context for the serializing callback.
 *
 * @see messagetable_SerializingCallback.
 */
typedef struct _SERIALIZING_CALLBACK_CONTEXT
{
	PMESSAGE_RESOURCE_DATA	ptMessageData;

	ULONG					nCurrentBlock;

	PUCHAR					pcCurrentStringPosition;
} SERIALIZING_CALLBACK_CONTEXT, *PSERIALIZING_CALLBACK_CONTEXT;
typedef CONST SERIALIZING_CALLBACK_CONTEXT *PCSERIALIZING_CALLBACK_CONTEXT;


/** Constants ***********************************************************/

/**
 * Pool tag for allocations made by this module.
 */
#define MESSAGE_TABLE_POOL_TAG (RtlUlongByteSwap('MsgT'))

/**
 * Maximum size, in bytes, of a MESSAGE_RESOURCE_ENTRY,
 * including the string data.
 *
 * @remark	This definition assumes that the size field in
 *			the MESSAGE_RESOURCE_ENTRY structure is a USHORT
 *			or larger.
 */
#define MESSAGE_RESOURCE_ENTRY_MAX_SIZE \
	(ALIGN_DOWN_BY(MAXUSHORT, __alignof(MESSAGE_RESOURCE_ENTRY)))

/**
 * Maximum size, in bytes, of a Unicode string that
 * can be stored in a message table. The size is
 * constrained by the ability to serialize the string
 * into a MESSAGE_RESOURCE_ENTRY.
 *
 * @remark	A valid UNICODE_STRING may actually be shorter
 *			than the size defined here.
 *			This is validated via messagetable_IsValidUnicodeString.
 *
 * @see MESSAGE_RESOURCE_ENTRY.
 * @see messagetable_IsValidUnicodeString.
 */
#define MESSAGE_TABLE_UNICODE_STRING_MAX_SIZE \
	(MESSAGE_RESOURCE_ENTRY_MAX_SIZE - UFIELD_OFFSET(MESSAGE_RESOURCE_ENTRY, acText) - sizeof(UNICODE_NULL))
C_ASSERT(MESSAGE_TABLE_UNICODE_STRING_MAX_SIZE > 0);

/**
 * Maximum size, in bytes, of an ANSI string that
 * can be stored in a message table. The size is
 * constrained by the ability to serialize the string
 * into a MESSAGE_RESOURCE_ENTRY.
 *
 * @see MESSAGE_RESOURCE_ENTRY.
 */
#define MESSAGE_TABLE_ANSI_STRING_MAX_SIZE \
	(MESSAGE_RESOURCE_ENTRY_MAX_SIZE - UFIELD_OFFSET(MESSAGE_RESOURCE_ENTRY, acText) - sizeof(ANSI_NULL))
C_ASSERT(MESSAGE_TABLE_ANSI_STRING_MAX_SIZE > 0);


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
_IRQL_requires_max_(APC_LEVEL)
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
_IRQL_requires_max_(APC_LEVEL)
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
_IRQL_requires_max_(APC_LEVEL)
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

	CLOSE(pvBuffer, ExFreePool);
}

/**
 * Clears a message table entry.
 *
 * @param[in]	ptEntry	Entry to clear.
 *
 * @remark	This does not actually free the entry
 *			itself, only the pointers within.
 */
_IRQL_requires_max_(APC_LEVEL)
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

	if (psString->Length > MESSAGE_TABLE_ANSI_STRING_MAX_SIZE)
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

	if (pusString->Length > MESSAGE_TABLE_UNICODE_STRING_MAX_SIZE)
	{
		goto lblCleanup;
	}

	bIsValid = TRUE;

lblCleanup:
	return bIsValid;
}

/**
 * Inserts an ANSI message resource entry
 * into a message table.
 *
 * @param[in]	hMessageTable	Message table to insert into.
 * @param[in]	nEntryId		ID of the string to insert.
 * @param[in]	ptResourceEntry	The resource entry.
 *
 * @returns NTSTATUS
 *
 * @remark	It is up to the caller to verify that
 *			the resource entry is indeed an ANSI
 *			resource entry.
 */
_IRQL_requires_(PASSIVE_LEVEL)
STATIC
NTSTATUS
messagetable_InsertResourceEntryAnsi(
	_In_	HMESSAGETABLE				hMessageTable,
	_In_	ULONG						nEntryId,
	_In_	PCMESSAGE_RESOURCE_ENTRY	ptResourceEntry
)
{
	NTSTATUS	eStatus		= STATUS_UNSUCCESSFUL;
	SIZE_T		cbStringMax	= 0;
	ANSI_STRING	sString		= { 0 };

	PAGED_CODE();

	ASSERT(NULL != hMessageTable);
	ASSERT(NULL != ptResourceEntry);
	ASSERT(0 == ptResourceEntry->fFlags);
	ASSERT(PASSIVE_LEVEL == KeGetCurrentIrql());

	// Calculate the string's maximum size (buffer size).
	eStatus = RtlSIZETSub(ptResourceEntry->cbLength,
						  UFIELD_OFFSET(MESSAGE_RESOURCE_ENTRY, acText),
						  &cbStringMax);
	if (!NT_SUCCESS(eStatus))
	{
		goto lblCleanup;
	}

	eStatus = UTIL_InitAnsiStringCb((PCHAR)&(ptResourceEntry->acText[0]),
									cbStringMax,
									&sString);
	if (!NT_SUCCESS(eStatus))
	{
		goto lblCleanup;
	}

	eStatus = MESSAGETABLE_InsertAnsi(hMessageTable,
									  nEntryId,
									  &sString);
	if (!NT_SUCCESS(eStatus))
	{
		goto lblCleanup;
	}

	eStatus = STATUS_SUCCESS;

lblCleanup:
	return eStatus;
}

/**
 * Inserts a Unicode message resource entry
 * into a message table.
 *
 * @param[in]	hMessageTable	Message table to insert into.
 * @param[in]	nEntryId		ID of the string to insert.
 * @param[in]	ptResourceEntry	The resource entry.
 *
 * @returns NTSTATUS
 *
 * @remark	It is up to the caller to verify that
 *			the resource entry is indeed a Unicode
 *			resource entry.
 */
_IRQL_requires_(PASSIVE_LEVEL)
STATIC
NTSTATUS
messagetable_InsertResourceEntryUnicode(
	_In_	HMESSAGETABLE				hMessageTable,
	_In_	ULONG						nEntryId,
	_In_	PCMESSAGE_RESOURCE_ENTRY	ptResourceEntry
)
{
	NTSTATUS		eStatus		= STATUS_UNSUCCESSFUL;
	SIZE_T			cbStringMax	= 0;
	UNICODE_STRING	usString	= { 0 };

	PAGED_CODE();

	ASSERT(NULL != hMessageTable);
	ASSERT(NULL != ptResourceEntry);
	ASSERT(1 == ptResourceEntry->fFlags);
	ASSERT(PASSIVE_LEVEL == KeGetCurrentIrql());

	// Calculate the string's maximum size (buffer size).
	eStatus = RtlSIZETSub(ptResourceEntry->cbLength,
						  UFIELD_OFFSET(MESSAGE_RESOURCE_ENTRY, acText),
						  &cbStringMax);
	if (!NT_SUCCESS(eStatus))
	{
		goto lblCleanup;
	}

	eStatus = UTIL_InitUnicodeStringCb((PWCHAR)&(ptResourceEntry->acText[0]),
									   cbStringMax,
									   &usString);
	if (!NT_SUCCESS(eStatus))
	{
		goto lblCleanup;
	}

	eStatus = MESSAGETABLE_InsertUnicode(hMessageTable,
										 nEntryId,
										 &usString);
	if (!NT_SUCCESS(eStatus))
	{
		goto lblCleanup;
	}

	eStatus = STATUS_SUCCESS;

lblCleanup:
	return eStatus;
}

/**
 * Calculates the size of a message table entry
 * in its serialized form.
 *
 * @param[in]	ptEntry	Entry to operate on.
 *
 * @returns USHORT.
 */
_IRQL_requires_max_(APC_LEVEL)
STATIC
USHORT
messagetable_SizeofSerializedEntry(
	_In_	PCMESSAGE_TABLE_ENTRY	ptEntry
)
{
	NTSTATUS	eStatus	= STATUS_UNSUCCESSFUL;
	USHORT		cbTotal	= 0;

	PAGED_CODE();

	ASSERT(NULL != ptEntry);

	// Begin with the size of the header.
	cbTotal = UFIELD_OFFSET(MESSAGE_RESOURCE_ENTRY, acText);

	// Add the size of the string itself.
	if (ptEntry->bUnicode)
	{
		eStatus = RtlUShortAdd(cbTotal,
							   ptEntry->tData.tUnicode.Length,
							   &cbTotal);
		ASSERT(NT_SUCCESS(eStatus));

		eStatus = RtlUShortAdd(cbTotal,
							   sizeof(UNICODE_NULL),
							   &cbTotal);
		ASSERT(NT_SUCCESS(eStatus));
	}
	else
	{
		eStatus = RtlUShortAdd(cbTotal,
							   ptEntry->tData.tAnsi.Length,
							   &cbTotal);
		ASSERT(NT_SUCCESS(eStatus));

		eStatus = RtlUShortAdd(cbTotal,
							   sizeof(ANSI_NULL),
							   &cbTotal);
		ASSERT(NT_SUCCESS(eStatus));
	}

	// Now add any required padding.
	eStatus = RtlUShortAdd(cbTotal,
						   cbTotal % __alignof(MESSAGE_RESOURCE_ENTRY),
						   &cbTotal);
	ASSERT(NT_SUCCESS(eStatus));

	return cbTotal;
}

/**
 * Callback for counting the number of blocks
 * in the serialized message table and the
 * total size required for the serialized strings.
 *
 * @param[in]	ptEntry					Entry currently being enumerated.
 *										Previous entry, or NULL if this is the first
 *										invocation of the callback.
 * @param[in]	pvContext				Alias for PCOUNTING_CALLBACK_CONTEXT.
 * @param[out]	pbContinueEnumeration	Ignored. Enumeration always continues.
 *
 * @see COUNTING_CALLBACK_CONTEXT.
 */
_IRQL_requires_max_(APC_LEVEL)
STATIC
VOID
messagetable_CountingCallback(
	_In_		PCMESSAGE_TABLE_ENTRY	ptEntry,
	_In_opt_	PCMESSAGE_TABLE_ENTRY	ptPreviousEntry,
	_In_		PVOID					pvContext,
	_Out_		PBOOLEAN				pbContinueEnumeration
)
{
	NTSTATUS					eStatus		= STATUS_UNSUCCESSFUL;
	PCOUNTING_CALLBACK_CONTEXT	ptContext	= (PCOUNTING_CALLBACK_CONTEXT)pvContext;

	PAGED_CODE();

	ASSERT(NULL != ptEntry);
	ASSERT(NULL != pvContext);
	ASSERT(NULL != pbContinueEnumeration);
	ASSERT(*pbContinueEnumeration);

	if ((NULL == ptPreviousEntry) ||
		(1 != ptEntry->nEntryId - ptPreviousEntry->nEntryId))
	{
		// This is either the first time we entered the callback,
		// or the current ID begins a new block.
		eStatus = RtlULongAdd(ptContext->nBlocks,
							  1,
							  &(ptContext->nBlocks));
		ASSERT(NT_SUCCESS(eStatus));
	}

	// Add the size for the current string.
	eStatus = RtlSIZETAdd(ptContext->cbTotalStrings,
						  messagetable_SizeofSerializedEntry(ptEntry),
						  &(ptContext->cbTotalStrings));
	ASSERT(NT_SUCCESS(eStatus));
}

/**
 * Callback for serializing message table entries.
 * Writes each entry to an allocated buffer.
 *
 * @param[in]	ptEntry					Entry currently being enumerated.
 *										Previous entry, or NULL if this is the first
 *										invocation of the callback.
 * @param[in]	pvContext				Alias for PSERIALIZING_CALLBACK_CONTEXT.
 * @param[out]	pbContinueEnumeration	Ignored. Enumeration always continues.
 *
 * @see SERIALIZING_CALLBACK_CONTEXT.
 */
_IRQL_requires_max_(APC_LEVEL)
STATIC
VOID
messagetable_SerializingCallback(
	_In_		PCMESSAGE_TABLE_ENTRY	ptEntry,
	_In_opt_	PCMESSAGE_TABLE_ENTRY	ptPreviousEntry,
	_In_		PVOID					pvContext,
	_Out_		PBOOLEAN				pbContinueEnumeration
)
{
	NTSTATUS						eStatus			= STATUS_UNSUCCESSFUL;
	PSERIALIZING_CALLBACK_CONTEXT	ptContext		= (PSERIALIZING_CALLBACK_CONTEXT)pvContext;
	PMESSAGE_RESOURCE_BLOCK			ptCurrentBlock	= NULL;
	PMESSAGE_RESOURCE_ENTRY			ptCurrentEntry	= NULL;

	PAGED_CODE();

	ASSERT(NULL != ptEntry);
	ASSERT(NULL != pvContext);
	ASSERT(NULL != pbContinueEnumeration);
	ASSERT(*pbContinueEnumeration);

	// Obtain a pointer to the current block header
	ptCurrentBlock = &(ptContext->ptMessageData->atBlocks[ptContext->nCurrentBlock]);

	// If there is a new block, set it up
	if ((NULL == ptPreviousEntry) ||
		(1 != ptEntry->nEntryId - ptPreviousEntry->nEntryId))
	{
		//
		// New block!
		//

		eStatus = RtlULongAdd(ptContext->nCurrentBlock,
							  1,
							  &(ptContext->nCurrentBlock));
		ASSERT(NT_SUCCESS(eStatus));

		++ptCurrentBlock;

		ptCurrentBlock->nLowId = ptEntry->nEntryId;
		ptCurrentBlock->cbOffsetToEntries = RtlPointerToOffset(ptContext->ptMessageData,
															  ptContext->pcCurrentStringPosition);
	}

	// Update the last ID for the current block
	ptCurrentBlock->nHighId = ptEntry->nEntryId;

	// Copy
	ptCurrentEntry = (PMESSAGE_RESOURCE_ENTRY)(ptContext->pcCurrentStringPosition);
	ptCurrentEntry->cbLength = messagetable_SizeofSerializedEntry(ptEntry);
	if (ptEntry->bUnicode)
	{
		ptCurrentEntry->fFlags = 1;
		RtlMoveMemory(ptCurrentEntry->acText,
					  ptEntry->tData.tUnicode.Buffer,
					  ptEntry->tData.tUnicode.Length);
	}
	else
	{
		ptCurrentEntry->fFlags = 0;
		RtlMoveMemory(ptCurrentEntry->acText,
					  ptEntry->tData.tAnsi.Buffer,
					  ptEntry->tData.tAnsi.Length);
	}
	ptContext->pcCurrentStringPosition += ptCurrentEntry->cbLength;
}

/**
 * Acquires the message table lock.
 *
 * @param[in]	ptMessageTable	Message table to lock.
 * @param[in]	bExclusive		Indicates whether the lock
 *								is to be acquired exclusively.
 *
 * @returns NTSTATUS
 */
_IRQL_requires_max_(APC_LEVEL)
_When_(bExclusive, _Acquires_exclusive_lock_(ptMessageTable->tLock))
_When_(!bExclusive, _Acquires_shared_lock_(ptMessageTable->tLock))
_Acquires_lock_(_Global_critical_region_)
STATIC
NTSTATUS
messagetable_AcquireLock(
	_In_	PMESSAGE_TABLE	ptMessageTable,
	_In_	BOOLEAN			bExclusive
)
{
	NTSTATUS	eStatus					= STATUS_UNSUCCESSFUL;
	BOOLEAN		bResult					= FALSE;
	BOOLEAN		bLeaveCriticalRegion	= FALSE;
	BOOLEAN		bReleaseLock			= FALSE;

	PAGED_CODE();

	ASSERT(NULL != ptMessageTable);

	KeEnterCriticalRegion();
	bLeaveCriticalRegion = TRUE;

	bResult =
		(bExclusive)
		? (ExAcquireResourceExclusiveLite(&(ptMessageTable->tLock), TRUE))
		: (ExAcquireResourceSharedLite(&(ptMessageTable->tLock), TRUE));
	if (!bResult)
	{
		eStatus = STATUS_LOCK_NOT_GRANTED;
		goto lblCleanup;
	}
	bReleaseLock = TRUE;

	// All done!
	bReleaseLock = FALSE;
	bLeaveCriticalRegion = FALSE;

	eStatus = STATUS_SUCCESS;

lblCleanup:
	if (bReleaseLock)
	{
		ExReleaseResourceLite(&(ptMessageTable->tLock));
		bReleaseLock = FALSE;
	}
	if (bLeaveCriticalRegion)
	{
		KeLeaveCriticalRegion();
		bLeaveCriticalRegion = FALSE;
	}

	return eStatus;
}

/**
 * Releases the message table lock.
 *
 * @param[in]	ptMessageTable	Message table to unlock.
 */
_IRQL_requires_max_(APC_LEVEL)
_Releases_lock_(ptMessageTable->tLock)
_Releases_lock_(_Global_critical_region_)
STATIC
VOID
messagetable_ReleaseLock(
	_In_	PMESSAGE_TABLE	ptMessageTable
)
{
	PAGED_CODE();

	ASSERT(NULL != ptMessageTable);

	ExReleaseResourceLite(&(ptMessageTable->tLock));
	KeLeaveCriticalRegion();
}

_IRQL_requires_(PASSIVE_LEVEL)
NTSTATUS
MESSAGETABLE_Create(
	_Out_	PHMESSAGETABLE	phMessageTable
)
{
	NTSTATUS		eStatus			= STATUS_UNSUCCESSFUL;
	PMESSAGE_TABLE	ptMessageTable	= NULL;

	PAGED_CODE();

	if ((NULL == phMessageTable) ||
		(PASSIVE_LEVEL != KeGetCurrentIrql()))
	{
		eStatus = STATUS_INVALID_PARAMETER;
		goto lblCleanup;
	}

	ptMessageTable = ExAllocatePoolWithTag(NonPagedPool,
										   sizeof(*ptMessageTable),
										   MESSAGE_TABLE_POOL_TAG);
	if (NULL == ptMessageTable)
	{
		eStatus = STATUS_INSUFFICIENT_RESOURCES;
		goto lblCleanup;
	}
	RtlSecureZeroMemory(ptMessageTable, sizeof(*ptMessageTable));

	RtlInitializeGenericTableAvl(&(ptMessageTable->tTable),
								 &messagetable_CompareRoutine,
								 &messagetable_AllocateRoutine,
								 &messagetable_FreeRoutine,
								 NULL);
	ptMessageTable->bTableInitialized = TRUE;

	eStatus = ExInitializeResourceLite(&(ptMessageTable->tLock));
	if (!NT_SUCCESS(eStatus))
	{
		goto lblCleanup;
	}
	ptMessageTable->bLockInitialized = TRUE;

	// Transfer ownership:
	*phMessageTable = (HMESSAGETABLE)ptMessageTable;
	ptMessageTable = NULL;

	eStatus = STATUS_SUCCESS;

lblCleanup:
#pragma warning(suppress: 4133)	// warning C4133: 'function': incompatible types - from 'PMESSAGE_TABLE' to 'HMESSAGETABLE'
	CLOSE(ptMessageTable, MESSAGETABLE_Destroy);

	return eStatus;
}

_IRQL_requires_(PASSIVE_LEVEL)
NTSTATUS
MESSAGETABLE_CreateFromResource(
	_In_reads_bytes_(cbMessageTableResource)	PVOID			pvMessageTableResource,
	_In_										ULONG			cbMessageTableResource,
	_Out_										PHMESSAGETABLE	phMessageTable
)
{
	NTSTATUS					eStatus			= STATUS_UNSUCCESSFUL;
	HMESSAGETABLE				hMessageTable	= NULL;
	PCMESSAGE_RESOURCE_DATA		ptResourceData	= (PCMESSAGE_RESOURCE_DATA)pvMessageTableResource;
	ULONG						nCurrentBlock	= 0;
	PCMESSAGE_RESOURCE_BLOCK	ptCurrentBlock	= NULL;
	ULONG						nCurrentId		= 0;
	PCMESSAGE_RESOURCE_ENTRY	ptCurrentEntry	= NULL;

	PAGED_CODE();

	if ((NULL == pvMessageTableResource) ||
		(0 == cbMessageTableResource) ||
		(NULL == phMessageTable) ||
		(PASSIVE_LEVEL != KeGetCurrentIrql()))
	{
		eStatus = STATUS_INVALID_PARAMETER;
		goto lblCleanup;
	}

	eStatus = MESSAGETABLE_Create(&hMessageTable);
	if (!NT_SUCCESS(eStatus))
	{
		goto lblCleanup;
	}

	for (nCurrentBlock = 0;
		 nCurrentBlock < ptResourceData->nBlocks;
		 ++nCurrentBlock)
	{
		ptCurrentBlock = &(ptResourceData->atBlocks[nCurrentBlock]);

		nCurrentId = ptCurrentBlock->nLowId;
		ptCurrentEntry = (PCMESSAGE_RESOURCE_ENTRY)RtlOffsetToPointer(pvMessageTableResource,
																	  ptCurrentBlock->cbOffsetToEntries);
		while (nCurrentId <= ptCurrentBlock->nHighId)
		{
			if (1 == ptCurrentEntry->fFlags)
			{
				// This is a Unicode string.
				eStatus = messagetable_InsertResourceEntryUnicode(hMessageTable,
																  nCurrentId,
																  ptCurrentEntry);
			}
			else if (0 == ptCurrentEntry->fFlags)
			{
				// This is an ANSI string.
				eStatus = messagetable_InsertResourceEntryAnsi(hMessageTable,
															   nCurrentId,
															   ptCurrentEntry);
			}
			else
			{
				// Wat.
				ASSERT(FALSE);
				eStatus = STATUS_INTERNAL_DB_CORRUPTION;
			}
			if (!NT_SUCCESS(eStatus))
			{
				goto lblCleanup;
			}

			++nCurrentId;
			ptCurrentEntry = (PCMESSAGE_RESOURCE_ENTRY)RtlOffsetToPointer(ptCurrentEntry,
																		  ptCurrentEntry->cbLength);
		}
	}

	// Transfer ownership:
	*phMessageTable = hMessageTable;
	hMessageTable = NULL;

	eStatus = STATUS_SUCCESS;

lblCleanup:
	CLOSE(hMessageTable, MESSAGETABLE_Destroy);

	return eStatus;
}

_IRQL_requires_(PASSIVE_LEVEL)
VOID
MESSAGETABLE_Destroy(
	_In_	HMESSAGETABLE	hMessageTable
)
{
	PMESSAGE_TABLE	ptMessageTable	= (PMESSAGE_TABLE)hMessageTable;
	PVOID			pvData			= NULL;

	PAGED_CODE();

	if ((NULL == hMessageTable) ||
		(PASSIVE_LEVEL != KeGetCurrentIrql()))
	{
		goto lblCleanup;
	}

	if (ptMessageTable->bLockInitialized)
	{
		(VOID)ExDeleteResourceLite(&(ptMessageTable->tLock));
		ptMessageTable->bLockInitialized = FALSE;
	}

	if (ptMessageTable->bTableInitialized)
	{
		for (pvData = RtlEnumerateGenericTableAvl(&(ptMessageTable->tTable), TRUE);
			 NULL != pvData;
			 pvData = RtlEnumerateGenericTableAvl(&(ptMessageTable->tTable), FALSE))
		{
			// Clear the current entry ...
			messagetable_ClearEntry((PMESSAGE_TABLE_ENTRY)pvData);

			// ... and delete it from the tree.
			(VOID)RtlDeleteElementGenericTableAvl(&(ptMessageTable->tTable),
												  pvData);
		}

		ASSERT(RtlIsGenericTableEmptyAvl(&(ptMessageTable->tTable)));

		ptMessageTable->bTableInitialized = FALSE;
	}

	CLOSE(ptMessageTable, ExFreePool);

lblCleanup:
	return;
}

_IRQL_requires_(PASSIVE_LEVEL)
NTSTATUS
MESSAGETABLE_InsertAnsi(
	_In_	HMESSAGETABLE	hMessageTable,
	_In_	ULONG			nEntryId,
	_In_	PCANSI_STRING	psString
)
{
	NTSTATUS				eStatus				= STATUS_UNSUCCESSFUL;
	PMESSAGE_TABLE			ptMessageTable		= (PMESSAGE_TABLE)hMessageTable;
	BOOLEAN					bLockAcquired		= FALSE;
	MESSAGE_TABLE_ENTRY		tEntry				= { 0 };
	PCHAR					pcDuplicateString	= NULL;
	BOOLEAN					bNewElement			= FALSE;
	PMESSAGE_TABLE_ENTRY	ptInserted			= NULL;

	PAGED_CODE();

	if ((NULL == hMessageTable) ||
		(!messagetable_IsValidAnsiString(psString)) ||
		(PASSIVE_LEVEL != KeGetCurrentIrql()))
	{
		eStatus = STATUS_INVALID_PARAMETER;
		goto lblCleanup;
	}

	eStatus = messagetable_AcquireLock(ptMessageTable, TRUE);
	if (!NT_SUCCESS(eStatus))
	{
		goto lblCleanup;
	}
	bLockAcquired = TRUE;

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
		(PMESSAGE_TABLE_ENTRY)RtlInsertElementGenericTableAvl(&(ptMessageTable->tTable),
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
	if (bLockAcquired)
	{
		messagetable_ReleaseLock(ptMessageTable);
		bLockAcquired = FALSE;
	}

	return eStatus;
}

_IRQL_requires_(PASSIVE_LEVEL)
NTSTATUS
MESSAGETABLE_InsertUnicode(
	_In_	HMESSAGETABLE		hMessageTable,
	_In_	ULONG				nEntryId,
	_In_	PCUNICODE_STRING	pusString
)
{
	NTSTATUS				eStatus				= STATUS_UNSUCCESSFUL;
	PMESSAGE_TABLE			ptMessageTable		= (PMESSAGE_TABLE)hMessageTable;
	BOOLEAN					bLockAcquired		= FALSE;
	MESSAGE_TABLE_ENTRY		tEntry				= { 0 };
	PWCHAR					pwcDuplicateString	= NULL;
	BOOLEAN					bNewElement			= FALSE;
	PMESSAGE_TABLE_ENTRY	ptInserted			= NULL;

	PAGED_CODE();

	if ((NULL == hMessageTable) ||
		(!messagetable_IsValidUnicodeString(pusString)) ||
		(PASSIVE_LEVEL != KeGetCurrentIrql()))
	{
		eStatus = STATUS_INVALID_PARAMETER;
		goto lblCleanup;
	}

	eStatus = messagetable_AcquireLock(ptMessageTable, TRUE);
	if (!NT_SUCCESS(eStatus))
	{
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
		(PMESSAGE_TABLE_ENTRY)RtlInsertElementGenericTableAvl(&(ptMessageTable->tTable),
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
	if (bLockAcquired)
	{
		messagetable_ReleaseLock(ptMessageTable);
		bLockAcquired = FALSE;
	}

	return eStatus;
}

_IRQL_requires_(PASSIVE_LEVEL)
NTSTATUS
MESSAGETABLE_EnumerateEntries(
	_In_		HMESSAGETABLE							hMessageTable,
	_In_		PFN_MESSAGETABLE_ENUMERATION_CALLBACK	pfnCallback,
	_In_opt_	PVOID									pvContext
)
{
	NTSTATUS				eStatus			= STATUS_UNSUCCESSFUL;
	PMESSAGE_TABLE			ptMessageTable	= (PMESSAGE_TABLE)hMessageTable;
	BOOLEAN					bLockAcquired	= FALSE;
	PVOID					pvRestartKey	= NULL;
	PVOID					pvData			= NULL;
	PCMESSAGE_TABLE_ENTRY	ptPrevious		= NULL;
	PCMESSAGE_TABLE_ENTRY	ptCurrent		= NULL;
	BOOLEAN					bContinue		= FALSE;

	PAGED_CODE();

	if ((NULL == hMessageTable) ||
		(NULL == pfnCallback) ||
		(PASSIVE_LEVEL != KeGetCurrentIrql()))
	{
		eStatus = STATUS_INVALID_PARAMETER;
		goto lblCleanup;
	}

	eStatus = messagetable_AcquireLock(ptMessageTable, FALSE);
	if (!NT_SUCCESS(eStatus))
	{
		goto lblCleanup;
	}
	bLockAcquired = TRUE;

	for (pvData = RtlEnumerateGenericTableWithoutSplayingAvl(&(ptMessageTable->tTable),
															 &pvRestartKey);
		 NULL != pvData;
		 pvData = RtlEnumerateGenericTableWithoutSplayingAvl(&(ptMessageTable->tTable),
															 &pvRestartKey))
	{
		ptCurrent = (PCMESSAGE_TABLE_ENTRY)pvData;

		bContinue = TRUE;
		pfnCallback(ptCurrent,
					ptPrevious,
					pvContext,
					&bContinue);
		if (!bContinue)
		{
			break;
		}

		ptPrevious = ptCurrent;
	}

	eStatus = STATUS_SUCCESS;

lblCleanup:
	if (bLockAcquired)
	{
		messagetable_ReleaseLock(ptMessageTable);
		bLockAcquired = FALSE;
	}

	return eStatus;
}

_IRQL_requires_(PASSIVE_LEVEL)
NTSTATUS
MESSAGETABLE_Serialize(
	_In_													HMESSAGETABLE	hMessageTable,
	_Outptr_result_bytebuffer_(*pcbMessageTableResource)	PVOID *			ppvMessageTableResource,
	_Out_													PSIZE_T			pcbMessageTableResource
)
{
	NTSTATUS						eStatus				= STATUS_UNSUCCESSFUL;
	PMESSAGE_TABLE					ptMessageTable		= (PMESSAGE_TABLE)hMessageTable;
	BOOLEAN							bLockAcquired		= FALSE;
	COUNTING_CALLBACK_CONTEXT		tCountingContext	= { 0 };
	SIZE_T							cbHeader			= 0;
	SIZE_T							cbTotal				= 0;
	PMESSAGE_RESOURCE_DATA			ptMessageData		= NULL;
	SERIALIZING_CALLBACK_CONTEXT	tSerializingContext	= { 0 };

	PAGED_CODE();

	if ((NULL == hMessageTable) ||
		(NULL == ppvMessageTableResource) ||
		(NULL == pcbMessageTableResource) ||
		(PASSIVE_LEVEL != KeGetCurrentIrql()))
	{
		eStatus = STATUS_INVALID_PARAMETER;
		goto lblCleanup;
	}

	eStatus = messagetable_AcquireLock(ptMessageTable, FALSE);
	if (!NT_SUCCESS(eStatus))
	{
		goto lblCleanup;
	}
	bLockAcquired = TRUE;

	eStatus = MESSAGETABLE_EnumerateEntries(hMessageTable,
											&messagetable_CountingCallback,
											&tCountingContext);
	if (!NT_SUCCESS(eStatus))
	{
		goto lblCleanup;
	}

	// Calculate the size of the header
	eStatus = RtlSIZETAdd(UFIELD_OFFSET(MESSAGE_RESOURCE_DATA, atBlocks),
						  tCountingContext.nBlocks * sizeof(ptMessageData->atBlocks[0]),
						  &cbHeader);
	ASSERT(NT_SUCCESS(eStatus));

	// Calculate the total size
	eStatus = RtlSIZETAdd(tCountingContext.cbTotalStrings,
						  cbHeader,
						  &cbTotal);
	ASSERT(NT_SUCCESS(eStatus));

	// Allocate memory for everything
	ptMessageData = ExAllocatePoolWithTag(PagedPool, cbTotal, MESSAGE_TABLE_POOL_TAG);
	if (NULL == ptMessageData)
	{
		eStatus = STATUS_INSUFFICIENT_RESOURCES;
		goto lblCleanup;
	}
	RtlSecureZeroMemory(ptMessageData, cbTotal);

	// Initialize the header
	ptMessageData->nBlocks = tCountingContext.nBlocks;

	// Serialize
	tSerializingContext.ptMessageData = ptMessageData;
	tSerializingContext.nCurrentBlock = (ULONG)-1;
	tSerializingContext.pcCurrentStringPosition = (PUCHAR)RtlOffsetToPointer(tSerializingContext.ptMessageData,
																			 cbHeader);
	eStatus = MESSAGETABLE_EnumerateEntries(hMessageTable,
											&messagetable_SerializingCallback,
											&tSerializingContext);
	if (!NT_SUCCESS(eStatus))
	{
		goto lblCleanup;
	}

	// Transfer ownership:
	*ppvMessageTableResource = ptMessageData;
	ptMessageData = NULL;
	*pcbMessageTableResource = cbTotal;

	eStatus = STATUS_SUCCESS;

lblCleanup:
	CLOSE(ptMessageData, ExFreePool);
	if (bLockAcquired)
	{
		messagetable_ReleaseLock(ptMessageTable);
		bLockAcquired = FALSE;
	}

	return eStatus;
}
