/**
 * @file Match.c
 * @author biko
 * @date 2021-08-20
 *
 * Binary pattern matching - implementation.
 */

/** Headers *************************************************************/
#include <ntifs.h>

#include <Common.h>

#include "Match.h"


/** Functions ***********************************************************/

/**
 * @brief Checks whether a given byte matches a pattern.
 *
 * @param[in] ptElement Element to match against.
 * @param[in] cByte		Byte to test.
 *
 * @return BOOLEAN
*/
STATIC
BOOLEAN
match_IsByteMatch(
	_In_	PCPATTERN_ELEMENT	ptElement,
	_In_	UCHAR				cByte
)
{
	NT_ASSERT(NULL != ptElement);

	return (ptElement->cMask & cByte) == ptElement->cValue;
}

_Use_decl_annotations_
NTSTATUS
MATCH_IsMatch(
	PCPATTERN_ELEMENT	ptPattern,
	UCHAR CONST *		pcBytes,
	SIZE_T				cbSize,
	PBOOLEAN			pbMatch
)
{
	NTSTATUS	eStatus	= STATUS_UNSUCCESSFUL;
	SIZE_T		nIndex	= 0;
	BOOLEAN		bMatch	= FALSE;

	if (NULL == pbMatch)
	{
		eStatus = STATUS_INVALID_PARAMETER;
		goto lblCleanup;
	}

	if (0 == cbSize)
	{
		*pbMatch = TRUE;
		eStatus = STATUS_SUCCESS;
		goto lblCleanup;
	}

	if (NULL == ptPattern || NULL == pcBytes)
	{
		eStatus = STATUS_INVALID_PARAMETER;
		goto lblCleanup;
	}

	for (nIndex = 0; nIndex < cbSize; ++nIndex)
	{
		bMatch = match_IsByteMatch(&ptPattern[nIndex], pcBytes[nIndex]);
		if (!bMatch)
		{
			break;
		}
	}

	*pbMatch = bMatch;

	eStatus = STATUS_SUCCESS;

lblCleanup:
	return eStatus;
}
