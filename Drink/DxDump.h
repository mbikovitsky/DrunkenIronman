/**
 * @file DxDump.h
 * @author biko
 * @date 2021-08-31
 *
 * Module for dumping the video framebuffer at bugcheck time, on Windows 10+.
 */
#pragma once

/** Headers *************************************************************/
#include <ntifs.h>

#include "Util.h"


/** Functions ***********************************************************/

/**
 * @brief Initializes the module.
 *
 * @param[in] nMaxWidth		Maximum width of the saved framebuffer.
 * @param[in] nMaxHeight	Maximum height of the saved framebuffer.
 *
 * @return NTSTATUS
 *
 * @remark Prior to initializing the module, make sure VgaDump is initialized as well.
 * @remark If the resulting image is truncated, increase the resolution specified here.
*/
_IRQL_requires_(PASSIVE_LEVEL)
NTSTATUS
PAGEABLE
DXDUMP_Initialize(
	_In_	ULONG	nMaxWidth,
	_In_	ULONG	nMaxHeight
);

/**
 * @brief Shuts down the module.
*/
_IRQL_requires_(PASSIVE_LEVEL)
VOID
PAGEABLE
DXDUMP_Shutdown(VOID);
