/**
 * @file Debug.c
 * @author biko
 * @date 2016-08-13
 *
 * Debug module implementation.
 */

/** Headers *************************************************************/
#include <Windows.h>

#include <stdio.h>
#include <stdarg.h>

#include "Util.h"

#include "Debug.h"


/** Globals *************************************************************/

/**
 * Synchronizes debugging output.
 */
STATIC HANDLE g_hDebugMutex = NULL;


/** Functions ***********************************************************/

HRESULT
DEBUG_Init(VOID)
{
	HRESULT	hrResult	= E_FAIL;
	HANDLE	hDebugMutex	= NULL;

	hDebugMutex = CreateMutexW(NULL, FALSE, NULL);
	if (NULL == hDebugMutex)
	{
		hrResult = HRESULT_FROM_WIN32(GetLastError());
		goto lblCleanup;
	}

	// Transfer ownership:
	g_hDebugMutex = hDebugMutex;
	hDebugMutex = NULL;

	hrResult = S_OK;

lblCleanup:
	CLOSE_HANDLE(hDebugMutex);
	return hrResult;
}

VOID
DEBUG_Shutdown(VOID)
{
	CLOSE_HANDLE(g_hDebugMutex);
}

VOID
DEBUG_Progress(
	_In_	PCSTR	pszFunction,
	_In_	PCSTR	pszFormat,
	...
)
{
	va_list	vaArgs				= { 0 };
	BOOL	bVaListInitialized	= FALSE;
	BOOL	bLockAcquired		= FALSE;

	va_start(vaArgs, pszFormat);
	bVaListInitialized = TRUE;

	// Synchronize with other debugging output.
	switch (WaitForSingleObject(g_hDebugMutex, INFINITE))
	{
	case WAIT_ABANDONED:
		__fallthrough;
	case WAIT_OBJECT_0:
		break;

	default:
		// Huh?
		goto lblCleanup;
	}
	bLockAcquired = TRUE;

	//
	// Output the message.
	//

	(VOID)fprintf(stderr,
				  "[*] %-20.20s - ",
				  pszFunction);

	(VOID)vfprintf(stderr,
				   pszFormat,
				   vaArgs);

	(VOID)fputs("\n", stderr);

lblCleanup:
	if (bLockAcquired)
	{
		(VOID)ReleaseMutex(g_hDebugMutex);
		bLockAcquired = FALSE;
	}
	if (bVaListInitialized)
	{
		va_end(vaArgs);
		bVaListInitialized = TRUE;
	}
}
