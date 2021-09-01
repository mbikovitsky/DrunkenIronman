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
 *
 * @remark After initialization, the module is _enabled_.
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

/**
 * @brief Instructs the module to actually save the VGA dump to disk.
 * 
 * @return The state of the module prior to calling this function.
*/
BOOLEAN
VGADUMP_Enable(VOID);

/**
 * @brief Instructs the module to _not_ save the VGA dump to disk.
 * 
 * @return The state of the module prior to calling this function.
*/
BOOLEAN
VGADUMP_Disable(VOID);

/**
 * @brief Determines whether the VGA dump will be saved to disk on BSoD.
 * 
 * @return The state of the module.
*/
BOOLEAN
VGADUMP_IsEnabled(VOID);
