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


/** Functions ***********************************************************/

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
