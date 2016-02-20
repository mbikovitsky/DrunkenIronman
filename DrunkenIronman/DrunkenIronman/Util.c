#include <windows.h>
#include <tchar.h>

#include "Util.h"


typedef
BOOL
WINAPI
FN_ISWOW64PROCESS(
	_In_	HANDLE	hProcess,
	_Out_	PBOOL	pbWow64Process
);
typedef FN_ISWOW64PROCESS *PFN_ISWOW64PROCESS;


HRESULT
UTIL_IsWow64Process(
	_Out_	PBOOL	pbWow64Process
)
{
	HRESULT				hrResult			= E_FAIL;
	BOOL				bWow64Process		= FALSE;
	HMODULE				hKernel32			= NULL;
	PFN_ISWOW64PROCESS	pfnIsWow64Process	= NULL;

	if (NULL == pbWow64Process)
	{
		hrResult = E_INVALIDARG;
		goto lblCleanup;
	}

	hKernel32 = GetModuleHandle(_T("kernel32.dll"));
	if (NULL == hKernel32)
	{
		hrResult = HRESULT_FROM_WIN32(GetLastError());
		goto lblCleanup;
	}

	pfnIsWow64Process = (PFN_ISWOW64PROCESS)GetProcAddress(hKernel32, "IsWow64Process");
	if (NULL != pfnIsWow64Process)
	{
		if (!pfnIsWow64Process(GetCurrentProcess(), &bWow64Process))
		{
			hrResult = HRESULT_FROM_WIN32(GetLastError());
			goto lblCleanup;
		}
	}

	// Return results
	*pbWow64Process = bWow64Process;

	hrResult = S_OK;

lblCleanup:
	return hrResult;
}

HRESULT
UTIL_ReadResource(
	_In_										HMODULE	hModule,
	_In_										PCTSTR	pszResourceName,
	_In_										PCTSTR	pszResourceType,
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
UTIL_ReadResourceFromFile(
	_In_										PCTSTR	pszResourceModulePath,
	_In_										PCTSTR	pszResourceName,
	_In_										PCTSTR	pszResourceType,
	_In_										WORD	eLanguage,
	_Outptr_result_bytebuffer_(*pcbResource)	PVOID *	ppvResource,
	_Out_										PDWORD	pcbResource
)
{
	HRESULT	hrResult		= E_FAIL;
	HMODULE	hResourceModule	= NULL;

	if ((NULL == pszResourceModulePath) ||
		(NULL == ppvResource) ||
		(NULL == pcbResource))
	{
		hrResult = E_INVALIDARG;
		goto lblCleanup;
	}

	hResourceModule = LoadLibraryEx(pszResourceModulePath, NULL, LOAD_LIBRARY_AS_DATAFILE);
	if (NULL == hResourceModule)
	{
		hrResult = HRESULT_FROM_WIN32(GetLastError());
		goto lblCleanup;
	}

	hrResult = UTIL_ReadResource(hResourceModule,
								 pszResourceName,
								 pszResourceType,
								 eLanguage,
								 ppvResource,
								 pcbResource);

	// Keep last status

lblCleanup:
	CLOSE(hResourceModule, FreeLibrary);

	return hrResult;
}
