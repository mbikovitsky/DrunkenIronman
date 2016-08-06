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
