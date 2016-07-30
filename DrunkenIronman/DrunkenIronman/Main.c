/**
 * @file Main.c
 * @author biko
 * @date 2016-07-30
 *
 * The application's entry-point.
 */

/** Headers *************************************************************/
#include "Precomp.h"
#include <assert.h>
#include <string.h>

#include <Drink.h>

#include "Util.h"
#include "DumpParse.h"

#include "Main_Internal.h"


/** Globals *************************************************************/

/**
 * Array of the registered subfunction handlers.
 */
STATIC CONST SUBFUNCTION_HANDLER_ENTRY g_atSubfunctionHandlers[] = {
	{
		L"convert",
		&main_HandleConvert
	},
};


/** Functions ***********************************************************/

STATIC
HRESULT
main_HandleConvert(
	_In_					INT				nArguments,
	_In_reads_(nArguments)	CONST PCWSTR *	ppwszArguments
)
{
	HRESULT		hrResult		= E_FAIL;
	PCWSTR		pwszDumpPath	= NULL;
	PCWSTR		pwszOutputPath	= NULL;
	HDUMP		hDump			= NULL;
	PVGA_DUMP	ptDump			= NULL;
	DWORD		cbDump			= 0;

	assert(0 != nArguments);
	assert(NULL != ppwszArguments);

	switch (nArguments)
	{
	case SUBFUNCTION_CONVERT_NO_INPUT_ARGS_COUNT:
		pwszOutputPath = ppwszArguments[SUBFUNCTION_CONVERT_NO_INPUT_ARG_OUTPUT];
		break;

	case SUBFUNCTION_CONVERT_ARGS_COUNT:
		pwszDumpPath = ppwszArguments[SUBFUNCTION_CONVERT_ARG_INPUT];
		pwszOutputPath = ppwszArguments[SUBFUNCTION_CONVERT_ARG_OUTPUT];
		break;

	default:
		hrResult = E_INVALIDARG;
		goto lblCleanup;
	}

	hrResult = DUMPPARSE_Open(pwszDumpPath, &hDump);
	if (FAILED(hrResult))
	{
		goto lblCleanup;
	}

	hrResult = DUMPPARSE_ReadTagged(hDump,
									&g_tVgaDumpGuid,
									&ptDump,
									&cbDump);
	if (FAILED(hrResult))
	{
		goto lblCleanup;
	}
	if (sizeof(*ptDump) != cbDump)
	{
		hrResult = HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
		goto lblCleanup;
	}

	hrResult = S_OK;

lblCleanup:
	HEAPFREE(ptDump);
	CLOSE(hDump, DUMPPARSE_Close);

	return hrResult;
}

/**
 * The application's entry-point.
 *
 * @returns INT (cast from HRESULT)
 */
INT
wmain(
	_In_					INT				nArguments,
	_In_reads_(nArguments)	CONST PCWSTR *	ppwszArguments
)
{
	HRESULT						hrResult		= E_FAIL;
	BOOL						bWow64Process	= FALSE;
	DWORD						nIndex			= 0;
	PCSUBFUNCTION_HANDLER_ENTRY	ptCurrentEntry	= NULL;
	PFN_SUBFUNCTION_HANDLER		pfnHandler		= NULL;

	assert(0 != nArguments);
	assert(NULL != ppwszArguments);

	hrResult = UTIL_IsWow64Process(&bWow64Process);
	if (FAILED(hrResult))
	{
		goto lblCleanup;
	}
	if (bWow64Process)
	{
		hrResult = HRESULT_FROM_WIN32(ERROR_WOW_ASSERTION);
		goto lblCleanup;
	}

	if (CMD_ARGS_COUNT > nArguments)
	{
		hrResult = E_INVALIDARG;
		goto lblCleanup;
	}

	for (nIndex = 0;
		 nIndex < ARRAYSIZE(g_atSubfunctionHandlers);
		 ++nIndex)
	{
		ptCurrentEntry = &(g_atSubfunctionHandlers[nIndex]);

		if (0 == _wcsicmp(ptCurrentEntry->pwszSubfunctionName,
						  ppwszArguments[CMD_ARG_SUBFUNCTION]))
		{
			pfnHandler = ptCurrentEntry->pfnHandler;
			break;
		}
	}
	if (NULL == pfnHandler)
	{
		hrResult = E_INVALIDARG;
		goto lblCleanup;
	}

	hrResult = pfnHandler(nArguments, ppwszArguments);

	// Keep last status

lblCleanup:
	return (INT)hrResult;
}
