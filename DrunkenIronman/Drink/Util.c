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

#include "Util.h"


/** Functions ***********************************************************/

NTSTATUS
UTIL_InitUnicodeStringCb(
	_In_reads_bytes_(cbInputStringMax)	PWCHAR			pwcInputString,
	_In_								SIZE_T			cbInputStringMax,
	_Out_								PUNICODE_STRING	pusOutputString
)
{
	NTSTATUS	eStatus		= STATUS_UNSUCCESSFUL;
	size_t		cbString	= 0;

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

NTSTATUS
UTIL_InitUnicodeStringCch(
	_In_reads_(cchInputStringMax)	PWCHAR			pwcInputString,
	_In_							SIZE_T			cchInputStringMax,
	_Out_							PUNICODE_STRING	pusOutputString
)
{
	NTSTATUS	eStatus				= STATUS_UNSUCCESSFUL;
	SIZE_T		cbInputStringMax	= 0;

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

NTSTATUS
UTIL_InitAnsiStringCb(
	_In_reads_bytes_(cbInputStringMax)	PCHAR			pcInputString,
	_In_								SIZE_T			cbInputStringMax,
	_Out_								PANSI_STRING	psOutputString
)
{
	NTSTATUS	eStatus		= STATUS_UNSUCCESSFUL;
	size_t		cbString	= 0;

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

NTSTATUS
UTIL_InitAnsiStringCch(
	_In_reads_(cchInputStringMax)	PCHAR			pcInputString,
	_In_							SIZE_T			cchInputStringMax,
	_Out_							PANSI_STRING	psOutputString
)
{
	NTSTATUS	eStatus				= STATUS_UNSUCCESSFUL;
	SIZE_T		cbInputStringMax	= 0;

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
