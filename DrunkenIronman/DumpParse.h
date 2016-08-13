/**
 * @file DumpParse.h
 * @author biko
 * @date 2016-07-30
 *
 * DumpParse module public header.
 * Contains routines for accessing memory dump files.
 */
#pragma once

/** Headers *************************************************************/
#include <Windows.h>


/** Typedefs ************************************************************/

/**
 * Handle to an open dump file.
 */
DECLARE_HANDLE(HDUMP);
typedef HDUMP *PHDUMP;


/** Functions ***********************************************************/

/**
 * Opens a dump file.
 *
 * @param[in]	pwszPath	Path to the dump file.
 *							If not specified, the system crash dump
 *							will be opened (usually C:\Windows\MEMORY.DMP).
 * @param[in]	phDump		Will receive a handle to the dump file.
 *
 * @returns HRESULT
 */
HRESULT
DUMPPARSE_Open(
	_In_opt_	PCWSTR	pwszPath,
	_Out_		PHDUMP	phDump
);

/**
 * Closes a dump file.
 *
 * @param[in]	hDump	Dump file to close.
 */
VOID
DUMPPARSE_Close(
	_In_	HDUMP	hDump
);

/**
 * Reads tagged data from the dump file.
 *
 * @param[in]	hDump	Dump file to read from.
 * @param[in]	ptTag	Tag identifying the data to read.
 * @param[out]	ppvData	Will receive the read data.
 * @param[out]	pcbData	Will receive the read data's size, in bytes.
 *
 * @returns HRESULT
 *
 * @remark Free the returned buffer to the process heap.
 */
HRESULT
DUMPPARSE_ReadTagged(
	_In_									HDUMP	hDump,
	_In_									LPCGUID	ptTag,
	_Outptr_result_bytebuffer_(*pcbData)	PVOID *	ppvData,
	_Out_									PDWORD	pcbData
);
