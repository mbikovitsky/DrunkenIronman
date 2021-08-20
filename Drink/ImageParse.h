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


/** Typedefs ************************************************************/

/**
 * Defines a single resource path component.
 */
typedef struct _RESOURCE_PATH_COMPONENT
{
	BOOLEAN	bNamed;
	union
	{
		USHORT			nId;
		UNICODE_STRING	usName;
	} tComponent;
} RESOURCE_PATH_COMPONENT, *PRESOURCE_PATH_COMPONENT;
typedef CONST RESOURCE_PATH_COMPONENT *PCRESOURCE_PATH_COMPONENT;


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

/**
 * Retrieves a resource from a mapped image.
 *
 * @param[in]	pvImageBase		Base of the mapped image.
 * @param[in]	ptResourcePath	Path for the resource to find.
 * @param[in]	nPathLength		Length of the path, in elements.
 * @param[out]	ppvResourceData	Will receive a pointer to the resource data.
 * @param[out]	pcbResourceData	Will receive the resource data's size, in bytes.
 *
 * @returns NTSTATUS
 */
NTSTATUS
IMAGEPARSE_FindResource(
	_In_											PVOID						pvImageBase,
	_In_reads_(nPathLength)							PCRESOURCE_PATH_COMPONENT	ptResourcePath,
	_In_											ULONG						nPathLength,
	_Outptr_result_bytebuffer_(*pcbResourceData)	PVOID *						ppvResourceData,
	_Out_											PULONG						pcbResourceData
);

/**
 * Retrieves the address and size of a section within a mapped image.
 *
 * @param[in]	pvImageBase		Base of the mapped image.
 * @param[in]	psSectionName	Name of the section to find.
 * @param[out]	ppvSection		Will receive the address of the section, if found.
 * @param[out]	pcbSection		Will receive the size of the section, if found.
 *
 * @returns NTSTATUS
 */
NTSTATUS
IMAGEPARSE_GetSection(
	_In_									PVOID			pvImageBase,
	_In_									PANSI_STRING	psSectionName,
	_Outptr_result_bytebuffer_(*pcbSection)	PVOID *			ppvSection,
	_Out_									PULONG			pcbSection
);
