/**
 * @file Main.c
 * @author biko
 * @date 2016-07-30
 *
 * The application's entry-point.
 */

/** Headers *************************************************************/
#include <Windows.h>
#include <intsafe.h>
#include <strsafe.h>

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

#include <Drink.h>

#include "DrinkControl.h"
#include "Util.h"
#include "DumpParse.h"
#include "Resource.h"
#include "Debug.h"

#include "Main_Internal.h"


/** Globals *************************************************************/

/**
 * Array of the registered subfunction handlers.
 */
STATIC CONST SUBFUNCTION_HANDLER_ENTRY g_atSubfunctionHandlers[] = {
	{
		L"convert",
		&main_HandleConvert
	},

	{
		L"load",
		&main_HandleLoad
	},

	{
		L"unload",
		&main_HandleUnload
	},

	{
		L"bugshot",
		&main_HandleBugshot
	},

	{
		L"vanity",
		&main_HandleVanity
	},

	{
		L"qr",
		&main_HandleQr
	}
};


/** Functions ***********************************************************/

_Use_decl_annotations_
STATIC
VOID
main_PrintUsage(VOID)
{
	PWSTR	pwszExecutablePath	= NULL;
	PCWSTR	pwszExecutableName	= NULL;

	if (0 != _get_wpgmptr(&pwszExecutablePath))
	{
		goto lblCleanup;
	}
	assert(NULL != pwszExecutablePath);

	// Retrieve the name of the executable.
	pwszExecutableName = wcsrchr(pwszExecutablePath, L'\\');
	pwszExecutableName =
		(NULL == pwszExecutableName)
		? (pwszExecutablePath)
		: (pwszExecutableName + 1);

	// Print the banner
	(VOID)fwprintf(stderr,
				   L"Copyright (c) 2016-2021 Michael Bikovitsky\n\n%s <subfunction> <subfunction args>\n\n",
				   pwszExecutableName);

	(VOID)fwprintf(stderr,
				   L"  convert [<input>] <output>\n    Extracts a screenshot from a memory dump.\n");

	(VOID)fwprintf(stderr,
				   L"  load\n    Loads the driver.\n");

	(VOID)fwprintf(stderr,
				   L"  unload\n    Unloads the driver.\n");

	(VOID)fwprintf(stderr,
				   L"  bugshot\n    Instructs the driver to capture a screenshot\n    of the next BSoD.\n");

	(VOID)fwprintf(stderr,
				   L"  vanity <string>\n    Crashes the system and displays the specified string\n    on the BSoD.\n");

	(VOID)fwprintf(stderr,
				   L"  qr\n    Displays the dimensions of the current QR image.\n");

	(VOID)fwprintf(stderr,
				   L"  qr <image>\n    Sets an image to be used instead of the default QR code.\n    The image must be a non-compressed BMP\n    with either 32 or 24 BPP, and with the same dimensions\n    as the default QR image.\n");

	(VOID)fwprintf(stderr, L"\n");

lblCleanup:
	return;
}

STATIC
BYTE
main_VgaDacEntryToRgb(
	_In_	BYTE	nDacEntry
)
{
	return nDacEntry * (256 / 64);
}

STATIC
BYTE
main_GetPixelBitFromPlane(
	_In_	CONST BYTE *	pnPlane,
	_In_	DWORD			nPixelIndex
)
{
	BYTE	nByteContainingPixel	= 0;
	BYTE	nBit					= 0;

	assert(NULL != pnPlane);

	// Find the byte containing the pixel data required
	nByteContainingPixel = pnPlane[nPixelIndex / PIXELS_IN_BYTE];

	// Move the relevant bit to be the MSB
	nBit = nByteContainingPixel;
	nBit <<= (nPixelIndex % PIXELS_IN_BYTE);

	// Move the relevant bit to be the LSB
	nBit >>= (PIXELS_IN_BYTE - 1);

	return nBit;
}

STATIC
HRESULT
main_VgaDumpToBitmap(
	_In_		PCVGA_DUMP		ptDump,
	_Outptr_	PVGA_BITMAP *	pptBitmap
)
{
	HRESULT		hrResult		= E_FAIL;
	PVGA_BITMAP	ptBitmap		= NULL;
	DWORD		nCurrentEntry	= 0;
	DWORD		nCurrentPixel	= 0;
	DWORD		nCurrentPlane	= 0;
	BYTE		nCurrentBit		= 0;

	PROGRESS("Converting raw VGA dump to BMP...");

	ptBitmap = HEAPALLOC(sizeof(*ptBitmap));
	if (NULL == ptBitmap)
	{
		PROGRESS("Oops. Ran out of memory.");
		hrResult = E_OUTOFMEMORY;
		goto lblCleanup;
	}

	PROGRESS("Writing the BMP header.");

	// Initialize the file header
	ptBitmap->tFileHeader.bfType = 'MB';
	ptBitmap->tFileHeader.bfSize = sizeof(*ptBitmap);
	ptBitmap->tFileHeader.bfOffBits = FIELD_OFFSET(VGA_BITMAP, anPixels);

	// Initialize the info header
	ptBitmap->tInfoHeader.biSize = sizeof(ptBitmap->tInfoHeader);
	ptBitmap->tInfoHeader.biWidth = SCREEN_WIDTH_PIXELS;
	ptBitmap->tInfoHeader.biHeight = -SCREEN_HEIGHT_PIXELS;		// Negative because otherwise the bitmap
																// is bottom-up :)
	ptBitmap->tInfoHeader.biPlanes = 1;
	ptBitmap->tInfoHeader.biBitCount = 8;
	ptBitmap->tInfoHeader.biCompression = BI_RGB;

	// Initialize the color palette
	PROGRESS("Writing the palette.");
	for (nCurrentEntry = 0;
		 nCurrentEntry < ARRAYSIZE(ptBitmap->atColors);
		 ++nCurrentEntry)
	{
		ptBitmap->atColors[nCurrentEntry].rgbRed = main_VgaDacEntryToRgb(ptDump->atPaletteEntries[nCurrentEntry].nRed);
		ptBitmap->atColors[nCurrentEntry].rgbGreen = main_VgaDacEntryToRgb(ptDump->atPaletteEntries[nCurrentEntry].nGreen);
		ptBitmap->atColors[nCurrentEntry].rgbBlue = main_VgaDacEntryToRgb(ptDump->atPaletteEntries[nCurrentEntry].nBlue);
	}

	// Set the pixel values
	PROGRESS("Writing the pixel data.");
	for (nCurrentPixel = 0;
		 nCurrentPixel < ARRAYSIZE(ptBitmap->anPixels);
		 ++nCurrentPixel)
	{
		for (nCurrentPlane = 0;
			 nCurrentPlane < ARRAYSIZE(ptDump->atPlanes);
			 ++nCurrentPlane)
		{
			nCurrentBit = main_GetPixelBitFromPlane(ptDump->atPlanes[nCurrentPlane],
													nCurrentPixel);
			ptBitmap->anPixels[nCurrentPixel] |= nCurrentBit << nCurrentPlane;
		}
	}

	// Transfer ownership:
	*pptBitmap = ptBitmap;
	ptBitmap = NULL;

	hrResult = S_OK;

lblCleanup:
	HEAPFREE(ptBitmap);

	return hrResult;
}

STATIC
HRESULT
main_FramebufferDumpToBitmap(
	_In_		PFRAMEBUFFER_DUMP		ptDump,
	_Outptr_	PFRAMEBUFFER_BITMAP *	pptBitmap
)
{
	HRESULT				hrResult		= E_FAIL;
	PFRAMEBUFFER_BITMAP	ptBitmap		= NULL;

	// Invalid dumps will never be written by the kernel.
	assert(ptDump->bValid);

	PROGRESS("Converting framebuffer dump to BMP...");

	if (ptDump->nMaxSeenWidth > ptDump->nWidth || ptDump->nMaxSeenHeight > ptDump->nHeight)
	{
		PROGRESS("Image truncated. Actual size was %lux%lu, but saved only %lux%lu",
				 ptDump->nMaxSeenWidth,
				 ptDump->nMaxSeenHeight,
				 ptDump->nWidth,
				 ptDump->nHeight);
	}

	// Should be safe since the kernel already performed the same calculation.
	ptBitmap = HEAPALLOC(FIELD_OFFSET(FRAMEBUFFER_BITMAP, acPixels[ptDump->nWidth * ptDump->nHeight * 4]));
	if (NULL == ptBitmap)
	{
		PROGRESS("Oops. Ran out of memory.");
		hrResult = E_OUTOFMEMORY;
		goto lblCleanup;
	}

	PROGRESS("Writing the BMP header.");

	// Initialize the file header
	ptBitmap->tFileHeader.bfType = 'MB';
	ptBitmap->tFileHeader.bfSize = FIELD_OFFSET(FRAMEBUFFER_BITMAP, acPixels[ptDump->nWidth * ptDump->nHeight * 4]);
	ptBitmap->tFileHeader.bfOffBits = FIELD_OFFSET(FRAMEBUFFER_BITMAP, acPixels);

	// Initialize the info header
	ptBitmap->tInfoHeader.biSize = sizeof(ptBitmap->tInfoHeader);
	ptBitmap->tInfoHeader.biWidth = ptDump->nWidth;
	ptBitmap->tInfoHeader.biHeight = -ptDump->nHeight;		// Negative because otherwise the bitmap
															// is bottom-up :)
	ptBitmap->tInfoHeader.biPlanes = 1;
	ptBitmap->tInfoHeader.biBitCount = 32;
	ptBitmap->tInfoHeader.biCompression = BI_RGB;

	// Set the pixel values
	PROGRESS("Writing the pixel data.");
	MoveMemory(ptBitmap->acPixels, ptDump->acPixels, ptDump->nWidth * ptDump->nHeight * 4);

	// Transfer ownership:
	*pptBitmap = ptBitmap;
	ptBitmap = NULL;

	hrResult = S_OK;

lblCleanup:
	HEAPFREE(ptBitmap);

	return hrResult;
}

_Use_decl_annotations_
STATIC
HRESULT
main_GetQrInfo(
	PQR_INFO	ptInfo
)
{
	HRESULT	hrResult	= E_FAIL;
	DWORD	cbReturned	= 0;

	assert(NULL != ptInfo);

	hrResult = DRINKCONTROL_ControlDriver(IOCTL_DRINK_QR_INFO,
										  NULL, 0,
										  ptInfo, sizeof(*ptInfo), &cbReturned);
	if (FAILED(hrResult))
	{
		PROGRESS("Failed retrieving information (0x%08lX).", hrResult);
		goto lblCleanup;
	}

	if (cbReturned < sizeof(*ptInfo))
	{
		PROGRESS("Returned buffer is unexpectedly small (%lu).", cbReturned);
	}
	assert(sizeof(*ptInfo) == cbReturned);

	hrResult = S_OK;

lblCleanup:
	return hrResult;
}

_Use_decl_annotations_
STATIC
HRESULT
main_ConvertBitmapForQr(
	PCWSTR	pwszFilename,
	PVOID *	ppvPixels,
	PDWORD	pcbPixels
)
{
	HRESULT				hrResult		= E_FAIL;
	QR_INFO				tQrInfo			= { 0 };
	PVOID				pvBitmap		= NULL;
	SIZE_T				cbBitmap		= 0;
	PBITMAPFILEHEADER	ptFileHeader	= NULL;
	PBITMAPINFOHEADER	ptInfoHeader	= NULL;
	PVOID				pvPixels		= NULL;
	DWORD				cbPixels		= 0;

	assert(NULL != pwszFilename);
	assert(NULL != ppvPixels);
	assert(NULL != pcbPixels);

	hrResult = main_GetQrInfo(&tQrInfo);
	if (FAILED(hrResult))
	{
		PROGRESS("main_GetQrInfo failed with code 0x%08lX.", hrResult);
		goto lblCleanup;
	}

	// Kernel also supports 24-bit bitmaps, but I don't wanna deal with conversion :)
	if (32 != tQrInfo.nBitCount)
	{
		hrResult = E_NOTIMPL;
		PROGRESS("Current QR bitmap has an unsupported BPP (%lu).", tQrInfo.nBitCount);
		goto lblCleanup;
	}

	hrResult = UTIL_ReadFile(pwszFilename, &pvBitmap, &cbBitmap);
	if (FAILED(hrResult))
	{
		PROGRESS("Failed reading bitmap file (0x%08lX).", hrResult);
		goto lblCleanup;
	}

	if (cbBitmap > MAXDWORD)
	{
		hrResult = HRESULT_FROM_WIN32(ERROR_FILE_TOO_LARGE);
		PROGRESS("Bitmap file is too large.");
		goto lblCleanup;
	}

	if (cbBitmap < sizeof(*ptFileHeader) + sizeof(*ptInfoHeader))
	{
		hrResult = HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);
		PROGRESS("Bitmap file is too small.");
		goto lblCleanup;
	}

	ptFileHeader = (PBITMAPFILEHEADER)pvBitmap;

	if ('MB' != ptFileHeader->bfType)
	{
		hrResult = HRESULT_FROM_WIN32(ERROR_BAD_FORMAT);
		PROGRESS("Invalid magic for bitmap file.");
		goto lblCleanup;
	}

	ptInfoHeader = (PBITMAPINFOHEADER)(ptFileHeader + 1);

	if (sizeof(*ptInfoHeader) != ptInfoHeader->biSize)
	{
		hrResult = HRESULT_FROM_WIN32(ERROR_BAD_FORMAT);
		PROGRESS("Invalid size for bitmap info header.");
		goto lblCleanup;
	}

	if (BI_RGB != ptInfoHeader->biCompression)
	{
		hrResult = HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
		PROGRESS("Unsupported bitmap compression.");
		goto lblCleanup;
	}

	if (32 != ptInfoHeader->biBitCount && 24 != ptInfoHeader->biBitCount)
	{
		hrResult = HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
		PROGRESS("Unsupported BPP value (%hu).", ptInfoHeader->biBitCount);
		goto lblCleanup;
	}

	if (ptInfoHeader->biWidth < 0)
	{
		hrResult = HRESULT_FROM_WIN32(ERROR_BAD_FORMAT);
		PROGRESS("Bitmap width is negative.");
		goto lblCleanup;
	}
	if ((ULONG)(ptInfoHeader->biWidth) != tQrInfo.nWidth)
	{
		hrResult = E_INVALIDARG;
		PROGRESS("Bitmap width does not match existing QR image.");
		goto lblCleanup;
	}

	if (llabs((LONGLONG)(ptInfoHeader->biHeight)) != (LONGLONG)(tQrInfo.nHeight))
	{
		hrResult = E_INVALIDARG;
		PROGRESS("Bitmap height does not match existing QR image.");
		goto lblCleanup;
	}

	// YOLO: not dealing with overflow. It's from the kernel, so it's trusted :)
	cbPixels = tQrInfo.nWidth * tQrInfo.nHeight * tQrInfo.nBitCount / 8;

	// TODO: Validate source file has enough data

	pvPixels = HEAPALLOC(cbPixels);
	if (NULL == pvPixels)
	{
		hrResult = E_OUTOFMEMORY;
		goto lblCleanup;
	}

	assert(32 == tQrInfo.nBitCount);	// The following code converts to 32 BPP
	if (32 == ptInfoHeader->biBitCount)
	{
		// No need to deal with bitmap line alignment

		if (ptInfoHeader->biHeight < 0)
		{
			// Top-down bitmap, just copy it.
			RtlMoveMemory(pvPixels, ptInfoHeader + 1, cbPixels);
		}
		else
		{
			ULONG	cbRow		= ptInfoHeader->biBitCount * tQrInfo.nWidth;
			PBYTE	pcSource	= (PBYTE)(ptInfoHeader + 1);
			PBYTE	pcDest		= ((PBYTE)pvPixels) + ((tQrInfo.nHeight - 1) * cbRow);
			ULONG	nRow		= 0;

			// Bottom-up bitmap. Copy in reverse.

			for (nRow = 0; nRow < tQrInfo.nHeight; ++nRow)
			{
				RtlMoveMemory(pcDest, pcSource, cbRow);
				pcDest -= cbRow;
				pcSource += cbRow;
			}
		}
	}
	else
	{
		ULONG		cbSourceRow	= (ptInfoHeader->biBitCount / 8 * tQrInfo.nWidth) + (4 - ptInfoHeader->biBitCount / 8 * tQrInfo.nWidth % 4);
		LPRGBTRIPLE	ptSourceRow	= (LPRGBTRIPLE)(ptInfoHeader + 1);
		ULONG		nRow		= 0;
		ULONG		nColumn		= 0;

		assert(24 == ptInfoHeader->biBitCount);

		if (ptInfoHeader->biHeight < 0)
		{
			LPRGBQUAD	ptDestRow	= pvPixels;

			// Top-down bitmap, copy normally.
			
			for (nRow = 0; nRow < tQrInfo.nHeight; ++nRow)
			{
				for (nColumn = 0; nColumn < tQrInfo.nWidth; ++nColumn)
				{
					ptDestRow[nColumn].rgbBlue = ptSourceRow[nColumn].rgbtBlue;
					ptDestRow[nColumn].rgbGreen = ptSourceRow[nColumn].rgbtGreen;
					ptDestRow[nColumn].rgbRed = ptSourceRow[nColumn].rgbtRed;
					ptDestRow[nColumn].rgbReserved = 0;
				}
				ptSourceRow = (LPRGBTRIPLE)((PBYTE)ptSourceRow + cbSourceRow);
				ptDestRow += tQrInfo.nWidth;
			}
		}
		else
		{
			LPRGBQUAD	ptDestRow	= ((LPRGBQUAD)pvPixels) + ((tQrInfo.nHeight - 1) * tQrInfo.nWidth);

			// Bottom-up bitmap. Copy in reverse.

			for (nRow = 0; nRow < tQrInfo.nHeight; ++nRow)
			{
				for (nColumn = 0; nColumn < tQrInfo.nWidth; ++nColumn)
				{
					ptDestRow[nColumn].rgbBlue = ptSourceRow[nColumn].rgbtBlue;
					ptDestRow[nColumn].rgbGreen = ptSourceRow[nColumn].rgbtGreen;
					ptDestRow[nColumn].rgbRed = ptSourceRow[nColumn].rgbtRed;
					ptDestRow[nColumn].rgbReserved = 0;
				}
				ptSourceRow = (LPRGBTRIPLE)((PBYTE)ptSourceRow + cbSourceRow);
				ptDestRow -= tQrInfo.nWidth;
			}
		}
	}

	// Transfer ownership:
	*ppvPixels = pvPixels;
	pvPixels = NULL;
	*pcbPixels = cbPixels;

	hrResult = S_OK;

lblCleanup:
	HEAPFREE(pvPixels);
	HEAPFREE(pvBitmap);

	return hrResult;
}

STATIC
HRESULT
main_HandleConvert(
	_In_					INT				nArguments,
	_In_reads_(nArguments)	CONST PCWSTR *	ppwszArguments
)
{
	HRESULT				hrResult			= E_FAIL;
	PCWSTR				pwszDumpPath		= NULL;
	PCWSTR				pwszOutputPath		= NULL;
	HDUMP				hDump				= NULL;
	PFRAMEBUFFER_DUMP	ptFramebufferDump	= NULL;
	DWORD				cbFramebufferDump	= 0;
	PFRAMEBUFFER_BITMAP	ptFramebufferBitmap	= NULL;
	PVGA_DUMP			ptDump				= NULL;
	DWORD				cbDump				= 0;
	PVGA_BITMAP			ptBitmap			= NULL;
	PVOID				pvToWrite			= NULL;
	DWORD				cbToWrite			= 0;
	HANDLE				hOutputFile			= INVALID_HANDLE_VALUE;
	DWORD				cbWritten			= 0;

	assert(NULL != ppwszArguments);

	switch (nArguments)
	{
	case SUBFUNCTION_CONVERT_NO_INPUT_ARGS_COUNT:
		pwszOutputPath = ppwszArguments[SUBFUNCTION_CONVERT_NO_INPUT_ARG_OUTPUT];
		PROGRESS("Converting system memory dump to '%S'.", pwszOutputPath);
		break;

	case SUBFUNCTION_CONVERT_ARGS_COUNT:
		pwszDumpPath = ppwszArguments[SUBFUNCTION_CONVERT_ARG_INPUT];
		pwszOutputPath = ppwszArguments[SUBFUNCTION_CONVERT_ARG_OUTPUT];
		PROGRESS("Converting dump '%S' to '%S'.", pwszDumpPath, pwszOutputPath);
		break;

	default:
		PROGRESS("Invalid number of arguments specified.");
		hrResult = E_INVALIDARG;
		goto lblCleanup;
	}

	hrResult = DUMPPARSE_Open(pwszDumpPath, &hDump);
	if (FAILED(hrResult))
	{
		PROGRESS("Failed opening the dump file.");
		goto lblCleanup;
	}

	hrResult = DUMPPARSE_ReadTagged(hDump,
									&g_tFramebufferDumpGuid,
									(PVOID *)&ptFramebufferDump,
									&cbFramebufferDump);
	if (SUCCEEDED(hrResult))
	{
		hrResult = main_FramebufferDumpToBitmap(ptFramebufferDump, &ptFramebufferBitmap);
		if (FAILED(hrResult))
		{
			PROGRESS("Failed converting framebuffer dump to BMP.");
			goto lblCleanup;
		}

		pvToWrite = ptFramebufferBitmap;
		cbToWrite = ptFramebufferBitmap->tFileHeader.bfSize;
	}
	else
	{
		hrResult = DUMPPARSE_ReadTagged(hDump,
										&g_tVgaDumpGuid,
										(PVOID *)&ptDump,
										&cbDump);
		if (FAILED(hrResult))
		{
			PROGRESS("Failed reading saved bugcheck screenshot. Did you save it?");
			goto lblCleanup;
		}
		if (sizeof(*ptDump) != cbDump)
		{
			PROGRESS("The stored screenshot has a weird size.");
			hrResult = HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
			goto lblCleanup;
		}

		hrResult = main_VgaDumpToBitmap(ptDump, &ptBitmap);
		if (FAILED(hrResult))
		{
			PROGRESS("Failed converting raw VGA dump to BMP.");
			goto lblCleanup;
		}

		pvToWrite = ptBitmap;
		cbToWrite = ptBitmap->tFileHeader.bfSize;
	}

	hOutputFile = CreateFileW(pwszOutputPath,
							  GENERIC_WRITE,
							  0,
							  NULL,
							  CREATE_ALWAYS,
							  FILE_ATTRIBUTE_NORMAL,
							  NULL);
	if (INVALID_HANDLE_VALUE == hOutputFile)
	{
		PROGRESS("Failed creating the output file.");
		hrResult = HRESULT_FROM_WIN32(GetLastError());
		goto lblCleanup;
	}

	if (!WriteFile(hOutputFile,
				   pvToWrite,
				   cbToWrite,
				   &cbWritten,
				   NULL))
	{
		PROGRESS("Failed writing to the output file.");
		hrResult = HRESULT_FROM_WIN32(GetLastError());
		goto lblCleanup;
	}
	if (cbToWrite != cbWritten)
	{
		PROGRESS("Not all data written to the output file. Strange...");
		hrResult = E_UNEXPECTED;
		goto lblCleanup;
	}

	hrResult = S_OK;

lblCleanup:
	CLOSE_FILE_HANDLE(hOutputFile);
	HEAPFREE(ptBitmap);
	HEAPFREE(ptDump);
	HEAPFREE(ptFramebufferBitmap);
	HEAPFREE(ptFramebufferDump);
	CLOSE(hDump, DUMPPARSE_Close);

	return hrResult;
}

STATIC
HRESULT
main_HandleLoad(
	_In_					INT				nArguments,
	_In_reads_(nArguments)	CONST PCWSTR *	ppwszArguments
)
{
	HRESULT	hrResult	= E_FAIL;

	UNREFERENCED_PARAMETER(nArguments);
	UNREFERENCED_PARAMETER(ppwszArguments);

	PROGRESS("Loading the driver...");

	hrResult = DRINKCONTROL_LoadDriver();
	if (FAILED(hrResult))
	{
		PROGRESS("Failed loading the driver.");
		goto lblCleanup;
	}

	hrResult = S_OK;

lblCleanup:
	return hrResult;
}

STATIC
HRESULT
main_HandleUnload(
	_In_					INT				nArguments,
	_In_reads_(nArguments)	CONST PCWSTR *	ppwszArguments
)
{
	HRESULT	hrResult	= E_FAIL;

	UNREFERENCED_PARAMETER(nArguments);
	UNREFERENCED_PARAMETER(ppwszArguments);

	PROGRESS("Unloading the driver...");

	hrResult = DRINKCONTROL_UnloadDriver();
	if (FAILED(hrResult))
	{
		PROGRESS("Failed unloading the driver.");
		goto lblCleanup;
	}

	hrResult = S_OK;

lblCleanup:
	return hrResult;
}

STATIC
HRESULT
main_HandleBugshot(
	_In_					INT				nArguments,
	_In_reads_(nArguments)	CONST PCWSTR *	ppwszArguments
)
{
	HRESULT		hrResult	= E_FAIL;
	RESOLUTION	tResolution	= { 0 };
	ULONGLONG	nWidth		= 0;
	ULONGLONG	nHeight		= 0;

	if (SUBFUNCTION_BUGSHOT_ARGS_COUNT != nArguments && 0 != nArguments)
	{
		PROGRESS("Invalid number of arguments specified.");
		hrResult = E_INVALIDARG;
		goto lblCleanup;
	}

	if (0 == nArguments)
	{
		PROGRESS("No resolution specified. Defaulting to 640x480.");
		tResolution.nWidth = 640;
		tResolution.nHeight = 480;
	}
	else
	{
		nWidth = wcstoull(ppwszArguments[SUBFUNCTION_BUGSHOT_ARG_WIDTH], NULL, 10);
		if (0 == nWidth || nWidth > ULONG_MAX)
		{
			PROGRESS("Invalid width specified (%ws)", ppwszArguments[SUBFUNCTION_BUGSHOT_ARG_WIDTH]);
		}

		nHeight = wcstoull(ppwszArguments[SUBFUNCTION_BUGSHOT_ARG_HEIGHT], NULL, 10);
		if (0 == nHeight || nHeight > ULONG_MAX)
		{
			PROGRESS("Invalid height specified (%ws)", ppwszArguments[SUBFUNCTION_BUGSHOT_ARG_HEIGHT]);
		}

		tResolution.nWidth = (ULONG)nWidth;
		tResolution.nHeight = (ULONG)nHeight;

		PROGRESS("Using max resolution of %lux%lu.", tResolution.nWidth, tResolution.nHeight);
	}

	PROGRESS("Registering callback to take a bugcheck snapshot.");

	hrResult = DRINKCONTROL_ControlDriver(IOCTL_DRINK_BUGSHOT,
										  &tResolution, sizeof(tResolution),
										  NULL, 0, NULL);
	if (FAILED(hrResult))
	{
		PROGRESS("Failed registering for snapshot.");
		goto lblCleanup;
	}

	hrResult = S_OK;

lblCleanup:
	return hrResult;
}

STATIC
HRESULT
main_HandleVanity(
	_In_					INT				nArguments,
	_In_reads_(nArguments)	CONST PCWSTR *	ppwszArguments
)
{
	HRESULT	hrResult		= E_FAIL;
	PCWSTR	pwszInput		= NULL;
	DWORD	cchFormatted	= 0;
	DWORD	cbFormatted		= 0;
	PSTR	pszFormatted	= NULL;

	if (SUBFUNCTION_VANITY_ARGS_COUNT != nArguments)
	{
		PROGRESS("Invalid number of arguments specified.");
		hrResult = E_INVALIDARG;
		goto lblCleanup;
	}

	pwszInput = ppwszArguments[SUBFUNCTION_VANITY_ARG_STRING];
	assert(NULL != pwszInput);

	PROGRESS("About to crash system with string: '%S'.", pwszInput);

	// Calculate the length of the formatted string.
	hrResult = IntToDWord(_scprintf(VANITY_FORMAT_STRING, pwszInput),
						  &cchFormatted);
	if (FAILED(hrResult))
	{
		goto lblCleanup;
	}

	// Add the null terminator.
	hrResult = DWordAdd(cchFormatted, 1, &cchFormatted);
	if (FAILED(hrResult))
	{
		goto lblCleanup;
	}

	// Calculate the size of the formatted string (in bytes).
	hrResult = DWordMult(cchFormatted,
						 sizeof(pszFormatted[0]),
						 &cbFormatted);
	if (FAILED(hrResult))
	{
		goto lblCleanup;
	}

	// Allocate memory for the string.
	pszFormatted = HEAPALLOC(cbFormatted);
	if (NULL == pszFormatted)
	{
		PROGRESS("Oops. Ran out of memory.");
		hrResult = E_OUTOFMEMORY;
		goto lblCleanup;
	}

	// Format it.
	hrResult = StringCbPrintfA(pszFormatted,
							   cbFormatted,
							   VANITY_FORMAT_STRING,
							   pwszInput);
	if (FAILED(hrResult))
	{
		goto lblCleanup;
	}

	// Adventure time.
	PROGRESS("Adventure time.");
	hrResult = DRINKCONTROL_ControlDriver(IOCTL_DRINK_VANITY, 
										  pszFormatted, cbFormatted, 
										  NULL, 0, NULL);
	if (FAILED(hrResult))
	{
		goto lblCleanup;
	}

	hrResult = S_OK;

lblCleanup:
	HEAPFREE(pszFormatted);

	PROGRESS("Returning 0x%lX", hrResult);
	return hrResult;
}

_Use_decl_annotations_
STATIC
HRESULT
main_HandleQr(
	INT				nArguments,
	PCWSTR CONST *	ppwszArguments
)
{
	HRESULT	hrResult	= E_FAIL;
	QR_INFO	tInfo		= { 0 };
	PVOID	pvPixels	= NULL;
	DWORD	cbPixels	= 0;

	PROGRESS("Retrieving QR bitmap information.");

	hrResult = main_GetQrInfo(&tInfo);
	if (FAILED(hrResult))
	{
		PROGRESS("main_GetQrInfo failed with code 0x%08lX.", hrResult);
		goto lblCleanup;
	}

	PROGRESS("QR bitmap is %lux%lu@%lubpp", tInfo.nWidth, tInfo.nHeight, tInfo.nBitCount);

	if (0 == nArguments)
	{
		// Nothing to do.
	}
	else if (SUBFUNCTION_QR_ARGS_COUNT == nArguments)
	{
		PROGRESS("Converting bitmap.");

		hrResult = main_ConvertBitmapForQr(ppwszArguments[SUBFUNCTION_QR_ARG_IMAGE],
										   &pvPixels,
										   &cbPixels);
		if (FAILED(hrResult))
		{
			PROGRESS("main_ConvertBitmapForQr failed with code 0x%08lX.", hrResult);
			goto lblCleanup;
		}

		PROGRESS("Setting bitmap.");

		hrResult = DRINKCONTROL_ControlDriver(IOCTL_DRINK_QR_SET,
											  pvPixels, cbPixels,
											  NULL, 0, NULL);
		if (FAILED(hrResult))
		{
			PROGRESS("Failed setting bitmap (0x%08lX).", hrResult);
			goto lblCleanup;
		}
	}
	else
	{
		PROGRESS("Invalid number of arguments specified.");
		hrResult = E_INVALIDARG;
		goto lblCleanup;
	}

	hrResult = S_OK;

lblCleanup:
	HEAPFREE(pvPixels);

	return hrResult;
}

/**
 * The application's entry-point.
 *
 * @returns INT (cast from HRESULT)
 */
INT
wmain(
	_In_					INT				nArguments,
	_In_reads_(nArguments)	CONST PCWSTR *	ppwszArguments
)
{
	HRESULT						hrResult		= E_FAIL;
	DWORD						nIndex			= 0;
	PCSUBFUNCTION_HANDLER_ENTRY	ptCurrentEntry	= NULL;
	PFN_SUBFUNCTION_HANDLER		pfnHandler		= NULL;

	assert(0 != nArguments);
	assert(NULL != ppwszArguments);

	(VOID)DEBUG_Init();

	if (CMD_ARGS_COUNT > nArguments)
	{
		main_PrintUsage();
		hrResult = E_INVALIDARG;
		goto lblCleanup;
	}

	PROGRESS("Selected subfunction: '%S'.", ppwszArguments[CMD_ARG_SUBFUNCTION]);

	for (nIndex = 0;
		 nIndex < ARRAYSIZE(g_atSubfunctionHandlers);
		 ++nIndex)
	{
		ptCurrentEntry = &(g_atSubfunctionHandlers[nIndex]);

		if (0 == _wcsicmp(ptCurrentEntry->pwszSubfunctionName,
						  ppwszArguments[CMD_ARG_SUBFUNCTION]))
		{
			pfnHandler = ptCurrentEntry->pfnHandler;
			break;
		}
	}
	if (NULL == pfnHandler)
	{
		main_PrintUsage();
		hrResult = E_INVALIDARG;
		goto lblCleanup;
	}

	hrResult = pfnHandler(nArguments - CMD_ARGS_COUNT,
						  ppwszArguments + CMD_ARGS_COUNT);
	if (FAILED(hrResult))
	{
		PROGRESS("Subfunction handler failed with code 0x%08lX.", hrResult);
	}

	// Keep last status

lblCleanup:
	DEBUG_Shutdown();

	return (INT)hrResult;
}
