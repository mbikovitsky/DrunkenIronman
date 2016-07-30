/**
 * @file DumpParse.cpp
 * @author biko
 * @date 2016-07-30
 *
 * DumpParse module implementation.
 */

/** Headers *************************************************************/
#include "Precomp.h"
#include <DbgEng.h>

#include "Util.h"

#include "DumpParse.h"


/** Typedefs ************************************************************/

typedef struct _DUMP_FILE_CONTEXT
{
	IDebugClient4 *	piDebugClient;
} DUMP_FILE_CONTEXT, *PDUMP_FILE_CONTEXT;
typedef CONST DUMP_FILE_CONTEXT *PCDUMP_FILE_CONTEXT;


/** Functions ***********************************************************/

HRESULT
DUMPPARSE_Open(
	_In_opt_	PCWSTR	pwszPath,
	_Out_		PHDUMP	phDump
)
{
	HRESULT				hrResult			= E_FAIL;
	PDUMP_FILE_CONTEXT	ptContext			= NULL;
	IDebugClient4 *		piDebugClient		= NULL;
	DWORD				eType				= REG_NONE;
	DWORD				cbSystemDumpFile	= 0;
	PWSTR				pwszSystemDumpFile	= NULL;
	PWSTR				pwszExpandedPath	= NULL;

	if (NULL == phDump)
	{
		hrResult = E_INVALIDARG;
		goto lblCleanup;
	}

	ptContext = HEAPALLOC(sizeof(*ptContext));
	if (NULL == ptContext)
	{
		hrResult = E_OUTOFMEMORY;
		goto lblCleanup;
	}

	hrResult = DebugCreate(&IID_IDebugClient4, &piDebugClient);
	if (FAILED(hrResult))
	{
		goto lblCleanup;
	}

	if (NULL == pwszPath)
	{
		hrResult = UTIL_RegGetValue(HKEY_LOCAL_MACHINE,
									L"SYSTEM\\CurrentControlSet\\Control\\CrashControl",
									L"DumpFile",
									&pwszSystemDumpFile,
									&cbSystemDumpFile,
									&eType);
		if (FAILED(hrResult))
		{
			goto lblCleanup;
		}
		if ((REG_SZ != eType) && (REG_EXPAND_SZ != eType))
		{
			hrResult = HRESULT_FROM_WIN32(ERROR_INVALID_DATATYPE);
			goto lblCleanup;
		}

		pwszPath = pwszSystemDumpFile;
	}

	hrResult = UTIL_ExpandEnvironmentStrings(pwszPath, &pwszExpandedPath);
	if (FAILED(hrResult))
	{
		goto lblCleanup;
	}

	hrResult = piDebugClient->lpVtbl->OpenDumpFileWide(piDebugClient,
													   pwszExpandedPath,
													   0);
	if (FAILED(hrResult))
	{
		goto lblCleanup;
	}

	// Transfer ownership:
	ptContext->piDebugClient = piDebugClient;
	piDebugClient = NULL;
	*phDump = (HDUMP)ptContext;
	ptContext = NULL;

	hrResult = S_OK;

lblCleanup:
	HEAPFREE(pwszExpandedPath);
	HEAPFREE(pwszSystemDumpFile);
	RELEASE(piDebugClient);
	HEAPFREE(ptContext);

	return hrResult;
}

VOID
DUMPPARSE_Close(
	_In_	HDUMP	hDump
)
{
	PDUMP_FILE_CONTEXT	ptContext	= (PDUMP_FILE_CONTEXT)hDump;

	if (NULL == hDump)
	{
		goto lblCleanup;
	}

	RELEASE(ptContext->piDebugClient);
	HEAPFREE(ptContext);

lblCleanup:
	return;
}
