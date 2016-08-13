/**
 * @file Carpenter.h
 * @author biko
 * @date 2016-08-06
 *
 * Carpenter module public header.
 * Contains routines for patching message tables.
 */
#pragma once

/** Headers *************************************************************/
#include <ntifs.h>

#include "Util.h"


/** Constants ***********************************************************/

/**
 * The default type of a message table resource.
 */
#define RT_MESSAGETABLE ((ULONG_PTR)(USHORT)11)


/** Typedefs ************************************************************/

/**
 * Handle to a patcher instance.
 */
DECLARE_HANDLE(HCARPENTER);
typedef HCARPENTER *PHCARPENTER;


/** Functions ***********************************************************/

/**
 * Creates a new patcher instance.
 *
 * @param[in]	pvImageBase	Base of the image that contains
 *							the message table.
 * @param[in]	pvType		Type of the resource that contains
 *							the message table. Usually, this is
 *							RT_MESSAGETABLE.
 * @param[in]	pvName		Name of the resource that contains
 *							the message table.
 * @param[in]	pvLanguage	Language of the resource that
 *							contains the message table.
 * @param[out]	phCarpenter	Will receive a patcher handle.
 *
 * @remark	The resource type, name and language can be
 *			either a numerical ID (USHORT-sized),
 *			or a pointer to a null-terminated Unicode string.
 *
 * @returns NTSTATUS
 */
_IRQL_requires_(PASSIVE_LEVEL)
PAGEABLE
NTSTATUS
CARPENTER_Create(
	_In_	PVOID		pvImageBase,
	_In_	ULONG_PTR	pvType,
	_In_	ULONG_PTR	pvName,
	_In_	ULONG_PTR	pvLanguage,
	_Out_	PHCARPENTER	phCarpenter
);

/**
 * Destroys a patcher instance.
 *
 * @param[in]	hCarpenter	Patcher to destroy.
 *
 * @remark	Destroying the patcher does not
 *			undo the patch!
 */
_IRQL_requires_(PASSIVE_LEVEL)
PAGEABLE
VOID
CARPENTER_Destroy(
	_In_	HCARPENTER	hCarpenter
);

/**
 * Stages a message for the patch.
 *
 * @param[in]	hCarpenter	A patcher instance.
 * @param[in]	nMessageId	The ID for the message being staged.
 * @param[in]	psMessage	The message string.
 *
 * @returns NTSTATUS
 *
 * @remark	The staged message overwrites
 *			any previous message with the
 *			same ID.
 */
_IRQL_requires_(PASSIVE_LEVEL)
PAGEABLE
NTSTATUS
CARPENTER_StageMessage(
	_In_	HCARPENTER		hCarpenter,
	_In_	ULONG			nMessageId,
	_In_	PCANSI_STRING	psMessage
);

/**
 * Applies the prepared patch to the message table.
 *
 * @param[in]	hCarpenter	A patcher instance.
 *
 * @returns NTSTATUS
 *
 * @remark	If the function fails, the patch
 *			is not applied.
 */
_IRQL_requires_(PASSIVE_LEVEL)
PAGEABLE
NTSTATUS
CARPENTER_ApplyPatch(
	_In_	HCARPENTER	hCarpenter
);
