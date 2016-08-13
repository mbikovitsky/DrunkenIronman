/**
 * @file DrinkControl.h
 * @author biko
 * @date 2016-08-07
 *
 * DrinkControl module public header.
 * Contains routines for manipulation of the
 * driver service.
 */
#pragma once

/** Headers *************************************************************/
#include <Windows.h>


/** Functions ***********************************************************/

/**
 * Loads the driver.
 *
 * @returns HRESULT
 */
HRESULT
DRINKCONTROL_LoadDriver(VOID);

/**
 * Unloads the driver.
 *
 * @returns HRESULT
 */
HRESULT
DRINKCONTROL_UnloadDriver(VOID);

/**
 * Sends a control request to the driver.
 *
 * @param[in]	eControlCode	Control code to send.
 * @param[in]	pvInputBuffer	Input buffer for the control request.
 * @param[in]	cbInputBuffer	Size of the input buffer, in bytes.
 *
 * @returns HRESULT
 */
HRESULT
DRINKCONTROL_ControlDriver(
	_In_								DWORD	eControlCode,
	_In_reads_bytes_opt_(cbInputBuffer)	PVOID	pvInputBuffer,
	_In_								DWORD	cbInputBuffer
);
