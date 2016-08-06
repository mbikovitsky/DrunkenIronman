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


/** Constants ***********************************************************/

/**
 * The maximum length of a loaded module's filename.
 *
 * @remark Adapted from aux_klib.h.
 */
#define AUX_KLIB_MODULE_PATH_LEN (256)


/** Typedefs ************************************************************/

/**
 * Contains basic information about a loaded module.
 *
 * @remark Adapted from aux_klib.h.
 */
typedef struct _AUX_MODULE_BASIC_INFO
{
	PVOID	pvImageBase;
} AUX_MODULE_BASIC_INFO, *PAUX_MODULE_BASIC_INFO;

/**
 * Contains extended information about a loaded module.
 *
 * @remark Adapted from aux_klib.h.
 */
typedef struct _AUX_MODULE_EXTENDED_INFO
{
	AUX_MODULE_BASIC_INFO	tBasicInfo;
	ULONG					cbImage;
	USHORT					cbFileNameOffset;
	UCHAR					acFullPathName[AUX_KLIB_MODULE_PATH_LEN];
} AUX_MODULE_EXTENDED_INFO, *PAUX_MODULE_EXTENDED_INFO;


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
_IRQL_requires_(PASSIVE_LEVEL)
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
_IRQL_requires_(PASSIVE_LEVEL)
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
_IRQL_requires_(PASSIVE_LEVEL)
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
_IRQL_requires_(PASSIVE_LEVEL)
NTSTATUS
UTIL_InitAnsiStringCch(
	_In_reads_(cchInputStringMax)	PCHAR			pcInputString,
	_In_							SIZE_T			cchInputStringMax,
	_Out_							PANSI_STRING	psOutputString
);

/**
 * Retrieves the list of loaded kernel modules.
 *
 * @param[out]	pptModules	Will receive the list of modules.
 * @param[out]	pnModules	Will receive the length of the list, in elements.
 *
 * @returns NTSTATUS
 *
 * @remark	Call AuxKlibInitialize before invoking this routine.
 * @remark	The returned buffer is allocated from the _paged_ pool.
 *			Free it with ExFreePool.
 */
_IRQL_requires_(PASSIVE_LEVEL)
NTSTATUS
UTIL_QueryModuleInformation(
	_Outptr_result_buffer_(*pnModules)	PAUX_MODULE_EXTENDED_INFO *	pptModules,
	_Out_								PULONG						pnModules
);
