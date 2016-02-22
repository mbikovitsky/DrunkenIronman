#include "Precomp.h"
#include "Util.h"


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
