/**
 * @file Match.h
 * @author biko
 * @date 2021-08-20
 *
 * Binary pattern matching.
 */
#pragma once


/** Headers *************************************************************/
#include <ntifs.h>


/** Macros **************************************************************/

/**
 * @brief Shorthand for a pattern element that matches a byte value exactly.
 *
 * @param cValue Value the byte should be equal to.
 */
#define MATCH_EXACT(cValue) { (cValue), 0xFF }

/**
 * @brief Shorthand for a pattern element that matches any byte.
 */
#define MATCH_ANY { 0, 0 }


/** Typedefs ************************************************************/

/**
 * @brief A single pattern matching element.
 *
 * A byte matches an element if cMask & BYTE == cValue.
*/
typedef struct _PATTERN_ELEMENT
{
	UCHAR	cValue;
	UCHAR	cMask;
} PATTERN_ELEMENT, *PPATTERN_ELEMENT;
typedef PATTERN_ELEMENT CONST *PCPATTERN_ELEMENT;


/** Functions ***********************************************************/

/**
 * @brief Checks whether a given sequence of bytes matches a pattern.
 *
 * @param[in]	ptPattern	Pattern to match against.
 * @param[in]	pcBytes		Bytes to test.
 * @param[in]	cbSize		Length of the byte sequence and pattern array.
 * @param[out]	pbMatch		Will indicate whether the bytes match the pattern.
 *
 * @return NTSTATUS
*/
NTSTATUS
MATCH_IsMatch(
	_In_reads_(cbSize)			PCPATTERN_ELEMENT	ptPattern,
	_In_reads_bytes_(cbSize)	UCHAR CONST *		pcBytes,
	_In_						SIZE_T				cbSize,
	_Out_						PBOOLEAN			pbMatch
);
