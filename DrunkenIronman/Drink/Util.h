/**
 * @file Util.h
 * @author biko
 * @date 2016-08-06
 *
 * Miscellaneous kernel-mode utilities.
 */
#pragma once

/** Headers *************************************************************/
#include <ntifs.h>


/** Functions ***********************************************************/

/**
 * Initializes a UNICODE_STRING structure.
 *
 * @param[in]	pwcInputString		Buffer to initialize with.
 * @param[in]	cbInputStringMax	Maximum size of the buffer, in bytes.
 * @param[in]	pusOutputString		UNICODE_STRING to initialize.
 *
 * @returns NTSTATUS
 *
 * @remark The input buffer may not be null-terminated.
 */
NTSTATUS
UTIL_InitUnicodeStringCb(
	_In_reads_bytes_(cbInputStringMax)	PWCHAR			pwcInputString,
	_In_								SIZE_T			cbInputStringMax,
	_Out_								PUNICODE_STRING	pusOutputString
);

/**
 * Initializes a UNICODE_STRING structure.
 *
 * @param[in]	pwcInputString		Buffer to initialize with.
 * @param[in]	cchInputStringMax	Maximum size of the buffer, in characters.
 * @param[in]	pusOutputString		UNICODE_STRING to initialize.
 *
 * @returns NTSTATUS
 *
 * @remark The input buffer may not be null-terminated.
 */
NTSTATUS
UTIL_InitUnicodeStringCch(
	_In_reads_(cchInputStringMax)	PWCHAR			pwcInputString,
	_In_							SIZE_T			cchInputStringMax,
	_Out_							PUNICODE_STRING	pusOutputString
);

/**
 * Initializes an ANSI_STRING structure.
 *
 * @param[in]	pcInputString		Buffer to initialize with.
 * @param[in]	cbInputStringMax	Maximum size of the buffer, in bytes.
 * @param[in]	psOutputString		ANSI_STRING to initialize.
 *
 * @returns NTSTATUS
 *
 * @remark The input buffer may not be null-terminated.
 */
NTSTATUS
UTIL_InitAnsiStringCb(
	_In_reads_bytes_(cbInputStringMax)	PCHAR			pcInputString,
	_In_								SIZE_T			cbInputStringMax,
	_Out_								PANSI_STRING	psOutputString
);

/**
 * Initializes an ANSI_STRING structure.
 *
 * @param[in]	pcInputString		Buffer to initialize with.
 * @param[in]	cchInputStringMax	Maximum size of the buffer, in bytes.
 * @param[in]	psOutputString		ANSI_STRING to initialize.
 *
 * @returns NTSTATUS
 *
 * @remark The input buffer may not be null-terminated.
 */
NTSTATUS
UTIL_InitAnsiStringCch(
	_In_reads_(cchInputStringMax)	PCHAR			pcInputString,
	_In_							SIZE_T			cchInputStringMax,
	_Out_							PANSI_STRING	psOutputString
);
