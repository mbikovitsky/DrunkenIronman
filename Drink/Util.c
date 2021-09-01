/**
 * @file Util.c
 * @author biko
 * @date 2016-08-06
 *
 * Miscellaneous kernel-mode utilities - implementation.
 */

/** Headers *************************************************************/
#include <ntifs.h>
#include <ntintsafe.h>
#include <ntstrsafe.h>
#include <aux_klib.h>

#include <Common.h>

#include "Util.h"


/** Constants ***********************************************************/

/**
 * Pool tag for allocations made by this module.
 */
#define UTIL_POOL_TAG (RtlUlongByteSwap('Util'))

/**
 * Number of iterations to use when querying for information.
 */
#define QUERY_INFO_ITERATIONS (10)


/** Forward Declarations ************************************************/

_IRQL_requires_(PASSIVE_LEVEL)
NTSYSAPI
NTSTATUS 
ZwQuerySystemInformation (
    _In_															SYSTEM_INFORMATION_CLASS	SystemInformationClass, 
    _Out_writes_bytes_to_(SystemInformationLength, *ReturnLength)	PVOID						SystemInformation, 
    _In_															ULONG						SystemInformationLength, 
    _Out_opt_														PULONG						ReturnLength
);

/**
 * @brief Returns the contents of an object directory.
 *
 * @see https://docs.microsoft.com/en-us/windows/win32/devnotes/ntquerydirectoryobject
*/
_IRQL_requires_(PASSIVE_LEVEL)
NTSTATUS
NTSYSAPI
NTAPI
ZwQueryDirectoryObject(
	_In_		HANDLE	DirectoryHandle,
	_Out_opt_	PVOID	Buffer,
	_In_		ULONG	Length,
	_In_		BOOLEAN	ReturnSingleEntry,
	_In_		BOOLEAN	RestartScan,
	_Inout_		PULONG	Context,
	_Out_opt_	PULONG	ReturnLength
);


/** Functions ***********************************************************/

_Use_decl_annotations_
PAGEABLE
BOOLEAN
UTIL_IsWindows10OrGreater(VOID)
{
	NTSTATUS				eStatus			= STATUS_UNSUCCESSFUL;
	RTL_OSVERSIONINFOEXW	tVersionInfo	= { 0 };
	ULONGLONG				fConditionMask	= 0;

	PAGED_CODE();
	NT_ASSERT(PASSIVE_LEVEL == KeGetCurrentIrql());

	tVersionInfo.dwMajorVersion = 10;
	tVersionInfo.dwMinorVersion = 0;

	VER_SET_CONDITION(fConditionMask, VER_MAJORVERSION, VER_GREATER_EQUAL);
	VER_SET_CONDITION(fConditionMask, VER_MINORVERSION, VER_GREATER_EQUAL);

	eStatus = RtlVerifyVersionInfo(&tVersionInfo, VER_MAJORVERSION | VER_MINORVERSION, fConditionMask);
	NT_ASSERT(STATUS_SUCCESS == eStatus || STATUS_REVISION_MISMATCH == eStatus);
	C_ASSERT(!NT_SUCCESS(STATUS_REVISION_MISMATCH));

	return NT_SUCCESS(eStatus);
}

_IRQL_requires_(PASSIVE_LEVEL)
PAGEABLE
NTSTATUS
UTIL_InitUnicodeStringCb(
	_In_reads_bytes_(cbInputStringMax)	PWCHAR			pwcInputString,
	_In_								SIZE_T			cbInputStringMax,
	_Out_								PUNICODE_STRING	pusOutputString
)
{
	NTSTATUS	eStatus		= STATUS_UNSUCCESSFUL;
	size_t		cbString	= 0;

	PAGED_CODE();
	ASSERT(PASSIVE_LEVEL == KeGetCurrentIrql());

	if (NULL == pusOutputString)
	{
		eStatus = STATUS_INVALID_PARAMETER;
		goto lblCleanup;
	}

	if (((NULL == pwcInputString) && (0 != cbInputStringMax)) ||
		((NULL != pwcInputString) && (0 == cbInputStringMax)))
	{
		eStatus = STATUS_INVALID_PARAMETER;
		goto lblCleanup;
	}

	// Go the short way if we got a NULL source pointer.
	if (NULL == pwcInputString)
	{
		RtlInitUnicodeString(pusOutputString, NULL);
		eStatus = STATUS_SUCCESS;
		goto lblCleanup;
	}

	// Set the pointer to the buffer.
	pusOutputString->Buffer = pwcInputString;

	// Set the string's maximum size (buffer size).
	eStatus = RtlSIZETToUShort(cbInputStringMax,
							   &(pusOutputString->MaximumLength));
	if (!NT_SUCCESS(eStatus))
	{
		goto lblCleanup;
	}

	// Calculate the size of the null-terminated string.
	eStatus = RtlStringCbLengthW(pusOutputString->Buffer,
								 pusOutputString->MaximumLength,
								 &cbString);
	if (!NT_SUCCESS(eStatus))
	{
		// Assume the string is not null-terminated.
		cbString = pusOutputString->MaximumLength;
	}

	// Convert the size.
	eStatus = RtlSizeTToUShort(cbString, &(pusOutputString->Length));
	if (!NT_SUCCESS(eStatus))
	{
		goto lblCleanup;
	}

	eStatus = STATUS_SUCCESS;

lblCleanup:
	return eStatus;
}

_IRQL_requires_(PASSIVE_LEVEL)
PAGEABLE
NTSTATUS
UTIL_InitUnicodeStringCch(
	_In_reads_(cchInputStringMax)	PWCHAR			pwcInputString,
	_In_							SIZE_T			cchInputStringMax,
	_Out_							PUNICODE_STRING	pusOutputString
)
{
	NTSTATUS	eStatus				= STATUS_UNSUCCESSFUL;
	SIZE_T		cbInputStringMax	= 0;

	PAGED_CODE();
	ASSERT(PASSIVE_LEVEL == KeGetCurrentIrql());

	if (NULL == pusOutputString)
	{
		eStatus = STATUS_INVALID_PARAMETER;
		goto lblCleanup;
	}

	if (((NULL == pwcInputString) && (0 != cchInputStringMax)) ||
		((NULL != pwcInputString) && (0 == cchInputStringMax)))
	{
		eStatus = STATUS_INVALID_PARAMETER;
		goto lblCleanup;
	}

	// Calculate the string's maximum size in bytes.
	eStatus = RtlSIZETMult(cchInputStringMax,
						   sizeof(pusOutputString->Buffer[0]),
						   &cbInputStringMax);
	if (!NT_SUCCESS(eStatus))
	{
		goto lblCleanup;
	}

	// Go down the common path.
	eStatus = UTIL_InitUnicodeStringCb(pwcInputString,
									   cbInputStringMax,
									   pusOutputString);

	// Keep last status

lblCleanup:
	return eStatus;
}

_IRQL_requires_(PASSIVE_LEVEL)
PAGEABLE
NTSTATUS
UTIL_InitAnsiStringCb(
	_In_reads_bytes_(cbInputStringMax)	PCHAR			pcInputString,
	_In_								SIZE_T			cbInputStringMax,
	_Out_								PANSI_STRING	psOutputString
)
{
	NTSTATUS	eStatus		= STATUS_UNSUCCESSFUL;
	size_t		cbString	= 0;

	PAGED_CODE();
	ASSERT(PASSIVE_LEVEL == KeGetCurrentIrql());

	if (NULL == psOutputString)
	{
		eStatus = STATUS_INVALID_PARAMETER;
		goto lblCleanup;
	}

	if (((NULL == pcInputString) && (0 != cbInputStringMax)) ||
		((NULL != pcInputString) && (0 == cbInputStringMax)))
	{
		eStatus = STATUS_INVALID_PARAMETER;
		goto lblCleanup;
	}

	// Go the short way if we got a NULL source pointer.
	if (NULL == pcInputString)
	{
		RtlInitAnsiString(psOutputString, NULL);
		eStatus = STATUS_SUCCESS;
		goto lblCleanup;
	}

	// Set the pointer to the buffer.
	psOutputString->Buffer = pcInputString;

	// Set the string's maximum size (buffer size).
	eStatus = RtlSIZETToUShort(cbInputStringMax,
							   &(psOutputString->MaximumLength));
	if (!NT_SUCCESS(eStatus))
	{
		goto lblCleanup;
	}

	// Calculate the size of the null-terminated string.
	eStatus = RtlStringCbLengthA(psOutputString->Buffer,
								 psOutputString->MaximumLength,
								 &cbString);
	if (!NT_SUCCESS(eStatus))
	{
		// Assume the string is not null-terminated.
		cbString = psOutputString->MaximumLength;
	}

	// Convert the size.
	eStatus = RtlSizeTToUShort(cbString, &(psOutputString->Length));
	if (!NT_SUCCESS(eStatus))
	{
		goto lblCleanup;
	}

	eStatus = STATUS_SUCCESS;

lblCleanup:
	return eStatus;
}

_IRQL_requires_(PASSIVE_LEVEL)
PAGEABLE
NTSTATUS
UTIL_InitAnsiStringCch(
	_In_reads_(cchInputStringMax)	PCHAR			pcInputString,
	_In_							SIZE_T			cchInputStringMax,
	_Out_							PANSI_STRING	psOutputString
)
{
	NTSTATUS	eStatus				= STATUS_UNSUCCESSFUL;
	SIZE_T		cbInputStringMax	= 0;

	PAGED_CODE();
	ASSERT(PASSIVE_LEVEL == KeGetCurrentIrql());

	if (NULL == psOutputString)
	{
		eStatus = STATUS_INVALID_PARAMETER;
		goto lblCleanup;
	}

	if (((NULL == pcInputString) && (0 != cchInputStringMax)) ||
		((NULL != pcInputString) && (0 == cchInputStringMax)))
	{
		eStatus = STATUS_INVALID_PARAMETER;
		goto lblCleanup;
	}

	// Calculate the string's maximum size in bytes.
	eStatus = RtlSIZETMult(cchInputStringMax,
						   sizeof(psOutputString->Buffer[0]),
						   &cbInputStringMax);
	if (!NT_SUCCESS(eStatus))
	{
		goto lblCleanup;
	}

	// Go down the common path.
	eStatus = UTIL_InitAnsiStringCb(pcInputString,
									   cbInputStringMax,
									   psOutputString);

	// Keep last status

lblCleanup:
	return eStatus;
}

_IRQL_requires_(PASSIVE_LEVEL)
PAGEABLE
NTSTATUS
UTIL_QueryModuleInformation(
	_Outptr_result_buffer_(*pnModules)	PAUX_MODULE_EXTENDED_INFO *	pptModules,
	_Out_								PULONG						pnModules
)
{
	NTSTATUS					eStatus			= STATUS_UNSUCCESSFUL;
	PAUX_MODULE_EXTENDED_INFO	ptModules		= NULL;
	ULONG						cbModules		= 0;
	ULONG						nModules		= 0;
	PAUX_MODULE_EXTENDED_INFO	ptCurrentModule	= NULL;

	PAGED_CODE();
	ASSERT(PASSIVE_LEVEL == KeGetCurrentIrql());

	if ((NULL == pptModules) ||
		(NULL == pnModules))
	{
		eStatus = STATUS_INVALID_PARAMETER;
		goto lblCleanup;
	}

	for (;;)
	{
		// Try to obtain the module list.
		eStatus = AuxKlibQueryModuleInformation(&cbModules,
												sizeof(ptModules[0]),
												ptModules);
		if (NT_SUCCESS(eStatus))
		{
			if (NULL != ptModules)
			{
				break;
			}
			else
			{
				eStatus = STATUS_BUFFER_TOO_SMALL;
			}
		}
		if (STATUS_BUFFER_TOO_SMALL != eStatus)
		{
			goto lblCleanup;
		}

		// Free the previous buffer.
		CLOSE(ptModules, ExFreePool);

		// Allocate a new one.
		ptModules =
			(PAUX_MODULE_EXTENDED_INFO)ExAllocatePoolWithTag(PagedPool,
															 cbModules,
															 UTIL_POOL_TAG);
		if (NULL == ptModules)
		{
			eStatus = STATUS_INSUFFICIENT_RESOURCES;
			goto lblCleanup;
		}
		RtlSecureZeroMemory(ptModules, cbModules);
	}

	nModules = 0;
	for (ptCurrentModule = ptModules;
		 ptCurrentModule < (PAUX_MODULE_EXTENDED_INFO)RtlOffsetToPointer(ptModules, cbModules);
		 ++ptCurrentModule)
	{
		if (NULL == ptCurrentModule->BasicInfo.ImageBase)
		{
			break;
		}

		++nModules;
	}

	// Transfer ownership:
	*pptModules = ptModules;
	ptModules = NULL;
	*pnModules = nModules;

	eStatus = STATUS_SUCCESS;

lblCleanup:
	CLOSE(ptModules, ExFreePool);

	return eStatus;
}

_Use_decl_annotations_
PAGEABLE
NTSTATUS
UTIL_QuerySystemInformation(
	SYSTEM_INFORMATION_CLASS	eInfoClass,
	PVOID *						ppvInformation,
	PULONG						pcbInformation
)
{
	NTSTATUS	eStatus			= STATUS_UNSUCCESSFUL;
	PVOID		pvInformation	= NULL;
	ULONG		cbInformation	= 0;
	ULONG		nIteration		= 0;
	ULONG		cbReturned		= 0;

	PAGED_CODE();
	ASSERT(PASSIVE_LEVEL == KeGetCurrentIrql());

	if (NULL == ppvInformation)
	{
		eStatus = STATUS_INVALID_PARAMETER;
		goto lblCleanup;
	}

	cbInformation = PAGE_SIZE;

	for (nIteration = 0; nIteration < QUERY_INFO_ITERATIONS; ++nIteration)
	{
		// Allocate a buffer.
		pvInformation = ExAllocatePoolWithTag(PagedPool, cbInformation, UTIL_POOL_TAG);
		if (NULL == pvInformation)
		{
			eStatus = STATUS_INSUFFICIENT_RESOURCES;
			goto lblCleanup;
		}

		// Try to obtain the information.
		eStatus = ZwQuerySystemInformation(eInfoClass, pvInformation, cbInformation, &cbReturned);
		if (NT_SUCCESS(eStatus))
		{
			break;
		}

		// Free the buffer.
		CLOSE(pvInformation, ExFreePool);

		// Try again with double the size.
		cbInformation *= 2;
	}
	if (!NT_SUCCESS(eStatus))
	{
		goto lblCleanup;
	}

	// Transfer ownership:
	*ppvInformation = pvInformation;
	pvInformation = NULL;

	if (NULL != pcbInformation)
	{
		*pcbInformation = cbReturned;
	}

	eStatus = STATUS_SUCCESS;

lblCleanup:
	CLOSE(pvInformation, ExFreePool);

	return eStatus;
}

_Use_decl_annotations_
PAGEABLE
NTSTATUS
UTIL_GetObjectDirectoryContents(
	HANDLE							hDirectory,
	POBJECT_DIRECTORY_INFORMATION *	pptObjectInfos
)
{
	NTSTATUS	eStatus			= STATUS_UNSUCCESSFUL;
	PVOID		pvInformation	= NULL;
	ULONG		cbInformation	= 0;
	ULONG		nIteration		= 0;
	ULONG		cbReturned		= 0;
	ULONG		nContext		= 0;

	PAGED_CODE();
	NT_ASSERT(PASSIVE_LEVEL == KeGetCurrentIrql());

	if (NULL == hDirectory || NULL == pptObjectInfos)
	{
		eStatus = STATUS_INVALID_PARAMETER;
		goto lblCleanup;
	}

	cbInformation = PAGE_SIZE;

	for (nIteration = 0; nIteration < QUERY_INFO_ITERATIONS; ++nIteration)
	{
		// Allocate a buffer.
		pvInformation = ExAllocatePoolWithTag(PagedPool, cbInformation, UTIL_POOL_TAG);
		if (NULL == pvInformation)
		{
			eStatus = STATUS_INSUFFICIENT_RESOURCES;
			goto lblCleanup;
		}

		// Try to obtain the information.
		eStatus = ZwQueryDirectoryObject(hDirectory, pvInformation, cbInformation, FALSE, TRUE, &nContext, &cbReturned);
		if (NT_SUCCESS(eStatus) && STATUS_MORE_ENTRIES != eStatus)
		{
			break;
		}

		// Free the buffer.
		CLOSE(pvInformation, ExFreePool);

		// Try again with double the size.
		cbInformation *= 2;
	}
	if (!NT_SUCCESS(eStatus))
	{
		goto lblCleanup;
	}

	// Transfer ownership:
	*pptObjectInfos = pvInformation;
	pvInformation = NULL;

	eStatus = STATUS_SUCCESS;

lblCleanup:
	CLOSE(pvInformation, ExFreePool);

	return eStatus;
}
