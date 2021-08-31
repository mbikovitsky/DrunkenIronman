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
#include <aux_klib.h>


/** Macros **************************************************************/

/**
 * Declares a function as residing in the pageable code section.
 */
#define PAGEABLE __declspec(code_seg("PAGE"))


/** Typedefs ************************************************************/

#pragma warning(push)
#pragma warning(disable: 4201)	// nonstandard extension used: nameless struct/union
typedef struct _SYSTEM_BIGPOOL_ENTRY
{
	union
	{
		PVOID		VirtualAddress;
		ULONG_PTR	NonPaged : 1;
	};

	ULONG_PTR	SizeInBytes;

	union
	{
		UCHAR	Tag[4];
		ULONG	TagUlong;
	};
} SYSTEM_BIGPOOL_ENTRY, *PSYSTEM_BIGPOOL_ENTRY;
typedef SYSTEM_BIGPOOL_ENTRY CONST *PCSYSTEM_BIGPOOL_ENTRY;
#pragma warning(pop)

typedef struct _SYSTEM_BIGPOOL_INFORMATION
{
	ULONG					Count;
	SYSTEM_BIGPOOL_ENTRY	AllocatedInfo[ANYSIZE_ARRAY];
} SYSTEM_BIGPOOL_INFORMATION, *PSYSTEM_BIGPOOL_INFORMATION;
typedef SYSTEM_BIGPOOL_INFORMATION CONST *PCSYSTEM_BIGPOOL_INFORMATION;


/** Enums ***************************************************************/

typedef enum _SYSTEM_INFORMATION_CLASS
{
	SystemBigPoolInformation = 0x42,
} SYSTEM_INFORMATION_CLASS, *PSYSTEM_INFORMATION_CLASS;
typedef SYSTEM_INFORMATION_CLASS CONST *PCSYSTEM_INFORMATION_CLASS;


/** Functions ***********************************************************/

/**
 * @brief Determines whether this system is Windows 10 or greater.
 *
 * @return BOOLEAN
*/
_IRQL_requires_(PASSIVE_LEVEL)
PAGEABLE
BOOLEAN
UTIL_IsWindows10OrGreater(VOID);

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
PAGEABLE
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
PAGEABLE
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
PAGEABLE
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
PAGEABLE
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
PAGEABLE
NTSTATUS
UTIL_QueryModuleInformation(
	_Outptr_result_buffer_(*pnModules)	PAUX_MODULE_EXTENDED_INFO *	pptModules,
	_Out_								PULONG						pnModules
);

_IRQL_requires_(PASSIVE_LEVEL)
PAGEABLE
NTSTATUS
UTIL_QuerySystemInformation(
	_In_										SYSTEM_INFORMATION_CLASS	eInfoClass,
	_Outptr_result_bytebuffer_(*pcbInformation)	PVOID *						ppvInformation,
	_Out_opt_									PULONG						pcbInformation
);
