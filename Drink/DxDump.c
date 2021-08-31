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
		__debugbreak();															\
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
	NT_ASSERT(NULL != pfnOriginal);

	if (NULL == g_ptShadowFramebuffer)
	{
		goto lblCleanup;
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
	SIZE_T				cbSize			= 0;
	PFRAMEBUFFER_DUMP	ptFramebuffer	= NULL;

	NT_ASSERT(DISPATCH_LEVEL >= KeGetCurrentIrql());

	eStatus = RtlSIZETMult(nMaxWidth, nMaxHeight, &cbSize);
	if (!NT_SUCCESS(eStatus))
	{
		goto lblCleanup;
	}

	// Allocate 4 bytes per pixel, as that's what Windows gives us
	eStatus = RtlSIZETMult(cbSize, 4, &cbSize);
	if (!NT_SUCCESS(eStatus))
	{
		goto lblCleanup;
	}

	// Add the header
	eStatus = RtlSIZETAdd(cbSize, FIELD_OFFSET(FRAMEBUFFER_DUMP, acPixels), &cbSize);
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
	PFRAMEBUFFER_DUMP	ptShadowFramebuffer	= NULL;
	PDISPLAY_DRIVER		ptDisplayDrivers	= NULL;
	ULONG				nDisplayDrivers		= 0;
	ULONG				nIndex				= 0;

	PAGED_CODE();
	NT_ASSERT(PASSIVE_LEVEL == KeGetCurrentIrql());

	eStatus = dxdump_AllocateFramebuffer(nMaxWidth, nMaxHeight, &ptShadowFramebuffer);
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

	// Transfer ownership:
	g_ptShadowFramebuffer = ptShadowFramebuffer;
	ptShadowFramebuffer = NULL;

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
	CLOSE(ptShadowFramebuffer, ExFreePool);

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
		g_atHookContexts[nIndex].ptInitializationData->DxgkDdiSystemDisplayWrite = g_atHookContexts[nIndex].pfnOriginal;
		CLOSE(g_atHookContexts[nIndex].ptDriverObject, ObfDereferenceObject);
		RtlZeroMemory(&g_atHookContexts[nIndex], sizeof(g_atHookContexts[nIndex]));
	}

	CLOSE(g_ptShadowFramebuffer, ExFreePool);
}
