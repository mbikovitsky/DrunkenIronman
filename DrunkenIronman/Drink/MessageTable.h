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


/** Typedefs ************************************************************/

/**
 * Handle to a message table.
 */
DECLARE_HANDLE(HMESSAGETABLE);
typedef HMESSAGETABLE *PHMESSAGETABLE;


/** Functions ***********************************************************/

/**
 * Creates an empty message table.
 *
 * @param[out]	phMessageTable	Will receive a handle
								to the message table.
 *
 * @returns NTSTATUS
 */
NTSTATUS
MESSAGETABLE_Create(
	_Out_	PHMESSAGETABLE	phMessageTable
);

/**
 * Destroys a message table.
 *
 * @param[in]	hMessageTable	Message table to destroy.
 */
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
 *
 * @returns NTSTATUS
 *
 * @remark	If an entry with the same ID already exists,
 *			it is overwritten.
 */
NTSTATUS
MESSAGETABLE_InsertAnsi(
	_In_	HMESSAGETABLE	hMessageTable,
	_In_	ULONG			nEntryId,
	_In_	PCANSI_STRING	psString
);

/**
 * Inserts a Unicode string into the message table.
 *
 * @param[in]	hMessageTable	Message table to insert into.
 * @param[in]	nEntryId		ID of the string to insert.
 * @param[in]	pusString		String to insert.
 *
 * @returns NTSTATUS
 *
 * @remark	If an entry with the same ID already exists,
 *			it is overwritten.
 */
NTSTATUS
MESSAGETABLE_InsertUnicode(
	_In_	HMESSAGETABLE		hMessageTable,
	_In_	ULONG				nEntryId,
	_In_	PCUNICODE_STRING	pusString
);
