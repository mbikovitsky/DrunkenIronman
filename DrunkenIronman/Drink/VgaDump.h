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
NTSTATUS
VGADUMP_Initialize(VOID);

/**
 * Shuts down the module.
 */
VOID
VGADUMP_Shutdown(VOID);
