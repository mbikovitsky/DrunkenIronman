/**
 * @file DumpParse.cpp
 * @author biko
 * @date 2016-07-30
 *
 * DumpParse module implementation.
 */

/** Headers *************************************************************/
#include <Windows.h>
#include <DbgEng.h>

#include "Util.h"
#include "Debug.h"

#include "DumpParse.h"


/** Typedefs ************************************************************/

typedef struct _DUMP_FILE_CONTEXT
{
	IDebugClient *	piDebugClient;
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
	IDebugClient *		piDebugClient		= NULL;
	DWORD				eType				= REG_NONE;
	DWORD				cbSystemDumpFile	= 0;
	PWSTR				pwszSystemDumpFile	= NULL;
	PWSTR				pwszExpandedPath	= NULL;
	PSTR				pszExpandedPath		= NULL;

	if (NULL == phDump)
	{
		PROGRESS("Invalid arguments specified.");
		hrResult = E_INVALIDARG;
		goto lblCleanup;
	}

	PROGRESS("Opening dump file '%S'.", pwszPath);

	ptContext = HEAPALLOC(sizeof(*ptContext));
	if (NULL == ptContext)
	{
		PROGRESS("Oops. Ran out of memory.");
		hrResult = E_OUTOFMEMORY;
		goto lblCleanup;
	}

	hrResult = DebugCreate(&IID_IDebugClient, &piDebugClient);
	if (FAILED(hrResult))
	{
		PROGRESS("Failed obtaining the IDebugClient4.");
		goto lblCleanup;
	}

	if (NULL == pwszPath)
	{
		PROGRESS("NULL path specified. Obtaining the path to the system dump file.");

		hrResult = UTIL_RegGetValue(HKEY_LOCAL_MACHINE,
									L"SYSTEM\\CurrentControlSet\\Control\\CrashControl",
									L"DumpFile",
									&pwszSystemDumpFile,
									&cbSystemDumpFile,
									&eType);
		if (FAILED(hrResult))
		{
			PROGRESS("Failed obtaining the path to the system dump file.");
			goto lblCleanup;
		}
		if ((REG_SZ != eType) && (REG_EXPAND_SZ != eType))
		{
			PROGRESS("Failed obtaining the path to the system dump file. Incorrect data format.");
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

	hrResult = UTIL_DuplicateStringUnicodeToAnsi(pwszExpandedPath, &pszExpandedPath);
	if (FAILED(hrResult))
	{
		goto lblCleanup;
	}

	hrResult = piDebugClient->lpVtbl->OpenDumpFile(piDebugClient, pszExpandedPath);
	if (FAILED(hrResult))
	{
		PROGRESS("Failed opening the dump file.");
		goto lblCleanup;
	}

	// Transfer ownership:
	ptContext->piDebugClient = piDebugClient;
	piDebugClient = NULL;
	*phDump = (HDUMP)ptContext;
	ptContext = NULL;

	hrResult = S_OK;

lblCleanup:
	HEAPFREE(pszExpandedPath);
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

HRESULT
DUMPPARSE_ReadTagged(
	_In_									HDUMP	hDump,
	_In_									LPCGUID	ptTag,
	_Outptr_result_bytebuffer_(*pcbData)	PVOID *	ppvData,
	_Out_									PDWORD	pcbData
)
{
	HRESULT				hrResult			= E_FAIL;
	PDUMP_FILE_CONTEXT	ptContext			= (PDUMP_FILE_CONTEXT)hDump;
	IDebugClient *		piDebugClient		= NULL;
	IDebugDataSpaces3 *	piDebugDataSpaces	= NULL;
	ULONG				cbData				= 0;
	PVOID				pvData				= NULL;

	C_ASSERT(sizeof(*pcbData) == sizeof(cbData));

	if ((NULL == hDump) ||
		(NULL == ptTag) ||
		(NULL == ppvData) ||
		(NULL == pcbData))
	{
		PROGRESS("Invalid arguments specified.");
		hrResult = E_INVALIDARG;
		goto lblCleanup;
	}

	piDebugClient = ptContext->piDebugClient;

	hrResult = piDebugClient->lpVtbl->QueryInterface(piDebugClient,
													 &IID_IDebugDataSpaces3,
													 &piDebugDataSpaces);
	if (FAILED(hrResult))
	{
		PROGRESS("Failed obtaining the IDebugDataSpaces3 interface.");
		goto lblCleanup;
	}

	hrResult = piDebugDataSpaces->lpVtbl->ReadTagged(piDebugDataSpaces,
													 (LPGUID)ptTag,
													 0,
													 NULL, 0,
													 &cbData);
	if (FAILED(hrResult))
	{
		PROGRESS("Failed reading the tagged data. Is it even there?");
		goto lblCleanup;
	}

	pvData = HEAPALLOC(cbData);
	if (NULL == pvData)
	{
		PROGRESS("Oops. Ran out of memory.");
		hrResult = E_OUTOFMEMORY;
		goto lblCleanup;
	}

	hrResult = piDebugDataSpaces->lpVtbl->ReadTagged(piDebugDataSpaces,
													 (LPGUID)ptTag,
													 0,
													 pvData, cbData,
													 NULL);
	if (FAILED(hrResult))
	{
		PROGRESS("Failed reading the tagged data. Is it even there?");
		goto lblCleanup;
	}

	// Transfer ownership:
	*ppvData = pvData;
	pvData = NULL;
	*pcbData = cbData;

	hrResult = S_OK;

lblCleanup:
	HEAPFREE(pvData);
	RELEASE(piDebugDataSpaces);

	return hrResult;
}
