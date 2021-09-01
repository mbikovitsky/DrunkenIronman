/**
 * @file DxDump.c
 * @author biko
 * @date 2021-08-31
 *
 * Implementation of framebuffer dump.
 */

/** Headers *************************************************************/
#include <ntifs.h>
#include <ntintsafe.h>

#include <Common.h>
#include <Drink.h>

#include "DxUtil.h"
#include "VgaDump.h"

#include "DxDump.h"


/** Macros **************************************************************/

#define TRAMPOLINE_NAME(nIndex) dxdump_HookTrampoline ## nIndex

#define DEFINE_TRAMPOLINE(nIndex)												\
	STATIC																		\
	VOID																		\
	TRAMPOLINE_NAME(nIndex) (													\
		PVOID	MiniportDeviceContext,											\
	    PVOID	Source,															\
	    UINT	SourceWidth,													\
	    UINT	SourceHeight,													\
	    UINT	SourceStride,													\
	    UINT	PositionX,														\
	    UINT	PositionY														\
	)																			\
	{																			\
		C_ASSERT((nIndex) < ARRAYSIZE(g_atHookContexts));						\
		dxdump_SystemDisplayWriteHook(MiniportDeviceContext,					\
									  Source,									\
									  SourceWidth,								\
									  SourceHeight,								\
									  SourceStride,								\
									  PositionX,								\
									  PositionY,								\
									  g_atHookContexts[nIndex].pfnOriginal);	\
	}


/** Typedefs ************************************************************/

typedef struct _HOOK_CONTEXT
{
	PDRIVER_OBJECT					ptDriverObject;
	PDRIVER_INITIALIZATION_DATA		ptInitializationData;
	PDXGKDDI_SYSTEM_DISPLAY_WRITE	pfnOriginal;
} HOOK_CONTEXT, *PHOOK_CONTEXT;
typedef HOOK_CONTEXT CONST *PCHOOK_CONTEXT;


/** Constants ***********************************************************/

#define DXDUMP_POOL_TAG ('mDxD')


/** Globals *************************************************************/

STATIC PFRAMEBUFFER_DUMP g_ptShadowFramebuffer = NULL;

STATIC HOOK_CONTEXT g_atHookContexts[5] = { 0 };

STATIC KBUGCHECK_REASON_CALLBACK_RECORD g_tCallbackRecord = { 0 };

STATIC BOOLEAN g_bCallbackRegistered = FALSE;


/** Functions ***********************************************************/

STATIC
VOID
dxdump_SystemDisplayWriteHook(
	_In_											PVOID							MiniportDeviceContext,
    _In_reads_bytes_(SourceHeight * SourceStride)	PVOID							Source,
    _In_											UINT							SourceWidth,
    _In_											UINT							SourceHeight,
    _In_											UINT							SourceStride,
    _In_											UINT							PositionX,
    _In_											UINT							PositionY,
	_In_											PDXGKDDI_SYSTEM_DISPLAY_WRITE	pfnOriginal
)
{
	UINT	cbFramebufferStride	= 0;
	PUCHAR	pcDstRow			= NULL;
	PUCHAR	pcSrcRow			= NULL;
	ULONG	cbToCopy			= 0;
	ULONG	nRows				= 0;
	ULONG	nCols				= 0;
	ULONG	nRow				= 0;

	NT_ASSERT(NULL != pfnOriginal);

	if (NULL == g_ptShadowFramebuffer || !g_ptShadowFramebuffer->bValid)
	{
		goto lblCleanup;
	}

	g_ptShadowFramebuffer->nMaxSeenWidth = max(g_ptShadowFramebuffer->nMaxSeenWidth, SourceWidth + PositionX);
	g_ptShadowFramebuffer->nMaxSeenHeight = max(g_ptShadowFramebuffer->nMaxSeenHeight, SourceHeight + PositionY);

	// The source is always 32 BPP, but sometimes it isn't, like when the output is actually to VGA.
	if (SourceStride < SourceWidth * 4)
	{
		// NOTE: The assumption here is that the MSDN is correct and the source image is
		// always 32 BPP. The only exception I know of is when the output is to VGA using
		// BasicDisplay.sys, then the source is actually 4 BPP. The source stride seems to be
		// a good indicator for this case, since in the case of 32 BPP the stride must be at
		// least cbToCopy. But I may be wrong.

		// Don't touch the framebuffer no more.
		g_ptShadowFramebuffer->bValid = FALSE;

		// Hand off to VGA dump module.
		(VOID)VGADUMP_Enable();

		// Time to leave.
		goto lblCleanup;
	}

	if (PositionY >= g_ptShadowFramebuffer->nHeight || PositionX >= g_ptShadowFramebuffer->nWidth)
	{
		goto lblCleanup;
	}

	cbFramebufferStride = g_ptShadowFramebuffer->nWidth * 4;

	pcDstRow = &g_ptShadowFramebuffer->acPixels[PositionY * cbFramebufferStride + PositionX * 4];
	pcSrcRow = Source;

	nRows = min(SourceHeight, g_ptShadowFramebuffer->nHeight - PositionY);
	nCols = min(SourceWidth, g_ptShadowFramebuffer->nWidth - PositionX);

	cbToCopy = nCols * 4;

	for (nRow = 0; nRow < nRows; ++nRow)
    {
        RtlMoveMemory(pcDstRow, pcSrcRow, cbToCopy);
        pcDstRow += cbFramebufferStride;
        pcSrcRow += SourceStride;
    }

lblCleanup:
	pfnOriginal(MiniportDeviceContext,
				Source,
				SourceWidth,
				SourceHeight,
				SourceStride,
				PositionX,
				PositionY);
}

DEFINE_TRAMPOLINE(0)
DEFINE_TRAMPOLINE(1)
DEFINE_TRAMPOLINE(2)
DEFINE_TRAMPOLINE(3)
DEFINE_TRAMPOLINE(4)

STATIC PDXGKDDI_SYSTEM_DISPLAY_WRITE CONST g_apfnTrampolines[] = {
	TRAMPOLINE_NAME(0),
	TRAMPOLINE_NAME(1),
	TRAMPOLINE_NAME(2),
	TRAMPOLINE_NAME(3),
	TRAMPOLINE_NAME(4)
};

C_ASSERT(ARRAYSIZE(g_apfnTrampolines) == ARRAYSIZE(g_atHookContexts));

STATIC
VOID
dxdump_BugCheckSecondaryDumpDataCallback(
	_In_	KBUGCHECK_CALLBACK_REASON			eReason,
	_In_	PKBUGCHECK_REASON_CALLBACK_RECORD	ptRecord,
	_Inout_	PVOID								pvReasonSpecificData,
	_In_	ULONG								cbReasonSpecificData
)
{
	PKBUGCHECK_SECONDARY_DUMP_DATA	ptSecondaryDumpData	= pvReasonSpecificData;
	ULONG							cbData				= 0;

#ifndef DBG
	UNREFERENCED_PARAMETER(eReason);
	UNREFERENCED_PARAMETER(ptRecord);
	UNREFERENCED_PARAMETER(cbReasonSpecificData);
#endif // !DBG

	NT_ASSERT(KbCallbackSecondaryDumpData == eReason);
	NT_ASSERT(NULL != ptRecord);
	NT_ASSERT(NULL != pvReasonSpecificData);
	NT_ASSERT(sizeof(*ptSecondaryDumpData) == cbReasonSpecificData);

	if (NULL == g_ptShadowFramebuffer || !g_ptShadowFramebuffer->bValid)
	{
		ptSecondaryDumpData->OutBuffer = NULL;
		ptSecondaryDumpData->OutBufferLength = 0;
		goto lblCleanup;
	}

	// This is safe since validation has already been performed in dxdump_AllocateFramebuffer
	cbData = UFIELD_OFFSET(FRAMEBUFFER_DUMP, acPixels[g_ptShadowFramebuffer->nWidth * g_ptShadowFramebuffer->nHeight * 4]);

	if (cbData > ptSecondaryDumpData->MaximumAllowed)
	{
		ptSecondaryDumpData->OutBuffer = NULL;
		ptSecondaryDumpData->OutBufferLength = 0;
		goto lblCleanup;
	}

	NT_ASSERT(
		(NULL == ptSecondaryDumpData->OutBuffer) ||
		(ptSecondaryDumpData->InBuffer == ptSecondaryDumpData->OutBuffer)
	);

	ptSecondaryDumpData->OutBuffer = g_ptShadowFramebuffer;
	ptSecondaryDumpData->OutBufferLength = cbData;
	ptSecondaryDumpData->Guid = g_tFramebufferDumpGuid;

lblCleanup:
	return;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
STATIC
NTSTATUS
dxdump_AllocateFramebuffer(
	_In_		ULONG				nMaxWidth,
	_In_		ULONG				nMaxHeight,
	_Outptr_	PFRAMEBUFFER_DUMP *	pptFramebuffer
)
{
	NTSTATUS			eStatus			= STATUS_UNSUCCESSFUL;
	ULONG				cbSize			= 0;
	PFRAMEBUFFER_DUMP	ptFramebuffer	= NULL;

	NT_ASSERT(DISPATCH_LEVEL >= KeGetCurrentIrql());

	eStatus = RtlULongMult(nMaxWidth, nMaxHeight, &cbSize);
	if (!NT_SUCCESS(eStatus))
	{
		goto lblCleanup;
	}

	// Allocate 4 bytes per pixel, as that's what Windows gives us
	eStatus = RtlULongMult(cbSize, 4, &cbSize);
	if (!NT_SUCCESS(eStatus))
	{
		goto lblCleanup;
	}

	// Add the header
	eStatus = RtlULongAdd(cbSize, FIELD_OFFSET(FRAMEBUFFER_DUMP, acPixels), &cbSize);
	if (!NT_SUCCESS(eStatus))
	{
		goto lblCleanup;
	}

	// Buffer has to be page-aligned, due to bugcheck callback requirements.
	// To make it page-aligned, ExAllocatePool needs the size to be at least a page.
	if (cbSize < PAGE_SIZE)
	{
		cbSize = PAGE_SIZE;
	}

	ptFramebuffer = ExAllocatePoolWithTag(NonPagedPoolNx, cbSize, DXDUMP_POOL_TAG);
	if (NULL == ptFramebuffer)
	{
		eStatus = STATUS_INSUFFICIENT_RESOURCES;
		goto lblCleanup;
	}
	RtlZeroMemory(ptFramebuffer, cbSize);
	ptFramebuffer->nWidth = nMaxWidth;
	ptFramebuffer->nHeight = nMaxHeight;
	ptFramebuffer->bValid = TRUE;

	// Transfer ownership:
	*pptFramebuffer = ptFramebuffer;
	ptFramebuffer = NULL;

	eStatus = STATUS_SUCCESS;

lblCleanup:
	CLOSE(ptFramebuffer, ExFreePool);

	return eStatus;
}

_Use_decl_annotations_
NTSTATUS
PAGEABLE
DXDUMP_Initialize(
	ULONG	nMaxWidth,
	ULONG	nMaxHeight
)
{
	NTSTATUS			eStatus				= STATUS_UNSUCCESSFUL;
	PDISPLAY_DRIVER		ptDisplayDrivers	= NULL;
	ULONG				nDisplayDrivers		= 0;
	ULONG				nIndex				= 0;

	PAGED_CODE();
	NT_ASSERT(PASSIVE_LEVEL == KeGetCurrentIrql());

	eStatus = dxdump_AllocateFramebuffer(nMaxWidth, nMaxHeight, &g_ptShadowFramebuffer);
	if (!NT_SUCCESS(eStatus))
	{
		goto lblCleanup;
	}

	eStatus = DXUTIL_FindAllDisplayDrivers(&ptDisplayDrivers, &nDisplayDrivers);
	if (!NT_SUCCESS(eStatus))
	{
		goto lblCleanup;
	}

	if (nDisplayDrivers > ARRAYSIZE(g_atHookContexts))
	{
		eStatus = STATUS_TOO_MANY_ADDRESSES;
		goto lblCleanup;
	}

	KeInitializeCallbackRecord(&g_tCallbackRecord);
	if (!KeRegisterBugCheckReasonCallback(&g_tCallbackRecord,
										  &dxdump_BugCheckSecondaryDumpDataCallback,
										  KbCallbackSecondaryDumpData,
										  (PUCHAR)"DxDump"))
	{
		eStatus = STATUS_BAD_DATA;
		goto lblCleanup;
	}
	g_bCallbackRegistered = TRUE;

	// NO FAILURE PAST THIS POINT
	// Unhooking is a pain here, so we prefer to not do that.

	for (nIndex = 0; nIndex < nDisplayDrivers; ++nIndex)
	{
		if (ptDisplayDrivers[nIndex].ptInitializationData->Version < DXGKDDI_INTERFACE_VERSION_WIN8)
		{
			continue;
		}

		if (NULL == ptDisplayDrivers[nIndex].ptInitializationData->DxgkDdiSystemDisplayWrite)
		{
			continue;
		}

		g_atHookContexts[nIndex].ptDriverObject = ptDisplayDrivers[nIndex].ptDriverObject;
		g_atHookContexts[nIndex].ptInitializationData = ptDisplayDrivers[nIndex].ptInitializationData;
		g_atHookContexts[nIndex].pfnOriginal = ptDisplayDrivers[nIndex].ptInitializationData->DxgkDdiSystemDisplayWrite;
		ptDisplayDrivers[nIndex].ptInitializationData->DxgkDdiSystemDisplayWrite = g_apfnTrampolines[nIndex];
		ObReferenceObject(ptDisplayDrivers[nIndex].ptDriverObject);
	}

	eStatus = STATUS_SUCCESS;

lblCleanup:
	DXUTIL_FREE_DISPLAY_DRIVERS(ptDisplayDrivers, nDisplayDrivers);
	if (!NT_SUCCESS(eStatus))
	{
		DXDUMP_Shutdown();
	}

	return eStatus;
}

_Use_decl_annotations_
VOID
PAGEABLE
DXDUMP_Shutdown(VOID)
{
	PAGED_CODE();
	NT_ASSERT(PASSIVE_LEVEL == KeGetCurrentIrql());

	ULONG	nIndex	= 0;

	for (nIndex = 0; nIndex < ARRAYSIZE(g_atHookContexts); ++nIndex)
	{
		if (NULL != g_atHookContexts[nIndex].ptInitializationData)
		{
			g_atHookContexts[nIndex].ptInitializationData->DxgkDdiSystemDisplayWrite = g_atHookContexts[nIndex].pfnOriginal;
		}
		CLOSE(g_atHookContexts[nIndex].ptDriverObject, ObfDereferenceObject);
		RtlZeroMemory(&g_atHookContexts[nIndex], sizeof(g_atHookContexts[nIndex]));
	}

	if (g_bCallbackRegistered)
	{
		(VOID)KeDeregisterBugCheckReasonCallback(&g_tCallbackRecord);
		g_bCallbackRegistered = FALSE;
	}

	CLOSE(g_ptShadowFramebuffer, ExFreePool);
}
