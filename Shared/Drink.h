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
 * {80AEEC5F-DE92-435D-9A05-D23ECAD9272E}
 * GUID for tagging the saved framebuffer dump in the dump file.
 */
EXTERN_C CONST GUID DECLSPEC_SELECTANY g_tFramebufferDumpGuid = 
{ 0x80aeec5f, 0xde92, 0x435d, { 0x9a, 0x5, 0xd2, 0x3e, 0xca, 0xd9, 0x27, 0x2e } };

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
 * Input:	None.
 * Output:	None.
 */
#define IOCTL_DRINK_BUGSHOT \
	(CTL_CODE(DRINK_DEVICE_TYPE, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS))

/**
 * IOCTL for setting-up a vanity bugcheck.
 *
 * Input:	ANSI string with the custom bugcheck string.
 * Output:	None.
 */
#define IOCTL_DRINK_VANITY \
	(CTL_CODE(DRINK_DEVICE_TYPE, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS))

/**
 * @brief Returns information about the current bugcheck QR image.
 *
 * Input:	None.
 * Output:	QR_INFO structure.
 */
#define IOCTL_DRINK_QR_INFO \
	(CTL_CODE(DRINK_DEVICE_TYPE, 0x802, METHOD_BUFFERED, FILE_ANY_ACCESS))

/**
 * @brief Sets the QR bitmap.
 *
 * Input:	Pixel data.
 * Output:	None.
 */
#define IOCTL_DRINK_QR_SET \
	(CTL_CODE(DRINK_DEVICE_TYPE, 0x803, METHOD_BUFFERED, FILE_ANY_ACCESS))


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

/**
 * @brief Holds information about the current bugcheck QR image.
*/
typedef struct _QR_INFO
{
	ULONG	nWidth;
	ULONG	nHeight;
	ULONG	nBitCount;
} QR_INFO, *PQR_INFO;
typedef QR_INFO CONST *PCQR_INFO;

typedef struct _FRAMEBUFFER_DUMP
{
	ULONG	nWidth;
	ULONG	nHeight;
	BOOLEAN	bValid;
	UCHAR	acPixels[ANYSIZE_ARRAY];
} FRAMEBUFFER_DUMP, *PFRAMEBUFFER_DUMP;
typedef FRAMEBUFFER_DUMP CONST *PCFRAMEBUFFER_DUMP;
