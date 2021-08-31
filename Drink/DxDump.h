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

_IRQL_requires_(PASSIVE_LEVEL)
NTSTATUS
PAGEABLE
DXDUMP_Initialize(
	_In_	ULONG	nMaxWidth,
	_In_	ULONG	nMaxHeight
);

_IRQL_requires_(PASSIVE_LEVEL)
VOID
PAGEABLE
DXDUMP_Shutdown(VOID);
