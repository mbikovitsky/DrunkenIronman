#pragma once

#include "Precomp.h"


#define HEAPALLOC(cbSize) HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (cbSize))
#define HEAPFREE(pvMemory) HeapFree(GetProcessHeap(), 0, (pvMemory))

#define CLOSE_TO_VALUE(object, pfnDestructor, value)	\
	if ((value) != (object))							\
	{													\
		(VOID)(pfnDestructor)(object);					\
		(object) = (value);								\
	}
#define CLOSE(object, pfnDestructor) CLOSE_TO_VALUE((object), (pfnDestructor), NULL)
#define CLOSE_HANDLE(hObject) CLOSE((hObject), CloseHandle)
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
