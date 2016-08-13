/**
 * @file MessageTable.h
 * @author biko
 * @date 2016-07-30
 *
 * MessageTable module public header.
 * Contains routines for manipulation of
 * message table resources.
 */
#pragma once

/** Headers *************************************************************/
#include <ntifs.h>

#include "Util.h"


/** Typedefs ************************************************************/

/**
 * Handle to a message table.
 */
DECLARE_HANDLE(HMESSAGETABLE);
typedef HMESSAGETABLE *PHMESSAGETABLE;

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

/**
 * Callback used when enumerating message table entries.
 *
 * @param[in]	ptEntry					Entry currently being enumerated.
 *										The entry and its contents MUST NOT
 *										be modified by the callback.
 * @param[in]	ptPreviousEntry			Previous entry, or NULL if this is the first
 *										invocation of the callback. The callback MUST
 *										NOT modify the entry or its contents.
 * @param[in]	pvContext				Context specified when invoking the enumeration
 *										function.
 * @param[out]	pbContinueEnumeration	The callback should set this to FALSE to
 *										abort the enumeration. The enumeration function
 *										sets this to TRUE before invoking the callback.
 */
typedef
VOID
FN_MESSAGETABLE_ENUMERATION_CALLBACK(
	_In_		PCMESSAGE_TABLE_ENTRY	ptEntry,
	_In_opt_	PCMESSAGE_TABLE_ENTRY	ptPreviousEntry,
	_In_opt_	PVOID					pvContext,
	_Out_		PBOOLEAN				pbContinueEnumeration
);
typedef FN_MESSAGETABLE_ENUMERATION_CALLBACK *PFN_MESSAGETABLE_ENUMERATION_CALLBACK;


/** Functions ***********************************************************/

/**
 * Creates an empty message table.
 *
 * @param[out]	phMessageTable	Will receive a handle
								to the message table.
 *
 * @returns NTSTATUS
 */
_IRQL_requires_(PASSIVE_LEVEL)
PAGEABLE
NTSTATUS
MESSAGETABLE_Create(
	_Out_	PHMESSAGETABLE	phMessageTable
);

/**
 * Creates a message table by parsing a message table resource.
 *
 * @param[in]	pvMessageTableResource	Resource buffer to parse.
 * @param[in]	cbMessageTableResource	Size of the buffer, in bytes.
 * @param[in]	bCompact				Indicates whether the strings
 *										in the message table resource
 *										should be compacted on insertion
 *										into the table.
 * @param[out]	phMessageTable			Will receive a handle
										to the message table.
 *
 * @returns NTSTATUS
 *
 * @see MESSAGETABLE_InsertAnsi
 * @see MESSAGETABLE_InsertUnicode
 */
_IRQL_requires_(PASSIVE_LEVEL)
PAGEABLE
NTSTATUS
MESSAGETABLE_CreateFromResource(
	_In_reads_bytes_(cbMessageTableResource)	PVOID			pvMessageTableResource,
	_In_										ULONG			cbMessageTableResource,
	_In_										BOOLEAN			bCompact,
	_Out_										PHMESSAGETABLE	phMessageTable
);

/**
 * Destroys a message table.
 *
 * @param[in]	hMessageTable	Message table to destroy.
 */
_IRQL_requires_(PASSIVE_LEVEL)
PAGEABLE
VOID
MESSAGETABLE_Destroy(
	_In_	HMESSAGETABLE	hMessageTable
);

/**
 * Inserts an ANSI string into the message table.
 *
 * @param[in]	hMessageTable	Message table to insert into.
 * @param[in]	nEntryId		ID of the string to insert.
 * @param[in]	psString		String to insert.
 * @param[in]	bCompact		Indicates whether to compact
 *								the string. A compacted
 *								string contains only the string
 *								data and a terminating null.
 *								A non-compacted string contains
 *								the whole ANSI_STRING buffer.
 *
 * @returns NTSTATUS
 *
 * @remark	If an entry with the same ID already exists,
 *			it is overwritten.
 */
_IRQL_requires_(PASSIVE_LEVEL)
PAGEABLE
NTSTATUS
MESSAGETABLE_InsertAnsi(
	_In_	HMESSAGETABLE	hMessageTable,
	_In_	ULONG			nEntryId,
	_In_	PCANSI_STRING	psString,
	_In_	BOOLEAN			bCompact
);

/**
 * Inserts a Unicode string into the message table.
 *
 * @param[in]	hMessageTable	Message table to insert into.
 * @param[in]	nEntryId		ID of the string to insert.
 * @param[in]	pusString		String to insert.
 * @param[in]	bCompact		Indicates whether to compact
 *								the string. A compacted
 *								string contains only the string
 *								data and a terminating null.
 *								A non-compacted string contains
 *								the whole UNICODE_STRING buffer.
 *
 * @returns NTSTATUS
 *
 * @remark	If an entry with the same ID already exists,
 *			it is overwritten.
 */
_IRQL_requires_(PASSIVE_LEVEL)
PAGEABLE
NTSTATUS
MESSAGETABLE_InsertUnicode(
	_In_	HMESSAGETABLE		hMessageTable,
	_In_	ULONG				nEntryId,
	_In_	PCUNICODE_STRING	pusString,
	_In_	BOOLEAN				bCompact
);

/**
 * Retrieves an entry with the specified ID.
 *
 * @param[in]	hMessageTable	Message table to search for the entry.
 * @param[in]	nEntryId		ID of the entry to look up.
 * @param[out]	pptEntry		Will receive a pointer to the found entry.
 *
 * @returns NTSTATUS
 *
 * @remark	The entry and its contents MUST NOT
 *			be modified by the calling code.
 */
_IRQL_requires_(PASSIVE_LEVEL)
PAGEABLE
NTSTATUS
MESSAGETABLE_GetEntry(
	_In_		HMESSAGETABLE			hMessageTable,
	_In_		ULONG					nEntryId,
	_Outptr_	PCMESSAGE_TABLE_ENTRY *	pptEntry
);

/**
 * Enumerates all entries in a message table.
 * The enumeration is performed in ascending ID order.
 *
 * @param[in]	hMessageTable	Message table to enumerate.
 * @param[in]	pfnCallback		Enumeration callback.
 * @param[in]	pvContext		Optional arbitrary context to be
 *								passed to the callback.
 *
 * @returns NTSTATUS
 *
 * @remark	There is no return value reserved
 *			for indicating an aborted enumeration.
 *			It is up to the caller to determine
 *			whether the callback aborted the enumeration.
 * @remark	Inserting an entry from within the callback
 *			may result in a deadlock.
 */
_IRQL_requires_(PASSIVE_LEVEL)
PAGEABLE
NTSTATUS
MESSAGETABLE_EnumerateEntries(
	_In_		HMESSAGETABLE							hMessageTable,
	_In_		PFN_MESSAGETABLE_ENUMERATION_CALLBACK	pfnCallback,
	_In_opt_	PVOID									pvContext
);

/**
 * Serializes a message table to a message table resource.
 *
 * @param[in]	hMessageTable			Message table to serialize.
 * @param[out]	ppvMessageTableResource	Will receive the serialized message table.
 * @param[in]	pcbMessageTableResource	Will receive the serialized table's size, in bytes.
 *
 * @returns NTSTATUS
 *
 * @remark	The returned buffer is allocated from the _paged_ pool.
 *			Free it using ExFreePool.
 */
_IRQL_requires_(PASSIVE_LEVEL)
PAGEABLE
NTSTATUS
MESSAGETABLE_Serialize(
	_In_													HMESSAGETABLE	hMessageTable,
	_Outptr_result_bytebuffer_(*pcbMessageTableResource)	PVOID *			ppvMessageTableResource,
	_Out_													PSIZE_T			pcbMessageTableResource
);
