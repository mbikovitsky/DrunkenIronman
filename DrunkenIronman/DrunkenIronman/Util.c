/**
 * @file Util.c
 * @author biko
 * @date 2016-07-30
 *
 * Miscellaneous utilities - implementation.
 */

/** Headers *************************************************************/
#include "Precomp.h"
#include <strsafe.h>

#include "Util.h"


/** Typedefs ************************************************************/

typedef
BOOL
WINAPI
FN_ISWOW64PROCESS(
	_In_	HANDLE	hProcess,
	_Out_	PBOOL	pbWow64Process
);
typedef FN_ISWOW64PROCESS *PFN_ISWOW64PROCESS;

typedef
NTSTATUS
NTAPI
FN_NTQUERYSYSTEMINFORMATION(
	_In_		SYSTEM_INFORMATION_CLASS	eSystemInformationClass,
	_Inout_		PVOID						pvSystemInformation,
	_In_		ULONG						cbSystemInformation,
	_Out_opt_	PULONG						pcbReturned
);
typedef FN_NTQUERYSYSTEMINFORMATION *PFN_NTQUERYSYSTEMINFORMATION;

typedef
VOID
NTAPI
FN_RTLINITUNICODESTRING(
	_Out_		PUNICODE_STRING	pusDestinationString,
	_In_opt_	PCWSTR			pwszSourceString
);
typedef FN_RTLINITUNICODESTRING *PFN_RTLINITUNICODESTRING;

typedef
NTSTATUS
NTAPI
FN_NTCREATEFILE(
	_Out_		PHANDLE				phFile,
	_In_		ACCESS_MASK			fDesiredAccess,
	_In_		POBJECT_ATTRIBUTES	ptObjectAttributes,
	_Out_		PIO_STATUS_BLOCK	ptIoStatusBlock,
	_In_opt_	PLARGE_INTEGER		pcbAllocationSize,
	_In_		ULONG				fFileAttributes,
	_In_		ULONG				fShareAccess,
	_In_		ULONG				eCreateDisposition,
	_In_		ULONG				fCreateOptions,
	_In_		PVOID				pvEaBuffer,
	_In_		ULONG				cbEaBuffer
);
typedef FN_NTCREATEFILE *PFN_NTCREATEFILE;


/** Functions ***********************************************************/

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

HRESULT
UTIL_QuerySystemInformation(
	_In_												SYSTEM_INFORMATION_CLASS	eSystemInformationClass,
	_Outptr_result_bytebuffer_(*pcbSystemInformation)	PVOID *						ppvSystemInformation,
	_Out_												PDWORD						pcbSystemInformation
)
{
	HRESULT							hrResult					= E_FAIL;
	NTSTATUS						eStatus						= STATUS_UNSUCCESSFUL;
	HMODULE							hNtdll						= NULL;
	PFN_NTQUERYSYSTEMINFORMATION	pfnNtQuerySystemInformation	= NULL;
	PVOID							pvSystemInformation			= NULL;
	ULONG							cbReturned					= 0;

	if ((NULL == ppvSystemInformation) ||
		(NULL == pcbSystemInformation))
	{
		hrResult = E_INVALIDARG;
		goto lblCleanup;
	}

	hNtdll = LoadLibrary(_T("ntdll.dll"));
	if (NULL == hNtdll)
	{
		hrResult = HRESULT_FROM_WIN32(GetLastError());
		goto lblCleanup;
	}

	pfnNtQuerySystemInformation = (PFN_NTQUERYSYSTEMINFORMATION)GetProcAddress(hNtdll, "NtQuerySystemInformation");
	if (NULL == pfnNtQuerySystemInformation)
	{
		hrResult = HRESULT_FROM_WIN32(GetLastError());
		goto lblCleanup;
	}

	cbReturned = 1;	// We don't want to perform a zero-length
					// allocation on the first go.
	do
	{
		// Allocate a buffer
		HEAPFREE(pvSystemInformation);
		pvSystemInformation = HEAPALLOC(cbReturned);
		if (NULL == pvSystemInformation)
		{
			hrResult = HRESULT_FROM_WIN32(GetLastError());
			goto lblCleanup;
		}

		eStatus = pfnNtQuerySystemInformation(eSystemInformationClass,
											  pvSystemInformation,
											  cbReturned,
											  &cbReturned);
	} while (STATUS_INFO_LENGTH_MISMATCH == eStatus);
	if (!NT_SUCCESS(eStatus))
	{
		goto lblCleanup;
	}

	// Transfer ownership:
	*ppvSystemInformation = pvSystemInformation;
	pvSystemInformation = NULL;
	*pcbSystemInformation = cbReturned;

	hrResult = S_OK;

lblCleanup:
	HEAPFREE(pvSystemInformation);
	CLOSE(hNtdll, FreeLibrary);

	return hrResult;
}

HRESULT
UTIL_DuplicateStringAnsiToUnicode(
	_In_		PCSTR	pszSource,
	_Outptr_	PWSTR *	ppwszDestination
)
{
	HRESULT	hrResult		= E_FAIL;
	INT		cchDestination	= 0;
	PWSTR	pwszDestination	= NULL;

	if ((NULL == pszSource) ||
		(NULL == ppwszDestination))
	{
		hrResult = E_INVALIDARG;
		goto lblCleanup;
	}

	cchDestination = MultiByteToWideChar(CP_ACP,
										 MB_ERR_INVALID_CHARS,
										 pszSource,
										 -1,
										 NULL,
										 0);
	if (0 == cchDestination)
	{
		hrResult = HRESULT_FROM_WIN32(GetLastError());
		goto lblCleanup;
	}

	pwszDestination = HEAPALLOC(cchDestination * sizeof(*pwszDestination));
	if (NULL == pwszDestination)
	{
		hrResult = HRESULT_FROM_WIN32(GetLastError());
		goto lblCleanup;
	}

	if (cchDestination != MultiByteToWideChar(CP_ACP,
											  MB_ERR_INVALID_CHARS,
											  pszSource,
											  -1,
											  pwszDestination,
											  cchDestination))
	{
		hrResult = HRESULT_FROM_WIN32(GetLastError());
		goto lblCleanup;
	}

	// Transfer ownership:
	*ppwszDestination = pwszDestination;
	pwszDestination = NULL;

	hrResult = S_OK;

lblCleanup:
	HEAPFREE(pwszDestination);

	return hrResult;
}

HRESULT
UTIL_NtCreateFile(
	_In_		PCWSTR		pwszPath,
	_In_		ACCESS_MASK	eDesiredAccess,
	_In_		DWORD		fFileAttributes,
	_In_		DWORD		fShareAccess,
	_In_		DWORD		eCreateDisposition,
	_In_		DWORD		fCreateOptions,
	_Out_		PHANDLE		phFile
)
{
	HRESULT						hrResult				= E_FAIL;
	NTSTATUS					eStatus					= STATUS_UNSUCCESSFUL;
	HMODULE						hNtdll					= NULL;
	PFN_RTLINITUNICODESTRING	pfnRtlInitUnicodeString	= NULL;
	PFN_NTCREATEFILE			pfnNtCreateFile			= NULL;
	UNICODE_STRING				usPath					= { 0 };
	OBJECT_ATTRIBUTES			tAttributes				= { 0 };
	IO_STATUS_BLOCK				tStatusBlock			= { 0 };
	HANDLE						hFile					= INVALID_HANDLE_VALUE;

	if ((NULL == pwszPath) ||
		(NULL == phFile))
	{
		hrResult = E_INVALIDARG;
		goto lblCleanup;
	}

	hNtdll = LoadLibrary(_T("ntdll.dll"));
	if (NULL == hNtdll)
	{
		hrResult = HRESULT_FROM_WIN32(GetLastError());
		goto lblCleanup;
	}

	pfnRtlInitUnicodeString = (PFN_RTLINITUNICODESTRING)GetProcAddress(hNtdll, "RtlInitUnicodeString");
	if (NULL == pfnRtlInitUnicodeString)
	{
		hrResult = HRESULT_FROM_WIN32(GetLastError());
		goto lblCleanup;
	}

	pfnNtCreateFile = (PFN_NTCREATEFILE)GetProcAddress(hNtdll, "NtCreateFile");
	if (NULL == pfnNtCreateFile)
	{
		hrResult = HRESULT_FROM_WIN32(GetLastError());
		goto lblCleanup;
	}

	pfnRtlInitUnicodeString(&usPath, pwszPath);
	InitializeObjectAttributes(&tAttributes, &usPath, 0, NULL, NULL);
	eStatus = pfnNtCreateFile(&hFile,
							  eDesiredAccess,
							  &tAttributes,
							  &tStatusBlock,
							  NULL,
							  fFileAttributes,
							  fShareAccess,
							  eCreateDisposition,
							  fCreateOptions,
							  NULL,
							  0);
	if (!NT_SUCCESS(eStatus))
	{
		hrResult = HRESULT_FROM_NT(eStatus);
		goto lblCleanup;
	}

	// Transfer ownership:
	*phFile = hFile;
	hFile = INVALID_HANDLE_VALUE;

	hrResult = S_OK;

lblCleanup:
	CLOSE_FILE_HANDLE(hFile);
	CLOSE(hNtdll, FreeLibrary);

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
	// Returns the file's path to the caller.
	//

	cchTempPath = wcslen(wszTempPath);
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
