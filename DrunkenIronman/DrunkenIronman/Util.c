#include <windows.h>
#include <tchar.h>


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
