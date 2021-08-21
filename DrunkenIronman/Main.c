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
};


/** Functions ***********************************************************/

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
				   L"Copyright (c) 2016 Michael Bikovitsky\n\n%s <subfunction> <subfunction args>\n\n",
				   pwszExecutableName);

	(VOID)fwprintf(stderr,
				   L"  convert [input] output\n    Extracts a screenshot from a memory dump.\n");

	(VOID)fwprintf(stderr,
				   L"  load\n    Loads the driver.\n");

	(VOID)fwprintf(stderr,
				   L"  unload\n    Unloads the driver.\n");

	(VOID)fwprintf(stderr,
				   L"  bugshot\n    Instructs the driver to capture a screenshot\n    of the next BSoD.\n");

	(VOID)fwprintf(stderr,
				   L"  vanity string\n    Crashes the system and displays the specified string\n    on the BSoD.\n");

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
main_HandleConvert(
	_In_					INT				nArguments,
	_In_reads_(nArguments)	CONST PCWSTR *	ppwszArguments
)
{
	HRESULT		hrResult		= E_FAIL;
	PCWSTR		pwszDumpPath	= NULL;
	PCWSTR		pwszOutputPath	= NULL;
	HDUMP		hDump			= NULL;
	PVGA_DUMP	ptDump			= NULL;
	DWORD		cbDump			= 0;
	PVGA_BITMAP	ptBitmap		= NULL;
	HANDLE		hOutputFile		= INVALID_HANDLE_VALUE;
	DWORD		cbWritten		= 0;

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
									&g_tVgaDumpGuid,
									&ptDump,
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
				   ptBitmap,
				   sizeof(*ptBitmap),
				   &cbWritten,
				   NULL))
	{
		PROGRESS("Failed writing to the output file.");
		hrResult = HRESULT_FROM_WIN32(GetLastError());
		goto lblCleanup;
	}
	if (sizeof(*ptBitmap) != cbWritten)
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
	HRESULT	hrResult	= E_FAIL;

	UNREFERENCED_PARAMETER(nArguments);
	UNREFERENCED_PARAMETER(ppwszArguments);

	PROGRESS("Registering callback to take a bugcheck snapshot.");

	hrResult = DRINKCONTROL_ControlDriver(IOCTL_DRINK_BUGSHOT,
										  NULL, 0,
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
