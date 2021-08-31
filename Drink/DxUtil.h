/**
 * @file DxUtil.h
 * @author biko
 * @date 2021-08-31
 *
 * Various utilities for manipulating Dxgkrnl.
 */
#pragma once

/** Headers *************************************************************/
#include <ntifs.h>
#include <dispmprt.h>

#include "Util.h"


/** Macros **************************************************************/

#define DXUTIL_FREE_DISPLAY_DRIVERS(ptDrivers, nDrivers) \
	CLOSE_TO_VALUE_VARIADIC((ptDrivers), DXUTIL_FreeDisplayDrivers, NULL, (nDrivers))


/** Typedefs ************************************************************/

/**
 * @brief Contains information about a display driver.
*/
typedef struct _DISPLAY_DRIVER
{
	PDRIVER_OBJECT				ptDriverObject;
	PDRIVER_INITIALIZATION_DATA	ptInitializationData;
} DISPLAY_DRIVER, *PDISPLAY_DRIVER;
typedef DISPLAY_DRIVER CONST *PCDISPLAY_DRIVER;


/** Functions ***********************************************************/

/**
 * @brief Retrieves pointers to all loaded display drivers.
 *
 * @param pptDrivers	Will receive the driver information.
 * @param pnDrivers		Will receive the number of elements in the returned array.
 *
 * @return NTSTATUS
 *
 * @remark The returned buffer is pageable.
 * @remark Free the returned buffer with DXUTIL_FREE_DISPLAY_DRIVERS.
*/
_IRQL_requires_(PASSIVE_LEVEL)
PAGEABLE
NTSTATUS
DXUTIL_FindAllDisplayDrivers(
	_Outptr_result_buffer_(*pnDrivers)	PDISPLAY_DRIVER *	pptDrivers,
	_Out_								PULONG				pnDrivers
);

/**
 * @brief Frees the buffer returned from DXUTIL_FindAllDisplayDrivers.
 *
 * @param ptDrivers	The buffer to free.
 * @param nDrivers	The number of elements in the buffer.
 *
 * @remark Do not use this function directly. Use DXUTIL_FREE_DISPLAY_DRIVERS instead.
*/
_IRQL_requires_(PASSIVE_LEVEL)
PAGEABLE
VOID
DXUTIL_FreeDisplayDrivers(
	_In_reads_(nDrivers)	PDISPLAY_DRIVER	ptDrivers,
	_In_					ULONG			nDrivers
);
