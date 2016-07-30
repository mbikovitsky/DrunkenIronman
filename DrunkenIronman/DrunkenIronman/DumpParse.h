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
#include "Precomp.h"


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
