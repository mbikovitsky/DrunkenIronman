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


/** Typedefs ************************************************************/

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
	ULONG			anPalette[VGA_DAC_PALETTE_ENTRIES];

	// Contents of all the VGA's planes, sequentially.
	VGA_PLANE_DUMP	atPlanes[VGA_PLANES];
} VGA_DUMP, *PVGA_DUMP;
typedef CONST VGA_DUMP *PCVGA_DUMP;
