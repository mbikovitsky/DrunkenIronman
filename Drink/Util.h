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

/**
 * @brief Contains information about a single big pool allocation.
 *
 * @see https://www.geoffchappell.com/studies/windows/km/ntoskrnl/api/ex/sysinfo/bigpool_entry.htm
 */
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

/**
 * @brief Contains information about big pool allocations.
 *
 * @see https://www.geoffchappell.com/studies/windows/km/ntoskrnl/api/ex/sysinfo/bigpool.htm
*/
typedef struct _SYSTEM_BIGPOOL_INFORMATION
{
	ULONG					Count;
	SYSTEM_BIGPOOL_ENTRY	AllocatedInfo[ANYSIZE_ARRAY];
} SYSTEM_BIGPOOL_INFORMATION, *PSYSTEM_BIGPOOL_INFORMATION;
typedef SYSTEM_BIGPOOL_INFORMATION CONST *PCSYSTEM_BIGPOOL_INFORMATION;

/**
 * @brief Contains information about a named object.
 *
 * @see https://docs.microsoft.com/en-us/windows/win32/devnotes/ntquerydirectoryobject
*/
typedef struct _OBJECT_DIRECTORY_INFORMATION
{
	UNICODE_STRING	Name;
	UNICODE_STRING	TypeName;
} OBJECT_DIRECTORY_INFORMATION, *POBJECT_DIRECTORY_INFORMATION;
typedef OBJECT_DIRECTORY_INFORMATION CONST *PCOBJECT_DIRECTORY_INFORMATION;


/** Enums ***************************************************************/

/**
 * @brief Information classes for ZwQuerySystemInformation.
 *
 * @see https://www.geoffchappell.com/studies/windows/km/ntoskrnl/api/ex/sysinfo/query.htm
*/
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

/**
 * @brief Convenience wrapper around ZwQuerySystemInformation.
 *
 * @param eInfoClass		Information class.
 * @param ppvInformation	Will receive the information buffer.
 * @param pcbInformation	Will receive the size of the buffer, in bytes.
 *
 * @return NTSTATUS
*/
_IRQL_requires_(PASSIVE_LEVEL)
PAGEABLE
NTSTATUS
UTIL_QuerySystemInformation(
	_In_										SYSTEM_INFORMATION_CLASS	eInfoClass,
	_Outptr_result_bytebuffer_(*pcbInformation)	PVOID *						ppvInformation,
	_Out_opt_									PULONG						pcbInformation
);

/**
 * @brief Returns the contents of an object directory.
 *
 * @param hDirectory		Directory to query.
 * @param pptObjectInfos	Will receive the object information
 *
 * @return NTSTATUS
 *
 * @remark The returned buffer is null-terminated, i.e. the last element is all zeroes.
*/
_IRQL_requires_(PASSIVE_LEVEL)
PAGEABLE
NTSTATUS
UTIL_GetObjectDirectoryContents(
	_In_		HANDLE							hDirectory,
	_Outptr_	POBJECT_DIRECTORY_INFORMATION *	pptObjectInfos
);
