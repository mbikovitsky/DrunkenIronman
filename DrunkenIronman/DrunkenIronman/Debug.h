/**
 * @file Debug.h
 * @author biko
 * @date 2016-08-13
 *
 * Debug module public header.
 * Contains functions for debugging support.
 */
#pragma once

/** Headers *************************************************************/
#include <Windows.h>


/** Macros **************************************************************/

/**
 * Wrapper for the progress-reporting function.
 *
 * @see DEBUG_Progress.
 */
#define PROGRESS(pszFormat, ...) \
	DEBUG_Progress(__FUNCTION__, (pszFormat), __VA_ARGS__)


/** Functions ***********************************************************/

/**
 * Initializes debugging support.
 *
 * @returns HRESULT
 */
HRESULT
DEBUG_Init(VOID);

/**
 * Terminates debugging support.
 */
VOID
DEBUG_Shutdown(VOID);

/**
 * Reports the progress of the application.
 *
 * @param[in]	pszFunction	Current function.
 * @param[in]	pszFormat	printf-style format string.
 * @param[in]	...			Format string parameters.
 *
 * @remark	Do not use this function directly.
 *			Instead, use the PROGRESS macro.
 */
VOID
DEBUG_Progress(
	_In_	PCSTR	pszFunction,
	_In_	PCSTR	pszFormat,
	...
);
