/**
 * @file VgaDump.h
 * @author biko
 * @date 2016-07-29
 *
 * VgaDump module public header.
 * The module is responsible for capturing a dump
 * of the VGA video memory during a bugcheck.
 * The captured data is saved to the dump file.
 */
#pragma once

/** Headers *************************************************************/
#include <ntifs.h>


/** Functions ***********************************************************/

/**
 * Initializes the module.
 *
 * @returns NTSTATUS
 */
_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
VGADUMP_Initialize(VOID);

/**
 * Shuts down the module.
 */
_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
VGADUMP_Shutdown(VOID);
