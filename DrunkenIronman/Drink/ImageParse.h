/**
 * @file ImageParse.h
 * @author biko
 * @date 2016-08-06
 *
 * ImageParse module public header.
 * Contains routines for parsing of
 * PE images.
 */
#pragma once

/** Headers *************************************************************/
#include <ntifs.h>
#include <ntimage.h>


/** Functions ***********************************************************/

/**
 * Retrieves the NT headers from an image.
 *
 * @param[in]	pvImageBase		Base of the mapped image.
 * @param[out]	pptNtHeaders	Will receive a pointer to the NT headers.
 * @param[out]	pptDosHeader	Optionally receives the DOS header.
 *
 * @returns NTSTATUS
 */
NTSTATUS
IMAGEPARSE_GetNtHeaders(
	_In_			PVOID				pvImageBase,
	_Outptr_		PIMAGE_NT_HEADERS *	pptNtHeaders,
	_Outptr_opt_	PIMAGE_DOS_HEADER *	pptDosHeader
);

/**
 * Obtains a pointer to a data directory
 * within a mapped image.
 *
 * @param[in]	pvImageBase			Base of the mapped image.
 * @param[in]	nDirectoryEntry		Index of the directory entry to retrieve.
 * @param[out]	ppvDirectoryData	Will receive a pointer to the directory.
 * @param[out]	pcbDirectoryData	Will receive the directory's size.
 *
 * @returns NTSTATUS
 */
NTSTATUS
IMAGEPARSE_DirectoryEntryToData(
	_In_		PVOID	pvImageBase,
	_In_		USHORT	nDirectoryEntry,
	_Outptr_	PVOID *	ppvDirectoryData,
	_Out_		PULONG	pcbDirectoryData
);
