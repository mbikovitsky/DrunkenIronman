/**
 * @file Main_Internal.h
 * @author biko
 * @date 2016-07-30
 *
 * Internal declarations for the Main module.
 */
#pragma once

/** Headers *************************************************************/
#include <Windows.h>

#include <Drink.h>


/** Constants ***********************************************************/

/**
 * Format string for the custom bugcheck message.
 */
#define VANITY_FORMAT_STRING ("%S\r\n")


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
	// Indicates the path to the resulting BMP file.
	SUBFUNCTION_CONVERT_NO_INPUT_ARG_OUTPUT = 0,

	// Must be last:
	SUBFUNCTION_CONVERT_NO_INPUT_ARGS_COUNT
} SUBFUNCTION_CONVERT_NO_INPUT_ARGS, *PSUBFUNCTION_CONVERT_NO_INPUT_ARGS;

/**
 * Command line argument positions for the "convert" subfunction
 * (input and output arguments).
 */
typedef enum _SUBFUNCTION_CONVERT_ARGS
{
	// Indicates the path to the dump file.
	SUBFUNCTION_CONVERT_ARG_INPUT = 0,

	// Indicates the path to the resulting BMP file.
	SUBFUNCTION_CONVERT_ARG_OUTPUT,

	// Must be last:
	SUBFUNCTION_CONVERT_ARGS_COUNT
} SUBFUNCTION_CONVERT_ARGS, *PSUBFUNCTION_CONVERT_ARGS;

/**
 * Command line argument positions for the "vanity" subfunction.
 */
typedef enum _SUBFUNCTION_VANITY_ARGS
{
	// The string to display on the BSoD.
	SUBFUNCTION_VANITY_ARG_STRING = 0,

	// Must be last:
	SUBFUNCTION_VANITY_ARGS_COUNT
} SUBFUNCTION_VANITY_ARGS, *PSUBFUNCTION_VANITY_ARGS;

/**
 * Command line argument positions for the "qr" subfunction.
 */
typedef enum _SUBFUNCTION_QR_ARGS
{
	SUBFUNCTION_QR_ARG_IMAGE = 0,

	// Must be last:
	SUBFUNCTION_QR_ARGS_COUNT
} SUBFUNCTION_QR_ARGS, *PSUBFUNCTION_QR_ARGS;
typedef SUBFUNCTION_QR_ARGS CONST *PCSUBFUNCTION_QR_ARGS;


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
 * Prints out the usage of the application.
 */
STATIC
VOID
main_PrintUsage(VOID);

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
 * @brief Gets the current QR bitmap information.
 *
 * @param[out] ptInfo Will receive the info.
 *
 * @return HRESULT
*/
STATIC
HRESULT
main_GetQrInfo(
	_Out_	PQR_INFO	ptInfo
);

/**
 * @brief Converts a bitmap file to be used as a bugcheck QR image.
 *
 * @param pwszFilename	Name of the bitmap file.
 * @param ppvPixels		Will receive the converted pixel data.
 * @param pcbPixels		Will receive the size of the pixel data, in bytes.
 *
 * @return HRESULT
 *
 * @remark Free the returned buffer to the process heap.
*/
STATIC
HRESULT
main_ConvertBitmapForQr(
	_In_									PCWSTR	pwszFilename,
	_Outptr_result_bytebuffer_(*pcbPixels)	PVOID *	ppvPixels,
	_Out_									PDWORD	pcbPixels
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
 * Handler for the "load" subfunction.
 * Loads the driver.
 *
 * @param[in]	nArguments		Ignored.
 * @param[in]	ppwszArguments	Ignored.
 *
 * @returns HRESULT
 */
STATIC
HRESULT
main_HandleLoad(
	_In_					INT				nArguments,
	_In_reads_(nArguments)	CONST PCWSTR *	ppwszArguments
);

/**
 * Handler for the "unload" subfunction.
 * Unloads the driver.
 *
 * @param[in]	nArguments		Ignored.
 * @param[in]	ppwszArguments	Ignored.
 *
 * @returns HRESULT
 */
STATIC
HRESULT
main_HandleUnload(
	_In_					INT				nArguments,
	_In_reads_(nArguments)	CONST PCWSTR *	ppwszArguments
);

/**
 * Handler for the "bugshot" subfunction.
 * Instructs the driver to take
 * a screenshot on the next bugheck.
 *
 * @param[in]	nArguments		Ignored.
 * @param[in]	ppwszArguments	Ignored.
 *
 * @returns HRESULT
 */
STATIC
HRESULT
main_HandleBugshot(
	_In_					INT				nArguments,
	_In_reads_(nArguments)	CONST PCWSTR *	ppwszArguments
);

/**
 * Handler for the "vanity" subfunction.
 * Instructs the driver to set a custom bugcheck
 * message and then crash the system.
 *
 * @param[in]	nArguments		Number of command line arguments.
 * @param[in]	ppwszArguments	The command line arguments.
 *
 * @returns HRESULT
 *
 * @see SUBFUNCTION_VANITY_ARGS
 */
STATIC
HRESULT
main_HandleVanity(
	_In_					INT				nArguments,
	_In_reads_(nArguments)	CONST PCWSTR *	ppwszArguments
);

/**
 * Handler for the "qr" subfunction.
 *
 * @param[in]	nArguments		Number of command line arguments.
 * @param[in]	ppwszArguments	The command line arguments.
 *
 * @returns HRESULT
 *
 * @see SUBFUNCTION_QR_ARGS
*/
STATIC
HRESULT
main_HandleQr(
	_In_					INT				nArguments,
	_In_reads_(nArguments)	PCWSTR CONST *	ppwszArguments
);
