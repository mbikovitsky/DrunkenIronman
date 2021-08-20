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

#include <Common.h>

#include "Util.h"


/** Constants ***********************************************************/

/**
 * Pool tag for allocations made by this module.
 */
#define UTIL_POOL_TAG (RtlUlongByteSwap('Util'))


/** Forward Declarations ************************************************/

/**
 * Retrieves the list of loaded kernel modules.
 *
 * @param[in,out]	pcbBuffer	On input, contains the size of
 *								the buffer at pvQueryInfo.
 *								On output, contains the required
 *								buffer size.
 * @param[in]		cbElement	Size of the element to retrieve.
 *								Can be either sizeof(AUX_MODULE_BASIC_INFO)
 *								or sizeof(AUX_MODULE_EXTENDED_INFO).
 * @param[out]		pcbBuffer	Buffer to receive the list.
 *
 * @returns NTSTATUS
 *
 * @remark Call AuxKlibInitialize before invoking this routine.
 * @remark Adapted from aux_klib.h.
 */
_IRQL_requires_(PASSIVE_LEVEL)
NTSTATUS
NTAPI
AuxKlibQueryModuleInformation(
	_Inout_								PULONG	pcbBuffer,
	_In_								ULONG	cbElement,
	_Out_writes_bytes_opt_(*pcbBuffer)	PVOID	pvQueryInfo
);


/** Functions ***********************************************************/

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
		if (NULL == ptCurrentModule->tBasicInfo.pvImageBase)
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
