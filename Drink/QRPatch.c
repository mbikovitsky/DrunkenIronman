/**
 * @file QRPatch.c
 * @author biko
 * @date 2021-08-20
 *
 * Patching of the QR bugcheck image on Windows 10 - implementation.
 */

/** Headers *************************************************************/
#include <ntifs.h>

#include <Common.h>

#include "Util.h"
#include "Match.h"
#include "ImageParse.h"

#include "QRPatch.h"


/** Typedefs ************************************************************/

typedef
PVOID
NTAPI
FN_BG_GET_DISPLAY_CONTEXT(VOID);
typedef FN_BG_GET_DISPLAY_CONTEXT *PFN_BG_GET_DISPLAY_CONTEXT;

typedef struct _RECTANGLE
{
	ULONG	nHeight;
	ULONG	nWidth;
	ULONG	nBitCount;
	ULONG	cbPixels;
	ULONG	fFlags;
	PVOID	pvPixels;
	UCHAR	acReserved[40];
} RECTANGLE, *PRECTANGLE;
typedef RECTANGLE CONST *PCRECTANGLE;


/** Constants ***********************************************************/

#ifdef _M_X64
#define QR_RECTANGLE_POINTER_OFFSET (0xF8)
#elif _M_IX86
#define QR_RECTANGLE_POINTER_OFFSET (0xD0)
#else
#error Unsupported architecture
#endif


/** Globals *************************************************************/

#ifdef _M_X64
STATIC PATTERN_ELEMENT CONST g_atGetDisplayContextPattern[] = {
	MATCH_EXACT(0x48),	// LEA RAX, [RIP + ?]
	MATCH_EXACT(0x8D),
	MATCH_EXACT(0x05),
	MATCH_ANY,
	MATCH_ANY,
	MATCH_ANY,
	MATCH_ANY,
	MATCH_EXACT(0xC3)	// RET
};
#elif _M_IX86
STATIC PATTERN_ELEMENT CONST g_atGetDisplayContextPattern[] = {
	MATCH_EXACT(0xB8),	// MOV EAX, ?
	MATCH_ANY,
	MATCH_ANY,
	MATCH_ANY,
	MATCH_ANY,
	MATCH_EXACT(0xC3)	// RET
};
#else
#error Unsupported architecture
#endif

STATIC PRECTANGLE g_ptQrRectangle = NULL;


/** Functions ***********************************************************/

_Use_decl_annotations_
PAGEABLE
NTSTATUS
QRPATCH_Initialize(VOID)
{
	NTSTATUS					eStatus					= STATUS_UNSUCCESSFUL;
	PAUX_MODULE_EXTENDED_INFO	ptModules				= NULL;
	ULONG						nModules				= 0;
	STRING						sCodeSectionName		= RTL_CONSTANT_STRING("PAGEBGFX");
	PVOID						pvCodeSection			= NULL;
	ULONG						cbCodeSection			= 0;
#ifdef _M_IX86
	STRING						sDataSectionName		= RTL_CONSTANT_STRING(".data");
	PVOID						pvDataSection			= NULL;
	ULONG						cbDataSection			= 0;
#endif
	PUCHAR						pcCurrent				= NULL;
	BOOLEAN						bMatch					= FALSE;
	PFN_BG_GET_DISPLAY_CONTEXT	pfnBgGetDisplayContext	= NULL;
	PVOID						pvDisplayContext		= NULL;
	PRECTANGLE					ptRectangle				= NULL;

	PAGED_CODE();
	NT_ASSERT(PASSIVE_LEVEL == KeGetCurrentIrql());

	if (!UTIL_IsWindows10OrGreater())
	{
		eStatus = STATUS_REVISION_MISMATCH;
		goto lblCleanup;
	}

	eStatus = UTIL_QueryModuleInformation(&ptModules, &nModules);
	if (!NT_SUCCESS(eStatus))
	{
		goto lblCleanup;
	}

	eStatus = IMAGEPARSE_GetSection(ptModules[0].BasicInfo.ImageBase, &sCodeSectionName, &pvCodeSection, &cbCodeSection);
	if (!NT_SUCCESS(eStatus))
	{
		goto lblCleanup;
	}

#ifdef _M_IX86
	eStatus = IMAGEPARSE_GetSection(ptModules[0].tBasicInfo.pvImageBase, &sDataSectionName, &pvDataSection, &cbDataSection);
	if (!NT_SUCCESS(eStatus))
	{
		goto lblCleanup;
	}
#endif

	if (cbCodeSection < ARRAYSIZE(g_atGetDisplayContextPattern))
	{
		eStatus = STATUS_BUFFER_TOO_SMALL;
		goto lblCleanup;
	}

	for (pcCurrent = (PUCHAR)pvCodeSection;
		 pcCurrent <= (PUCHAR)RtlOffsetToPointer(pvCodeSection, cbCodeSection - ARRAYSIZE(g_atGetDisplayContextPattern));
		 pcCurrent += 1)
	{
		eStatus = MATCH_IsMatch(g_atGetDisplayContextPattern,
								pcCurrent,
								ARRAYSIZE(g_atGetDisplayContextPattern),
								&bMatch);
		if (!NT_SUCCESS(eStatus))
		{
			goto lblCleanup;
		}

		if (bMatch)
		{
#ifdef _M_IX86
			{
				ULONG_PTR	pvAddress	= *(ULONG_PTR UNALIGNED *)(pcCurrent + 1);

				// We're matching against MOV EAX, ? so check that the address is within the data
				// section and not just some random number.
				if (pvAddress < (ULONG_PTR)pvDataSection || pvAddress >= (ULONG_PTR)RtlOffsetToPointer(pvDataSection, cbDataSection))
				{
					continue;
				}
			}
#endif

			if (NULL != pfnBgGetDisplayContext)
			{
				// More than one hit
				eStatus = STATUS_MULTIPLE_FAULT_VIOLATION;
				goto lblCleanup;
			}

			pfnBgGetDisplayContext = (PFN_BG_GET_DISPLAY_CONTEXT)pcCurrent;

			// Continue iteration in case we find another instance
		}
	}
	if (NULL == pfnBgGetDisplayContext)
	{
		eStatus = STATUS_NOT_FOUND;
		goto lblCleanup;
	}

	pvDisplayContext = pfnBgGetDisplayContext();
	if (NULL == pvDisplayContext)
	{
		eStatus = STATUS_UNEXPECTED_IO_ERROR;
		goto lblCleanup;
	}

	ptRectangle = *(PRECTANGLE *)RtlOffsetToPointer(pvDisplayContext, QR_RECTANGLE_POINTER_OFFSET);
	if (NULL == ptRectangle)
	{
		eStatus = STATUS_NOT_FOUND;
		goto lblCleanup;
	}

	g_ptQrRectangle = ptRectangle;

	eStatus = STATUS_SUCCESS;

lblCleanup:
	CLOSE(ptModules, ExFreePool);

	return eStatus;
}

_Use_decl_annotations_
NTSTATUS
QRPATCH_GetBitmapInfo(
	PBITMAP_INFO	ptBitmapInfo
)
{
	NTSTATUS	eStatus	= STATUS_UNSUCCESSFUL;

	if (NULL == ptBitmapInfo)
	{
		eStatus = STATUS_INVALID_PARAMETER;
		goto lblCleanup;
	}

	if (NULL == g_ptQrRectangle)
	{
		eStatus = STATUS_INVALID_DEVICE_STATE;
		goto lblCleanup;
	}

	ptBitmapInfo->nWidth = g_ptQrRectangle->nWidth;
	ptBitmapInfo->nHeight = g_ptQrRectangle->nHeight;
	ptBitmapInfo->nBitCount = g_ptQrRectangle->nBitCount;

	eStatus = STATUS_SUCCESS;

lblCleanup:
	return eStatus;
}

_Use_decl_annotations_
NTSTATUS
QRPATCH_SetBitmap(
	PVOID	pvPixels,
	ULONG	cbPixels
)
{
	NTSTATUS	eStatus	= STATUS_UNSUCCESSFUL;

	if (NULL == pvPixels)
	{
		eStatus = STATUS_INVALID_PARAMETER;
		goto lblCleanup;
	}

	if (NULL == g_ptQrRectangle)
	{
		eStatus = STATUS_INVALID_DEVICE_STATE;
		goto lblCleanup;
	}

	if (cbPixels != g_ptQrRectangle->cbPixels)
	{
		eStatus = STATUS_INVALID_PARAMETER;
		goto lblCleanup;
	}

	RtlMoveMemory(g_ptQrRectangle->pvPixels, pvPixels, cbPixels);

	eStatus = STATUS_SUCCESS;

lblCleanup:
	return eStatus;
}
