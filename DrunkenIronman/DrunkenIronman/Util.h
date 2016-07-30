/**
 * @file Util.h
 * @author biko
 * @date 2016-07-30
 *
 * Miscellaneous utilities
 */
#pragma once

/** Headers *************************************************************/
#include "Precomp.h"


/** Macros **************************************************************/

/**
 * Allocates memory from the process heap.
 */
#define HEAPALLOC(cbSize) HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (cbSize))

/**
 * Frees memory allocated from the process heap.
 */
#define HEAPFREE(pvMemory) HeapFree(GetProcessHeap(), 0, (pvMemory))

/**
 * Closes an object using a destructor function,
 * then resets the object to the given invalid value.
 */
#define CLOSE_TO_VALUE(object, pfnDestructor, value)	\
	if ((value) != (object))							\
	{													\
		(VOID)(pfnDestructor)(object);					\
		(object) = (value);								\
	}

/**
 * Closes an object using a destructor function,
 * then resets it to NULL.
 */
#define CLOSE(object, pfnDestructor) CLOSE_TO_VALUE((object), (pfnDestructor), NULL)

/**
 * Closes a kernel handle and resets it to NULL.
 */
#define CLOSE_HANDLE(hObject) CLOSE((hObject), CloseHandle)

/**
 * Closes a kernel handle and resets it to INVALID_HANDLE_VALUE.
 */
#define CLOSE_FILE_HANDLE(hFile) CLOSE_TO_VALUE((hFile), CloseHandle, INVALID_HANDLE_VALUE)


HRESULT
UTIL_IsWow64Process(
	_Out_	PBOOL	pbWow64Process
);

HRESULT
UTIL_ReadResource(
	_In_										HMODULE	hModule,
	_In_										PCTSTR	pszResourceName,
	_In_										PCTSTR	pszResourceType,
	_In_										WORD	eLanguage,
	_Outptr_result_bytebuffer_(*pcbResource)	PVOID *	ppvResource,
	_Out_										PDWORD	pcbResource
);

HRESULT
UTIL_ReadResourceFromFile(
	_In_										PCTSTR	pszResourceModulePath,
	_In_										PCTSTR	pszResourceName,
	_In_										PCTSTR	pszResourceType,
	_In_										WORD	eLanguage,
	_Outptr_result_bytebuffer_(*pcbResource)	PVOID *	ppvResource,
	_Out_										PDWORD	pcbResource
);

HRESULT
UTIL_QuerySystemInformation(
	_In_												SYSTEM_INFORMATION_CLASS	eSystemInformationClass,
	_Outptr_result_bytebuffer_(*pcbSystemInformation)	PVOID *						ppvSystemInformation,
	_Out_												PDWORD						pcbSystemInformation
);

HRESULT
UTIL_DuplicateStringAnsiToUnicode(
	_In_		PCSTR	pszSource,
	_Outptr_	PWSTR *	ppwszDestination
);

HRESULT
UTIL_NtCreateFile(
	_In_		PCWSTR		pwszPath,
	_In_		ACCESS_MASK	eDesiredAccess,
	_In_		DWORD		fFileAttributes,
	_In_		DWORD		fShareAccess,
	_In_		DWORD		eCreateDisposition,
	_In_		DWORD		fCreateOptions,
	_Out_		PHANDLE		phFile
);

/**
 * Retrieves a registry value. Strings are always terminated.
 *
 * @param[in]	hKey		Registry key to retrieve the value from.
 * @param[in]	pwszSubkey	Optional subkey.
 * @param[in]	pwszValue	Value to retrieve. If NULL, the default value is returned.
 * @param[out]	ppvData		Will receive the value's data.
 * @param[out]	pcbData		Will receive the data size.
 * @param[out]	peType		Will receive the data's type.
 *
 * @returns HRESULT
 *
 * @remark Empty values are treated the same way as nonexistent values.
 * @remark Strings are always returned as Unicode.
 * @remark Free the returned data to the process heap.
 */
HRESULT
UTIL_RegGetValue(
	_In_									HKEY	hKey,
	_In_opt_								PCWSTR	pwszSubkey,
	_In_opt_								PCWSTR	pwszValue,
	_Outptr_result_bytebuffer_(*pcbData)	PVOID *	ppvData,
	_Out_									PDWORD	pcbData,
	_Out_									PDWORD	peType
);

/**
 * Expands environment variables in a string.
 *
 * @param[in]	pwszSource		String to process.
 * @param[out]	ppwszExpanded	Will receive the expanded string.
 *
 * @returns HRESULT
 *
 * @remark Free the returned string to the process heap.
 */
HRESULT
UTIL_ExpandEnvironmentStrings(
	_In_		PCWSTR	pwszSource,
	_Outptr_	PWSTR *	ppwszExpanded
);
