/**
 * @file Util.c
 * @author biko
 * @date 2016-07-30
 *
 * Miscellaneous user-mode utilities - implementation.
 */

/** Headers *************************************************************/
#include "Precomp.h"
#include <strsafe.h>

#include "Util.h"


/** Functions ***********************************************************/

HRESULT
UTIL_ReadResource(
	_In_										HMODULE	hModule,
	_In_										PCWSTR	pszResourceName,
	_In_										PCWSTR	pszResourceType,
	_In_										WORD	eLanguage,
	_Outptr_result_bytebuffer_(*pcbResource)	PVOID *	ppvResource,
	_Out_										PDWORD	pcbResource
)
{
	HRESULT	hrResult		= E_FAIL;
	HRSRC	hResInfo		= NULL;
	HGLOBAL	hResData		= NULL;
	PVOID	pvResource		= NULL;
	DWORD	cbResource		= 0;
	PVOID	pvResourceCopy	= NULL;

	if ((NULL == hModule) ||
		(NULL == ppvResource) ||
		(NULL == pcbResource))
	{
		hrResult = E_INVALIDARG;
		goto lblCleanup;
	}

	hResInfo = FindResourceEx(hModule, pszResourceType, pszResourceName, eLanguage);
	if (NULL == hResInfo)
	{
		hrResult = HRESULT_FROM_WIN32(GetLastError());
		goto lblCleanup;
	}

	hResData = LoadResource(hModule, hResInfo);
	if (NULL == hResData)
	{
		hrResult = HRESULT_FROM_WIN32(GetLastError());
		goto lblCleanup;
	}

	pvResource = LockResource(hResData);
	if (NULL == pvResource)
	{
		hrResult = HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
		goto lblCleanup;
	}

	cbResource = SizeofResource(hModule, hResInfo);
	if (0 == cbResource)
	{
		hrResult = HRESULT_FROM_WIN32(GetLastError());
		goto lblCleanup;
	}

	pvResourceCopy = HEAPALLOC(cbResource);
	if (NULL == pvResourceCopy)
	{
		hrResult = HRESULT_FROM_WIN32(GetLastError());
		goto lblCleanup;
	}

	MoveMemory(pvResourceCopy, pvResource, cbResource);

	// Transfer ownership:
	*ppvResource = pvResourceCopy;
	pvResourceCopy = NULL;
	*pcbResource = cbResource;

	hrResult = S_OK;

lblCleanup:
	HEAPFREE(pvResourceCopy);

	return hrResult;
}

HRESULT
UTIL_RegGetValue(
	_In_									HKEY	hKey,
	_In_opt_								PCWSTR	pwszSubkey,
	_In_opt_								PCWSTR	pwszValue,
	_Outptr_result_bytebuffer_(*pcbData)	PVOID *	ppvData,
	_Out_									PDWORD	pcbData,
	_Out_									PDWORD	peType
)
{
	HRESULT	hrResult		= E_FAIL;
	LONG	lResult			= ERROR_SUCCESS;
	HKEY	hFinalKey		= NULL;
	BOOL	bCloseFinalKey	= FALSE;
	DWORD	cbData			= 0;
	PVOID	pvData			= NULL;
	DWORD	eType			= REG_NONE;

	if ((NULL == hKey) ||
		(NULL == ppvData) ||
		(NULL == pcbData) ||
		(NULL == peType))
	{
		hrResult = E_INVALIDARG;
		goto lblCleanup;
	}

	if (NULL != pwszSubkey)
	{
		lResult = RegOpenKeyExW(hKey,
								pwszSubkey,
								0,
								KEY_QUERY_VALUE,
								&hFinalKey);
		if (ERROR_SUCCESS != lResult)
		{
			hrResult = HRESULT_FROM_WIN32(lResult);
			goto lblCleanup;
		}
		bCloseFinalKey = TRUE;
	}
	else
	{
		hFinalKey = hKey;
	}

	lResult = RegQueryValueExW(hFinalKey,
							   pwszValue,
							   NULL,
							   NULL,
							   NULL,
							   &cbData);
	if (ERROR_SUCCESS != lResult)
	{
		hrResult = HRESULT_FROM_WIN32(lResult);
		goto lblCleanup;
	}
	if (0 == cbData)
	{
		hrResult = HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
		goto lblCleanup;
	}

	cbData += sizeof(UNICODE_NULL);
	pvData = HEAPALLOC(cbData);
	if (NULL == pvData)
	{
		hrResult = E_OUTOFMEMORY;
		goto lblCleanup;
	}

	lResult = RegQueryValueExW(hFinalKey,
							   pwszValue,
							   NULL,
							   &eType,
							   pvData,
							   &cbData);
	if (ERROR_SUCCESS != lResult)
	{
		hrResult = HRESULT_FROM_WIN32(lResult);
		goto lblCleanup;
	}
	if (0 == cbData)
	{
		hrResult = HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
		goto lblCleanup;
	}

	// Transfer ownership:
	*ppvData = pvData;
	pvData = NULL;
	*pcbData = cbData;
	*peType = eType;

	hrResult = S_OK;

lblCleanup:
	HEAPFREE(pvData);
	if (bCloseFinalKey)
	{
		CLOSE(hFinalKey, RegCloseKey);
		bCloseFinalKey = FALSE;
	}

	return hrResult;
}

HRESULT
UTIL_ExpandEnvironmentStrings(
	_In_		PCWSTR	pwszSource,
	_Outptr_	PWSTR *	ppwszExpanded
)
{
	HRESULT	hrResult		= E_FAIL;
	DWORD	cchExpanded		= 0;
	PWSTR	pwszExpanded	= NULL;

	if ((NULL == pwszSource) ||
		(NULL == ppwszExpanded))
	{
		hrResult = E_INVALIDARG;
		goto lblCleanup;
	}

	cchExpanded = ExpandEnvironmentStringsW(pwszSource, NULL, 0);
	if (0 == cchExpanded)
	{
		hrResult = HRESULT_FROM_WIN32(GetLastError());
		goto lblCleanup;
	}

	pwszExpanded = HEAPALLOC(cchExpanded * sizeof(pwszExpanded[0]));
	if (NULL == pwszExpanded)
	{
		hrResult = E_OUTOFMEMORY;
		goto lblCleanup;
	}

	cchExpanded = ExpandEnvironmentStringsW(pwszSource, pwszExpanded, cchExpanded);
	if (0 == cchExpanded)
	{
		hrResult = HRESULT_FROM_WIN32(GetLastError());
		goto lblCleanup;
	}

	// Transfer ownership:
	*ppwszExpanded = pwszExpanded;
	pwszExpanded = NULL;

	hrResult = S_OK;

lblCleanup:
	HEAPFREE(pwszExpanded);

	return hrResult;
}

HRESULT
UTIL_WriteToTemporaryFile(
	_In_reads_bytes_(cbBuffer)	PVOID	pvBuffer,
	_In_						DWORD	cbBuffer,
	_Outptr_					PWSTR *	ppwzTempPath
)
{
	HRESULT	hrResult						= E_FAIL;
	WCHAR	wszTempDirectory[MAX_PATH + 1]	= { L'\0' };
	WCHAR	wszTempPath[MAX_PATH]			= { L'\0' };
	BOOL	bDeleteFile						= FALSE;
	HANDLE	hTempFile						= INVALID_HANDLE_VALUE;
	DWORD	cbWritten						= 0;
	SIZE_T	cchTempPath						= 0;
	PWSTR	pwszTempPath					= NULL;

	if ((NULL == pvBuffer) ||
		(0 == cbBuffer) ||
		(NULL == ppwzTempPath))
	{
		hrResult = E_INVALIDARG;
		goto lblCleanup;
	}

	//
	// Create a temporary file.
	//

	if (0 == GetTempPathW(ARRAYSIZE(wszTempDirectory),
						  wszTempDirectory))
	{
		hrResult = HRESULT_FROM_WIN32(GetLastError());
		goto lblCleanup;
	}

	if (0 == GetTempFileNameW(wszTempDirectory,
							  L"",
							  0,
							  wszTempPath))
	{
		hrResult = HRESULT_FROM_WIN32(GetLastError());
		goto lblCleanup;
	}
	bDeleteFile = TRUE;

	//
	// Dump the data into the file.
	//

	hTempFile = CreateFileW(wszTempPath,
							GENERIC_WRITE,
							0,
							NULL,
							TRUNCATE_EXISTING,
							FILE_ATTRIBUTE_NORMAL,
							NULL);
	if (INVALID_HANDLE_VALUE == hTempFile)
	{
		hrResult = HRESULT_FROM_WIN32(GetLastError());
		goto lblCleanup;
	}

	if (!WriteFile(hTempFile,
				   pvBuffer,
				   cbBuffer,
				   &cbWritten,
				   NULL))
	{
		hrResult = HRESULT_FROM_WIN32(GetLastError());
		goto lblCleanup;
	}
	if (cbBuffer != cbWritten)
	{
		hrResult = E_UNEXPECTED;
		goto lblCleanup;
	}

	//
	// Return the file's path to the caller.
	//

	cchTempPath = wcslen(wszTempPath) + 1;
	pwszTempPath = HEAPALLOC(cchTempPath * sizeof(pwszTempPath[0]));
	if (NULL == pwszTempPath)
	{
		hrResult = E_OUTOFMEMORY;
		goto lblCleanup;
	}

	hrResult = StringCchCopyW(pwszTempPath, cchTempPath, wszTempPath);
	if (FAILED(hrResult))
	{
		goto lblCleanup;
	}

	// Transfer ownership:
	*ppwzTempPath = pwszTempPath;
	pwszTempPath = NULL;
	bDeleteFile = FALSE;

	hrResult = S_OK;

lblCleanup:
	HEAPFREE(pwszTempPath);
	CLOSE_FILE_HANDLE(hTempFile);
	if (bDeleteFile)
	{
		(VOID)DeleteFileW(wszTempPath);
		bDeleteFile = FALSE;
	}

	return hrResult;
}
