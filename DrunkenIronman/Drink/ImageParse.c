/**
 * @file ImageParse.c
 * @author biko
 * @date 2016-08-06
 *
 * ImageParse module implementation.
 */

/** Headers *************************************************************/
#include <ntifs.h>
#include <ntimage.h>

#include "ImageParse.h"


/** Functions ***********************************************************/

NTSTATUS
IMAGEPARSE_GetNtHeaders(
	_In_			PVOID				pvImageBase,
	_Outptr_		PIMAGE_NT_HEADERS *	pptNtHeaders,
	_Outptr_opt_	PIMAGE_DOS_HEADER *	pptDosHeader
)
{
	NTSTATUS			eStatus		= STATUS_UNSUCCESSFUL;
	PIMAGE_DOS_HEADER	ptDosHeader	= (PIMAGE_DOS_HEADER)pvImageBase;
	PIMAGE_NT_HEADERS	ptNtHeaders	= NULL;

	if ((NULL == pvImageBase) ||
		(NULL == pptNtHeaders))
	{
		eStatus = STATUS_INVALID_PARAMETER;
		goto lblCleanup;
	}

	if (IMAGE_DOS_SIGNATURE != ptDosHeader->e_magic)
	{
		eStatus = STATUS_INVALID_IMAGE_NOT_MZ;
		goto lblCleanup;
	}

	ptNtHeaders = (PIMAGE_NT_HEADERS)RtlOffsetToPointer(ptDosHeader,
														ptDosHeader->e_lfanew);
	ASSERT(NULL != ptNtHeaders);

	if ((IMAGE_NT_SIGNATURE != ptNtHeaders->Signature) ||
		(IMAGE_NT_OPTIONAL_HDR_MAGIC != ptNtHeaders->OptionalHeader.Magic))
	{
		eStatus = STATUS_INVALID_IMAGE_FORMAT;
		goto lblCleanup;
	}

	// Return results to caller:
	*pptNtHeaders = ptNtHeaders;
	if (NULL != pptDosHeader)
	{
		*pptDosHeader = ptDosHeader;
	}

	eStatus = STATUS_SUCCESS;

lblCleanup:
	return eStatus;
}

NTSTATUS
IMAGEPARSE_DirectoryEntryToData(
	_In_		PVOID	pvImageBase,
	_In_		USHORT	nDirectoryEntry,
	_Outptr_	PVOID *	ppvDirectoryData,
	_Out_		PULONG	pcbDirectoryData
)
{
	NTSTATUS				eStatus				= STATUS_UNSUCCESSFUL;
	PIMAGE_NT_HEADERS		ptNtHeaders			= NULL;
	PIMAGE_DATA_DIRECTORY	ptDirectoryEntry	= NULL;
	ULONG					cbDirectoryRva		= 0;
	ULONG					cbDirectoryData		= 0;
	PVOID					pvDirectoryData		= NULL;

	if ((NULL == pvImageBase) ||
		(NULL == ppvDirectoryData) ||
		(NULL == pcbDirectoryData))
	{
		eStatus = STATUS_INVALID_PARAMETER;
		goto lblCleanup;
	}

	eStatus = IMAGEPARSE_GetNtHeaders(pvImageBase, &ptNtHeaders, NULL);
	if (!NT_SUCCESS(eStatus))
	{
		goto lblCleanup;
	}

	if (nDirectoryEntry >= ptNtHeaders->OptionalHeader.NumberOfRvaAndSizes)
	{
		eStatus = STATUS_NOT_FOUND;
		goto lblCleanup;
	}

	ptDirectoryEntry = &(ptNtHeaders->OptionalHeader.DataDirectory[nDirectoryEntry]);
	ASSERT(NULL != ptDirectoryEntry);

	cbDirectoryRva = ptDirectoryEntry->VirtualAddress;
	if (0 == cbDirectoryRva)
	{
		eStatus = STATUS_NOT_FOUND;
		goto lblCleanup;
	}

	cbDirectoryData = ptDirectoryEntry->Size;
	if (0 == cbDirectoryData)
	{
		eStatus = STATUS_INVALID_IMAGE_FORMAT;
		goto lblCleanup;
	}

	pvDirectoryData = RtlOffsetToPointer(pvImageBase, cbDirectoryRva);
	ASSERT(NULL != pvDirectoryData);

	// Return results to caller:
	*ppvDirectoryData = pvDirectoryData;
	*pcbDirectoryData = cbDirectoryData;

	eStatus = STATUS_SUCCESS;

lblCleanup:
	return eStatus;
}
