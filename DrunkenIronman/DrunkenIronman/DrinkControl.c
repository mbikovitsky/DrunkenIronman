/**
 * @file DrinkControl.c
 * @author biko
 * @date 2016-08-07
 *
 * DrinkControl module implementation.
 */

/** Headers *************************************************************/
#include <Windows.h>

#include <Common.h>
#include <Drink.h>

#include "Resource.h"
#include "Util.h"

#include "DrinkControl.h"


/** Functions ***********************************************************/

HRESULT
DRINKCONTROL_LoadDriver(VOID)
{
	HRESULT		hrResult				= E_FAIL;
	PVOID		pvDriver				= NULL;
	DWORD		cbDriver				= 0;
	PWSTR		pwszTempDriverPath		= NULL;
	SC_HANDLE	hServiceControlManager	= NULL;
	SC_HANDLE	hDriverService			= NULL;

	hrResult = UTIL_ReadResource(GetModuleHandleW(NULL),
								 MAKEINTRESOURCEW(IDR_DRIVER),
								 L"BINARY",
								 MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL),
								 &pvDriver,
								 &cbDriver);
	if (FAILED(hrResult))
	{
		goto lblCleanup;
	}

	hrResult = UTIL_WriteToTemporaryFile(pvDriver,
										 cbDriver,
										 &pwszTempDriverPath);
	if (FAILED(hrResult))
	{
		goto lblCleanup;
	}

	hServiceControlManager = OpenSCManagerW(NULL,
											SERVICES_ACTIVE_DATABASEW,
											SC_MANAGER_CREATE_SERVICE);
	if (NULL == hServiceControlManager)
	{
		hrResult = HRESULT_FROM_WIN32(GetLastError());
		goto lblCleanup;
	}

	hDriverService = CreateServiceW(hServiceControlManager,
									L"Drink",
									NULL,
									SERVICE_START | DELETE,
									SERVICE_KERNEL_DRIVER,
									SERVICE_DEMAND_START,
									SERVICE_ERROR_IGNORE,
									pwszTempDriverPath,
									NULL,
									NULL,
									NULL,
									NULL,
									NULL);
	if (NULL == hDriverService)
	{
		hrResult = HRESULT_FROM_WIN32(GetLastError());
		goto lblCleanup;
	}

	if (!StartServiceW(hDriverService,
					   0,
					   NULL))
	{
		hrResult = HRESULT_FROM_WIN32(GetLastError());
		goto lblCleanup;
	}

	hrResult = S_OK;

lblCleanup:
	if (NULL != hDriverService)
	{
		(VOID)DeleteService(hDriverService);
	}
	CLOSE(hDriverService, CloseServiceHandle);
	CLOSE(hServiceControlManager, CloseServiceHandle);
	if (NULL != pwszTempDriverPath)
	{
		(VOID)DeleteFileW(pwszTempDriverPath);
	}
	HEAPFREE(pwszTempDriverPath);
	HEAPFREE(pvDriver);

	return hrResult;
}

HRESULT
DRINKCONTROL_UnloadDriver(VOID)
{
	HRESULT			hrResult				= E_FAIL;
	SC_HANDLE		hServiceControlManager	= NULL;
	SC_HANDLE		hDriverService			= NULL;
	SERVICE_STATUS	tStatus					= { 0 };

	hServiceControlManager = OpenSCManagerW(NULL,
											SERVICES_ACTIVE_DATABASEW,
											SC_MANAGER_CONNECT);
	if (NULL == hServiceControlManager)
	{
		hrResult = HRESULT_FROM_WIN32(GetLastError());
		goto lblCleanup;
	}

	hDriverService = OpenServiceW(hServiceControlManager,
								  L"Drink",
								  SERVICE_STOP);
	if (NULL == hDriverService)
	{
		hrResult = HRESULT_FROM_WIN32(GetLastError());
		goto lblCleanup;
	}

	if (!ControlService(hDriverService,
						SERVICE_CONTROL_STOP,
						&tStatus))
	{
		hrResult = HRESULT_FROM_WIN32(GetLastError());
		goto lblCleanup;
	}

	hrResult = S_OK;

lblCleanup:
	CLOSE(hDriverService, CloseServiceHandle);
	CLOSE(hServiceControlManager, CloseServiceHandle);

	return hrResult;
}

HRESULT
DRINKCONTROL_ControlDriver(
	_In_								DWORD	eControlCode,
	_In_reads_bytes_opt_(cbInputBuffer)	PVOID	pvInputBuffer,
	_In_								DWORD	cbInputBuffer
)
{
	HRESULT	hrResult		= E_FAIL;
	HANDLE	hDrinkDevice	= INVALID_HANDLE_VALUE;
	DWORD	cbReturned		= 0;

	hDrinkDevice = CreateFileW(L"\\\\.\\" DRINK_DEVICE_NAME,
							   GENERIC_READ | GENERIC_WRITE,
							   0,
							   NULL,
							   OPEN_EXISTING,
							   FILE_ATTRIBUTE_NORMAL,
							   NULL);
	if (INVALID_HANDLE_VALUE == hDrinkDevice)
	{
		hrResult = HRESULT_FROM_WIN32(GetLastError());
		goto lblCleanup;
	}

	if (!DeviceIoControl(hDrinkDevice,
						 eControlCode,
						 pvInputBuffer, cbInputBuffer,
						 NULL, 0,
						 &cbReturned,
						 NULL))
	{
		hrResult = HRESULT_FROM_WIN32(GetLastError());
		goto lblCleanup;
	}

	hrResult = S_OK;

lblCleanup:
	CLOSE_FILE_HANDLE(hDrinkDevice);

	return hrResult;
}
