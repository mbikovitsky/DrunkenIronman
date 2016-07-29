/**
 * @file VgaDump.c
 * @author biko
 * @date 2016-07-29
 *
 * VgaDump module implementation.
 */

/** Headers *************************************************************/
#include <ntifs.h>

#include <Drink.h>

#include "VgaDump.h"


/** Constants ***********************************************************/

/**
 * Physical base address of the VGA video memory.
 */
#define VGA_PHYSICAL_BASE (0xA0000)


/** Globals *************************************************************/

/**
 * Mapped VGA video memory base address.
 */
STATIC PVOID g_pvVgaBase = NULL;

 /**
 * VGA dump callback registration record.
 */
STATIC KBUGCHECK_REASON_CALLBACK_RECORD g_tCallbackRecord = { 0 };

/**
 * Indicates whether the callback has been registered.
 */
STATIC BOOLEAN g_bCallbackRegistered = FALSE;

/**
 * Holds the dump of the VGA memory.
 * Special alignment is due to bugcheck callback requirements.
 * See the MSDN for more information.
 */
STATIC DECLSPEC_ALIGN(PAGE_SIZE) VGA_DUMP g_tDump = { 0 };

/**
 * {c2b07ffc-519a-45e2-8a97-c7b24291182c}
 * GUID for tagging the saved VGA dump in the dump file.
 */
STATIC CONST GUID g_tDumpGuid = 
{ 0xc2b07ffc, 0x519a, 0x45e2, { 0x8a, 0x97, 0xc7, 0xb2, 0x42, 0x91, 0x18, 0x2c } };


/** Functions ***********************************************************/

/**
 * Dumps the VGA's DAC palette to the given buffer.
 *
 * @param[out]	pnPalette	Will receive the palette's contents.
 *							This buffer must be large enough to contain
 *							the whole palette.
 *
 * @returns description
 */
STATIC
VOID
vgadump_DumpPalette(
	_Out_	PULONG	pnPalette
)
{
	ULONG	nEntry	= 0;

	ASSERT(NULL != pnPalette);

	// DAC Address Read Mode Register
	__outbyte(0x3C7, 0);

	for (nEntry = 0; nEntry < VGA_DAC_PALETTE_ENTRIES; ++nEntry)
	{
		pnPalette[nEntry] = __inbyte(0x3C9);		// Red
		pnPalette[nEntry] |= __inbyte(0x3C9) << 8;	// Green
		pnPalette[nEntry] |= __inbyte(0x3C9) << 16;	// Blue
	}
}

/**
 * Bugcheck callback for dumping the VGA video memory.
 *
 * @param[in]		eReason					Specifies the situation in which the callback is executed.
 *											Always KbCallbackSecondaryDumpData.
 * @param[in]		ptRecord				Pointer to the registration record for this callback.
 * @param[in,out]	pvReasonSpecificData	Pointer to a KBUGCHECK_SECONDARY_DUMP_DATA structure.
 * @param[in]		cbReasonSpecificData	Size of the buffer pointer to by pvReasonSpecificData.
 *											Always sizeof(KBUGCHECK_SECONDARY_DUMP_DATA).
 */
STATIC
VOID
vgadump_BugCheckSecondaryDumpDataCallback(
	_In_	KBUGCHECK_CALLBACK_REASON			eReason,
	_In_	PKBUGCHECK_REASON_CALLBACK_RECORD	ptRecord,
	_Inout_	PVOID								pvReasonSpecificData,
	_In_	ULONG								cbReasonSpecificData
)
{
	PKBUGCHECK_SECONDARY_DUMP_DATA	ptSecondaryDumpData	= (PKBUGCHECK_SECONDARY_DUMP_DATA)pvReasonSpecificData;

	ASSERT(KbCallbackSecondaryDumpData == eReason);
	ASSERT(NULL != ptRecord);
	ASSERT(NULL != pvReasonSpecificData);
	ASSERT(sizeof(*ptSecondaryDumpData) == cbReasonSpecificData);

	if (sizeof(g_tDump) > ptSecondaryDumpData->MaximumAllowed)
	{
		ptSecondaryDumpData->OutBuffer = NULL;
		ptSecondaryDumpData->OutBufferLength = 0;
		goto lblCleanup;
	}

	ASSERT(
		(NULL == ptSecondaryDumpData->OutBuffer) ||
		(ptSecondaryDumpData->InBuffer == ptSecondaryDumpData->OutBuffer)
	);

	// First time around, fill the dump data.
	if (NULL == ptSecondaryDumpData->OutBuffer)
	{
		vgadump_DumpPalette(g_tDump.anPalette);
	}

	ptSecondaryDumpData->OutBuffer = &g_tDump;
	ptSecondaryDumpData->OutBufferLength = sizeof(g_tDump);
	ptSecondaryDumpData->Guid = g_tDumpGuid;

lblCleanup:
	return;
}

NTSTATUS
VGADUMP_Initialize(VOID)
{
	NTSTATUS			eStatus				= STATUS_UNSUCCESSFUL;
	PHYSICAL_ADDRESS	pvVgaPhysicalBase	= { 0 };

	// Map the VGA video memory so that we'll be able
	// to access it in protected mode.
	pvVgaPhysicalBase.QuadPart = VGA_PHYSICAL_BASE;
	g_pvVgaBase = MmMapIoSpace(pvVgaPhysicalBase,
							   sizeof(VGA_PLANE_DUMP),
							   MmNonCached);
	if (NULL == g_pvVgaBase)
	{
		eStatus = STATUS_INSUFFICIENT_RESOURCES;
		goto lblCleanup;
	}

	KeInitializeCallbackRecord(&g_tCallbackRecord);
	if (!KeRegisterBugCheckReasonCallback(&g_tCallbackRecord,
										  &vgadump_BugCheckSecondaryDumpDataCallback,
										  KbCallbackSecondaryDumpData,
										  (PUCHAR)"VgaDump"))
	{
		eStatus = STATUS_BAD_DATA;
		goto lblCleanup;
	}
	g_bCallbackRegistered = TRUE;

	eStatus = STATUS_SUCCESS;

lblCleanup:
	if (!NT_SUCCESS(eStatus))
	{
		VGADUMP_Shutdown();
	}

	return eStatus;
}

VOID
VGADUMP_Shutdown(VOID)
{
	if (g_bCallbackRegistered)
	{
		(VOID)KeDeregisterBugCheckReasonCallback(&g_tCallbackRecord);
		g_bCallbackRegistered = FALSE;
	}

	if (NULL != g_pvVgaBase)
	{
		MmUnmapIoSpace(g_pvVgaBase, sizeof(VGA_PLANE_DUMP));
		g_pvVgaBase = NULL;
	}
}
