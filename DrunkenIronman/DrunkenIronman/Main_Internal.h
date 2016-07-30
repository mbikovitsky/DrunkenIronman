/**
 * @file Main_Internal.h
 * @author biko
 * @date 2016-07-30
 *
 * Internal declarations for the Main module.
 */
#pragma once

/** Headers *************************************************************/
#include "Precomp.h"

#include <Drink.h>


/** Enums ***************************************************************/

/**
 * Command line argument positions for the application.
 */
typedef enum _CMD_ARGS
{
	CMD_ARG_APPLICATION = 0,
	CMD_ARG_SUBFUNCTION,

	// Must be last:
	CMD_ARGS_COUNT
} CMD_ARGS, *PCMD_ARGS;

/**
 * Command line argument positions for the "convert" subfunction
 * (no input argument).
 */
typedef enum _SUBFUNCTION_CONVERT_NO_INPUT_ARGS
{
	SUBFUNCTION_CONVERT_NO_INPUT_ARG_APPLICATION = 0,
	SUBFUNCTION_CONVERT_NO_INPUT_ARG_SUBFUNCTION,

	// Indicates the path to the resulting BMP file.
	SUBFUNCTION_CONVERT_NO_INPUT_ARG_OUTPUT,

	// Must be last:
	SUBFUNCTION_CONVERT_NO_INPUT_ARGS_COUNT
} SUBFUNCTION_CONVERT_NO_INPUT_ARGS, *PSUBFUNCTION_CONVERT_NO_INPUT_ARGS;

/**
 * Command line argument positions for the "convert" subfunction
 * (input and output arguments).
 */
typedef enum _SUBFUNCTION_CONVERT_ARGS
{
	SUBFUNCTION_CONVERT_ARG_APPLICATION = 0,
	SUBFUNCTION_CONVERT_ARG_SUBFUNCTION,

	// Indicates the path to the dump file.
	SUBFUNCTION_CONVERT_ARG_INPUT,

	// Indicates the path to the resulting BMP file.
	SUBFUNCTION_CONVERT_ARG_OUTPUT,

	// Must be last:
	SUBFUNCTION_CONVERT_ARGS_COUNT
} SUBFUNCTION_CONVERT_ARGS, *PSUBFUNCTION_CONVERT_ARGS;


/** Typedefs ************************************************************/

/**
 * Subfunction handler prototype.
 *
 * @param[in]	nArguments		Number of command line arguments.
 * @param[in]	ppwszArguments	The command line arguments.
 *
 * @returns HRESULT
 */
typedef
HRESULT
FN_SUBFUNCTION_HANDLER(
	_In_					INT				nArguments,
	_In_reads_(nArguments)	CONST PCWSTR *	ppwszArguments
);
typedef FN_SUBFUNCTION_HANDLER *PFN_SUBFUNCTION_HANDLER;

/**
 * Structure describing a single subfunction handler.
 */
typedef struct _SUBFUNCTION_HANDLER_ENTRY
{
	// The subfunction name.
	PCWSTR					pwszSubfunctionName;

	// The handler function.
	PFN_SUBFUNCTION_HANDLER	pfnHandler;
} SUBFUNCTION_HANDLER_ENTRY, *PSUBFUNCTION_HANDLER_ENTRY;
typedef CONST SUBFUNCTION_HANDLER_ENTRY *PCSUBFUNCTION_HANDLER_ENTRY;

/**
 * Structure of the finished BMP on disk.
 */
#pragma pack(push, 1)
typedef struct _VGA_BITMAP
{
	BITMAPFILEHEADER	tFileHeader;
	BITMAPINFOHEADER	tInfoHeader;
	RGBQUAD				atColors[VGA_DAC_PALETTE_ENTRIES];
	BYTE				anPixels[SCREEN_WIDTH_PIXELS * SCREEN_HEIGHT_PIXELS];
} VGA_BITMAP, *PVGA_BITMAP;
typedef CONST VGA_BITMAP *PCVGA_BITMAP;
#pragma pack(pop)


/** Functions ***********************************************************/

/**
 * Converts a VGA's DAC color (6-bit) to an RGB value (8-bit).
 * This function operates on a single color!
 *
 * @param[in]	nDacEntry	The DAC entry to convert.
 *
 * @returns BYTE
 */
STATIC
BYTE
main_VgaDacEntryToRgb(
	_In_	BYTE	nDacEntry
);

/**
 * Extracts the bit value of a pixel from a single VGA plane.
 * The bit values from all 4 planes should be combined
 * to obtain the index into the Palette RAM.
 *
 * @param[in]	pnPlane		The plane data.
 * @param[in]	nPixelIndex	Index of the pixel.
 *
 * @returns BYTE (contains just one bit, the LSB)
 */
STATIC
BYTE
main_GetPixelBitFromPlane(
	_In_	CONST BYTE *	pnPlane,
	_In_	DWORD			nPixelIndex
);

/**
 * Converts a VGA dump to a bitmap.
 *
 * @param[in]	ptDump		Dump to convert.
 * @param[out]	pptBitmap	Will receive the converted bitmap.
 *
 * @returns HRESULT
 */
STATIC
HRESULT
main_VgaDumpToBitmap(
	_In_		PCVGA_DUMP		ptDump,
	_Outptr_	PVGA_BITMAP *	pptBitmap
);

/**
 * Handler for the "convert" subfunction.
 * Extracts a VGA dump from a memory dump file
 * and converts it to a BMP file.
 *
 * @param[in]	nArguments		Number of command line arguments.
 * @param[in]	ppwszArguments	The command line arguments.
 *
 * @returns HRESULT
 *
 * @see SUBFUNCTION_CONVERT_NO_INPUT_ARGS
 *      SUBFUNCTION_CONVERT_ARGS
 */
STATIC
HRESULT
main_HandleConvert(
	_In_					INT				nArguments,
	_In_reads_(nArguments)	CONST PCWSTR *	ppwszArguments
);

/**
 * Handler for the "bugshot" subfunction.
 * Loads the driver and instructs it to take
 * a screenshot on the next bugheck.
 *
 * @param[in]	nArguments		Number of command line arguments.
 * @param[in]	ppwszArguments	The command line arguments.
 *
 * @returns HRESULT
 *
 * @see something
 */
STATIC
HRESULT
main_HandleBugshot(
	_In_					INT				nArguments,
	_In_reads_(nArguments)	CONST PCWSTR *	ppwszArguments
);
