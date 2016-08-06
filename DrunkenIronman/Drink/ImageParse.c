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

#include "Util.h"

#include "ImageParse.h"


/** Functions ***********************************************************/

/**
 * Retrieves a resource from a mapped image.
 * Recursively descends the resource tree as needed.
 *
 * @param[in]	pvResourceDirectoryBase	Base address of the resource directory.
 * @param[in]	ptRoot					The current resource tree root.
 * @param[in]	ptResourcePath			Path for the resource to find.
 * @param[in]	nPathLength				Length of the path, in elements.
 * @param[out]	ppvResourceData			Will receive a pointer to the resource data.
 * @param[out]	pcbResourceData			Will receive the resource data's size, in bytes.
 *
 * @returns NTSTATUS
 */
STATIC
NTSTATUS
imageparse_FindResourceRecursive(
	_In_											PVOID						pvResourceDirectoryBase,
	_In_											PIMAGE_RESOURCE_DIRECTORY	ptRoot,
	_In_reads_(nPathLength)							PCRESOURCE_PATH_COMPONENT	ptResourcePath,
	_In_											ULONG						nPathLength,
	_Outptr_result_bytebuffer_(*pcbResourceData)	PVOID *						ppvResourceData,
	_Out_											PULONG						pcbResourceData
)
{
	NTSTATUS						eStatus			= STATUS_UNSUCCESSFUL;
	USHORT							nIndex			= 0;
	PIMAGE_RESOURCE_DIRECTORY_ENTRY	ptNamedEntries	= NULL;
	PIMAGE_RESOURCE_DIRECTORY_ENTRY	ptIdEntries		= NULL;
	PIMAGE_RESOURCE_DIRECTORY_ENTRY	ptFoundEntry	= NULL;
	PIMAGE_RESOURCE_DIR_STRING_U	ptEntryName		= NULL;
	UNICODE_STRING					usEntryName		= { 0 };
	PIMAGE_RESOURCE_DIRECTORY		ptNextLevel		= NULL;
	PIMAGE_RESOURCE_DATA_ENTRY		ptDataEntry		= NULL;

	ASSERT(NULL != pvResourceDirectoryBase);
	ASSERT(NULL != ptRoot);
	ASSERT(NULL != ptResourcePath);
	ASSERT(0 != nPathLength);
	ASSERT(NULL != ppvResourceData);
	ASSERT(NULL != pcbResourceData);

	ptNamedEntries = (PIMAGE_RESOURCE_DIRECTORY_ENTRY)(ptRoot + 1);
	ptIdEntries = ptNamedEntries + ptRoot->NumberOfNamedEntries;

	// Look for the entry by name or by ID
	if (ptResourcePath[0].bNamed)
	{
		for (nIndex = 0; nIndex < ptRoot->NumberOfNamedEntries; ++nIndex)
		{
			ASSERT(ptNamedEntries[nIndex].NameIsString);

			ptEntryName =
				(PIMAGE_RESOURCE_DIR_STRING_U)RtlOffsetToPointer(pvResourceDirectoryBase,
																 ptNamedEntries[nIndex].NameOffset);

			eStatus = UTIL_InitUnicodeStringCch(ptEntryName->NameString,
												ptEntryName->Length,
												&usEntryName);
			if (!NT_SUCCESS(eStatus))
			{
				goto lblCleanup;
			}

			if (RtlEqualUnicodeString(&(ptResourcePath[0].tComponent.usName),
									  &usEntryName,
									  TRUE))
			{
				ptFoundEntry = &(ptNamedEntries[nIndex]);
				break;
			}
		}
	}
	else
	{
		for (nIndex = 0; nIndex < ptRoot->NumberOfIdEntries; ++nIndex)
		{
			ASSERT(!(ptIdEntries[nIndex].NameIsString));

			if (ptResourcePath[0].tComponent.nId == ptIdEntries[nIndex].Id)
			{
				ptFoundEntry = &(ptIdEntries[nIndex]);
				break;
			}
		}
	}
	if (NULL == ptFoundEntry)
	{
		eStatus = STATUS_NOT_FOUND;
		goto lblCleanup;
	}

	if (ptFoundEntry->DataIsDirectory)
	{
		// Fail if this is the last path component but we didn't find a leaf.
		if (1 == nPathLength)
		{
			eStatus = STATUS_NOT_FOUND;
			goto lblCleanup;
		}

		ptNextLevel =
			(PIMAGE_RESOURCE_DIRECTORY)RtlOffsetToPointer(pvResourceDirectoryBase,
														  ptFoundEntry->OffsetToDirectory);
		ASSERT(NULL != ptNextLevel);
		ASSERT(0 != ptFoundEntry->OffsetToDirectory);

		// Go down to the next level.
		eStatus = imageparse_FindResourceRecursive(pvResourceDirectoryBase,
												   ptNextLevel,
												   ptResourcePath + 1,
												   nPathLength - 1,
												   ppvResourceData,
												   pcbResourceData);
	}
	else
	{
		// Fail if we have more path components but we found a leaf.
		if (1 < nPathLength)
		{
			eStatus = STATUS_NOT_FOUND;
			goto lblCleanup;
		}

		ptDataEntry =
			(PIMAGE_RESOURCE_DATA_ENTRY)RtlOffsetToPointer(pvResourceDirectoryBase,
														   ptFoundEntry->OffsetToData);
		ASSERT(NULL != ptDataEntry);
		ASSERT(0 != ptFoundEntry->OffsetToData);

		// Return results.
		*ppvResourceData = RtlOffsetToPointer(pvResourceDirectoryBase,
											  ptDataEntry->OffsetToData);
		*pcbResourceData = ptDataEntry->Size;
		eStatus = STATUS_SUCCESS;
	}

	// Keep last status

lblCleanup:
	return eStatus;
}

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

NTSTATUS
IMAGEPARSE_FindResource(
	_In_											PVOID						pvImageBase,
	_In_reads_(nPathLength)							PCRESOURCE_PATH_COMPONENT	ptResourcePath,
	_In_											ULONG						nPathLength,
	_Outptr_result_bytebuffer_(*pcbResourceData)	PVOID *						ppvResourceData,
	_Out_											PULONG						pcbResourceData
)
{
	NTSTATUS					eStatus				= STATUS_UNSUCCESSFUL;
	PVOID						pvResourceDirectory	= NULL;
	ULONG						cbResourceDirectory	= 0;

	if ((NULL == pvImageBase) ||
		(NULL == ptResourcePath) ||
		(0 == nPathLength) ||
		(NULL == ppvResourceData) ||
		(NULL == pcbResourceData))
	{
		eStatus = STATUS_INVALID_PARAMETER;
		goto lblCleanup;
	}

	eStatus = IMAGEPARSE_DirectoryEntryToData(pvImageBase,
											  IMAGE_DIRECTORY_ENTRY_RESOURCE,
											  &pvResourceDirectory,
											  &cbResourceDirectory);
	if (!NT_SUCCESS(eStatus))
	{
		goto lblCleanup;
	}
	ASSERT(0 != cbResourceDirectory);

	eStatus = imageparse_FindResourceRecursive(pvResourceDirectory,
											   (PIMAGE_RESOURCE_DIRECTORY)pvResourceDirectory,
											   ptResourcePath,
											   nPathLength,
											   ppvResourceData,
											   pcbResourceData);

	// Keep last status

lblCleanup:
	return eStatus;
}
