/**
 * @file Driver.c
 * @author biko
 * @date 2016-07-29
 *
 * The driver's entry-point.
 */

/** Headers *************************************************************/
#include <ntifs.h>

#include "VgaDump.h"


/** Functions ***********************************************************/

/**
 * The driver's unload routine.
 *
 * @param[in]	ptDriverObject	Pointer to the driver object.
 */
STATIC
VOID
driver_Unload(
	_In_	PDRIVER_OBJECT	ptDriverObject
)
{
	PAGED_CODE();

#ifndef DBG
	UNREFERENCED_PARAMETER(ptDriverObject);
#endif // !DBG

	ASSERT(NULL != ptDriverObject);
	ASSERT(PASSIVE_LEVEL == KeGetCurrentIrql());

	VGADUMP_Shutdown();
}

/**
 * The driver's entry-point.
 *
 * @param[in]	ptDriverObject	Pointer to the driver object.
 * @param[in]	pusRegistryPath	Pointer to the driver's registry key.
 *								Unreferenced.
 *
 * @returns NTSTATUS
 */
NTSTATUS
DriverEntry(
	_In_	PDRIVER_OBJECT	ptDriverObject,
	_In_	PUNICODE_STRING	pusRegistryPath
)
{
	NTSTATUS	eStatus			= STATUS_UNSUCCESSFUL;
	BOOLEAN		bShutdownDump	= FALSE;

	PAGED_CODE();

	UNREFERENCED_PARAMETER(ptDriverObject);
	UNREFERENCED_PARAMETER(pusRegistryPath);

	ASSERT(PASSIVE_LEVEL == KeGetCurrentIrql());

	eStatus = VGADUMP_Initialize();
	if (!NT_SUCCESS(eStatus))
	{
		goto lblCleanup;
	}
	bShutdownDump = TRUE;

	// All done!
	bShutdownDump = FALSE;

	eStatus = STATUS_SUCCESS;

lblCleanup:
	if (bShutdownDump)
	{
		VGADUMP_Shutdown();
		bShutdownDump = FALSE;
	}

	return eStatus;
}
