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

/**
 * DAC read index register.
 * Writes to this register determine the index
 * of the next DAC entry to read.
 */
#define DAC_READ_INDEX_REG (0x3C7)

/**
 * DAC data register.
 * DAC entries are read from and written to here.
 */
#define DAC_DATA_REG (0x3C9)

/**
 * Graphics Controller index register.
 */
#define GC_INDEX_REG (0x3CE)

/**
 * Graphics Controller data register.
 */
#define GC_DATA_REG (0x3CF)

/**
 * Index of the GC Read Map register.
 */
#define GC_READ_MAP_INDEX (4)

/**
 * Index of the GC Mode register.
 */
#define GC_MODE_INDEX (5)


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
 * Counts how many times interrupts have been disabled.
 */
STATIC ULONG g_nInterruptDisableCount = 0;


/** Functions ***********************************************************/

/**
 * Determines whether interrupts are enabled
 * on the current processor.
 *
 * @returns BOOLEAN
 */
STATIC
BOOLEAN
vgadump_AreInterruptsEnabled(VOID)
{
	// Test the IF bit.
	// If it is set then interrupts are enabled.
	return BooleanFlagOn(__readeflags(), 1 << 9);
}

/**
 * Disables interrupts on the current processor.
 */
STATIC
VOID
vgadump_DisableInterrupts(VOID)
{
	if (0 == g_nInterruptDisableCount++)
	{
		_disable();
	}
}

/**
 * Enables interrupts on the current processor.
 */
STATIC
VOID
vgadump_EnableInterrupts(VOID)
{
	if (0 == --g_nInterruptDisableCount)
	{
		_enable();
	}
}

/**
 * Reads a byte from a VGA register.
 *
 * @param[in]	nIndexRegister	VGA index register.
 * @param[in]	nDataRegister	VGA data register.
 * @param[in]	nIndex			Index of the register to read.
 *
 * @returns The byte read.
 */
STATIC
UCHAR
vgadump_ReadRegisterByte(
	_In_	USHORT	nIndexRegister,
	_In_	USHORT	nDataRegister,
	_In_	UCHAR	nIndex
)
{
	UCHAR	nOldIndex	= 0;
	UCHAR	fValue		= 0;

	vgadump_DisableInterrupts();
	{
		// Save the previous value in the index register
		nOldIndex = __inbyte(nIndexRegister);

		// Set the new index
		__outbyte(nIndexRegister, nIndex);

		// Read byte from the data register
		fValue = __inbyte(nDataRegister);

		// Restore the index register
		__outbyte(nIndexRegister, nOldIndex);
	}
	vgadump_EnableInterrupts();

	return fValue;
}

/**
 * Writes a byte to a VGA register.
 *
 * @param[in]	nIndexRegister	VGA index register.
 * @param[in]	nDataRegister	VGA data register.
 * @param[in]	nIndex			Index of the register to read.
 * @param[in]	fValue			The value to write.
 */
STATIC
VOID
vgadump_WriteRegisterByte(
	_In_	USHORT	nIndexRegister,
	_In_	USHORT	nDataRegister,
	_In_	UCHAR	nIndex,
	_In_	UCHAR	fValue
)
{
	UCHAR	nOldIndex	= 0;

	vgadump_DisableInterrupts();
	{
		// Save the previous value in the index register
		nOldIndex = __inbyte(nIndexRegister);

		// Set the new index
		__outbyte(nIndexRegister, nIndex);

		// Write the supplied value
		__outbyte(nDataRegister, fValue);

		// Restore the index register
		__outbyte(nIndexRegister, nOldIndex);
	}
	vgadump_EnableInterrupts();
}

/**
 * Dumps the VGA's DAC palette to the given buffer.
 *
 * @param[out]	ptPaletteEntries	Will receive the palette's contents.
 *									This buffer must be large enough to contain
 *									the whole palette.
 */
STATIC
VOID
vgadump_DumpPalette(
	_Out_writes_all_(VGA_DAC_PALETTE_ENTRIES)	PPALETTE_ENTRY	ptPaletteEntries
)
{
	ULONG	nEntry	= 0;

	ASSERT(NULL != ptPaletteEntries);

	//
	// NOTE: We don't use the safe register functions
	//       here because these registers are not addressed
	//       using an index/data pair.
	//

	vgadump_DisableInterrupts();
	{
		// Set the first DAC index to read from
		__outbyte(DAC_READ_INDEX_REG, 0);

		for (nEntry = 0; nEntry < VGA_DAC_PALETTE_ENTRIES; ++nEntry)
		{
			ptPaletteEntries[nEntry].nRed =  __inbyte(DAC_DATA_REG);
			ptPaletteEntries[nEntry].nGreen = __inbyte(DAC_DATA_REG);
			ptPaletteEntries[nEntry].nBlue = __inbyte(DAC_DATA_REG);
		}
	}
	vgadump_EnableInterrupts();
}

/**
 * Dumps a single VGA plane.
 *
 * @param[in]	nPlane	Index of the plane to dump.
 * @param[out]	pvPlane	Will receive the plane's data.
 * @param[in]	cbPlane	Size of the output buffer, in bytes.
 */
STATIC
VOID
vgadump_DumpPlane(
	_In_							ULONG	nPlane,
	_Out_writes_bytes_all_(cbPlane)	PVOID	pvPlane,
	_In_							ULONG	cbPlane
)
{
	UCHAR	fOldGcMode	= 0;
	UCHAR	nOldPlane	= 0;

	ASSERT(nPlane < VGA_PLANES);
	ASSERT(NULL != pvPlane);
	ASSERT(0 != cbPlane);

	vgadump_DisableInterrupts();
	{
		// Set read mode 0
		fOldGcMode = vgadump_ReadRegisterByte(GC_INDEX_REG,
											  GC_DATA_REG,
											  GC_MODE_INDEX);
		vgadump_WriteRegisterByte(GC_INDEX_REG,
								  GC_DATA_REG,
								  GC_MODE_INDEX,
								  fOldGcMode & (~(1 << 3)));

		// Select the plane
		nOldPlane = vgadump_ReadRegisterByte(GC_INDEX_REG,
											 GC_DATA_REG,
											 GC_READ_MAP_INDEX);
		vgadump_WriteRegisterByte(GC_INDEX_REG,
								  GC_DATA_REG,
								  GC_READ_MAP_INDEX,
								  (UCHAR)nPlane);

		// Copy the video memory
		RtlMoveMemory(pvPlane, g_pvVgaBase, cbPlane);

		// Restore values
		vgadump_WriteRegisterByte(GC_INDEX_REG,
								  GC_DATA_REG,
								  GC_READ_MAP_INDEX,
								  nOldPlane);
		vgadump_WriteRegisterByte(GC_INDEX_REG,
								  GC_DATA_REG,
								  GC_MODE_INDEX,
								  fOldGcMode);
	}
	vgadump_EnableInterrupts();
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
	ULONG							nPlane				= 0;

#ifndef DBG
	UNREFERENCED_PARAMETER(eReason);
	UNREFERENCED_PARAMETER(ptRecord);
	UNREFERENCED_PARAMETER(cbReasonSpecificData);
#endif // !DBG

	ASSERT(KbCallbackSecondaryDumpData == eReason);
	ASSERT(NULL != ptRecord);
	ASSERT(NULL != pvReasonSpecificData);
	ASSERT(sizeof(*ptSecondaryDumpData) == cbReasonSpecificData);

	// Enforce correct behaviour of vgadump_EnableInterrupts
	// and vgadump_DisableInterrupts. If interrupts are currently
	// disabled, set the counter to 1, so that we don't erroneously
	// enable them on return from the callback.
	g_nInterruptDisableCount = vgadump_AreInterruptsEnabled() ? 0 : 1;

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
		vgadump_DumpPalette(g_tDump.atPaletteEntries);

		for (nPlane = 0; nPlane < VGA_PLANES; ++nPlane)
		{
			vgadump_DumpPlane(nPlane,
							  g_tDump.atPlanes[nPlane],
							  sizeof(g_tDump.atPlanes[nPlane]));
		}
	}

	ptSecondaryDumpData->OutBuffer = &g_tDump;
	ptSecondaryDumpData->OutBufferLength = sizeof(g_tDump);
	ptSecondaryDumpData->Guid = g_tVgaDumpGuid;

lblCleanup:
	return;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
VGADUMP_Initialize(VOID)
{
	NTSTATUS			eStatus				= STATUS_UNSUCCESSFUL;
	PHYSICAL_ADDRESS	pvVgaPhysicalBase	= { 0 };

	ASSERT(DISPATCH_LEVEL >= KeGetCurrentIrql());

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

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
VGADUMP_Shutdown(VOID)
{
	ASSERT(DISPATCH_LEVEL >= KeGetCurrentIrql());

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

//lblCleanup:
	return;
}
