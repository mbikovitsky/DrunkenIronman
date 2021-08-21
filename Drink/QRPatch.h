/**
 * @file QRPatch.h
 * @author biko
 * @date 2021-08-20
 *
 * Patching of the QR bugcheck image on Windows 10.
 */
#pragma once

/** Headers *************************************************************/
#include <ntifs.h>

#include "Util.h"


/** Typedefs ************************************************************/

typedef struct _BITMAP_INFO
{
	ULONG	nWidth;
	ULONG	nHeight;
	ULONG	nBitCount;
} BITMAP_INFO, *PBITMAP_INFO;
typedef BITMAP_INFO CONST *PCBITMAP_INFO;


/** Functions ***********************************************************/

/**
 * @brief Initialized the module.
 *
 * @return NTSTATUS
*/
_IRQL_requires_(PASSIVE_LEVEL)
PAGEABLE
NTSTATUS
QRPATCH_Initialize(VOID);

/**
 * @brief Retrieves information about the current QR bitmap.
 *
 * @param ptBitmapInfo Will receive the information.
 *
 * @return NTSTATUS
*/
NTSTATUS
QRPATCH_GetBitmapInfo(
	_Out_	PBITMAP_INFO	ptBitmapInfo
);
