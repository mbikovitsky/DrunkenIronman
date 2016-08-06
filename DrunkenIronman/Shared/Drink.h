/**
 * @file Drink.h
 * @author biko
 * @date 2016-07-29
 *
 * Drink driver interface.
 */
#pragma once

/** Constants ***********************************************************/

/**
 * Width of the bugcheck screen, in pixels.
 */
#define SCREEN_WIDTH_PIXELS (640)

/**
 * Height of the bugcheck screen, in pixels.
 */
#define SCREEN_HEIGHT_PIXELS (480)

/**
 * Number of pixels stored in a single byte.
 *        Must be equal to the number of bits in a byte.
 */
#define PIXELS_IN_BYTE (8)
C_ASSERT(PIXELS_IN_BYTE == 8);

/**
 * Number of VGA planes.
 */
#define VGA_PLANES (4)

/**
 * Number of entries in the VGA's DAC palette.
 */
#define VGA_DAC_PALETTE_ENTRIES (256)

/**
 * {ab490092-9446-4088-901b-b6a801cd6c75}
 * GUID for tagging the saved VGA dump in the dump file.
 */
EXTERN_C CONST GUID DECLSPEC_SELECTANY g_tVgaDumpGuid =
{ 0xab490092, 0x9446, 0x4088, { 0x90, 0x1b, 0xb6, 0xa8, 0x01, 0xcd, 0x6c, 0x75 } };

/**
 * Name of the Drink control device.
 */
#define DRINK_DEVICE_NAME L"DrinkMe"

/**
 * The Drink control device's type.
 */
#define DRINK_DEVICE_TYPE (FILE_DEVICE_UNKNOWN)

/**
 * IOCTL for setting-up a bugcheck screenshot.
 *
 * Input:	None
 * Output:	None
 */
#define IOCTL_DRINK_BUGSHOT \
	(CTL_CODE(DRINK_DEVICE_TYPE, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS))


/** Typedefs ************************************************************/

/**
 * Contains information about a single DAC palette entry.
 */
typedef struct _PALETTE_ENTRY
{
	UCHAR	nRed;
	UCHAR	nGreen;
	UCHAR	nBlue;
} PALETTE_ENTRY, *PPALETTE_ENTRY;
typedef CONST PALETTE_ENTRY *PCPALETTE_ENTRY;

/**
 * Structure of a single plane of VGA video memory.
 */
typedef UCHAR VGA_PLANE_DUMP[(SCREEN_WIDTH_PIXELS * SCREEN_HEIGHT_PIXELS) / PIXELS_IN_BYTE];

/**
 * Structure of a VGA video memory dump.
 */
typedef struct _VGA_DUMP
{
	// The VGA's DAC palette entries.
	PALETTE_ENTRY	atPaletteEntries[VGA_DAC_PALETTE_ENTRIES];

	// Contents of all the VGA's planes, sequentially.
	VGA_PLANE_DUMP	atPlanes[VGA_PLANES];
} VGA_DUMP, *PVGA_DUMP;
typedef CONST VGA_DUMP *PCVGA_DUMP;
