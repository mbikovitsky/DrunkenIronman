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
#include "Debug.h"

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

	PROGRESS("About to load the driver.");

	hrResult = UTIL_ReadResource(GetModuleHandleW(NULL),
								 MAKEINTRESOURCEW(IDR_DRIVER),
								 L"BINARY",
								 MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL),
								 &pvDriver,
								 &cbDriver);
	if (FAILED(hrResult))
	{
		PROGRESS("Failed reading the driver from the resource section.");
		goto lblCleanup;
	}

	hrResult = UTIL_WriteToTemporaryFile(pvDriver,
										 cbDriver,
										 &pwszTempDriverPath);
	if (FAILED(hrResult))
	{
		PROGRESS("Failed writing the driver to a temporary file.");
		goto lblCleanup;
	}

	hServiceControlManager = OpenSCManagerW(NULL,
											SERVICES_ACTIVE_DATABASEW,
											SC_MANAGER_CREATE_SERVICE);
	if (NULL == hServiceControlManager)
	{
		PROGRESS("Failed opening the SCM.");
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
		PROGRESS("Failed creating a service for the driver.");
		hrResult = HRESULT_FROM_WIN32(GetLastError());
		goto lblCleanup;
	}

	if (!StartServiceW(hDriverService,
					   0,
					   NULL))
	{
		PROGRESS("Failed starting the driver service.");
		hrResult = HRESULT_FROM_WIN32(GetLastError());
		goto lblCleanup;
	}

	PROGRESS("Driver loaded.");
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
	HRESULT					hrResult				= E_FAIL;
	SC_HANDLE				hServiceControlManager	= NULL;
	SC_HANDLE				hDriverService			= NULL;
	DWORD					cbServiceConfig			= 0;
	LPQUERY_SERVICE_CONFIGW	ptServiceConfig			= NULL;
	SERVICE_STATUS			tStatus					= { 0 };

	PROGRESS("About to unload the driver.");

	hServiceControlManager = OpenSCManagerW(NULL,
											SERVICES_ACTIVE_DATABASEW,
											SC_MANAGER_CONNECT);
	if (NULL == hServiceControlManager)
	{
		hrResult = HRESULT_FROM_WIN32(GetLastError());
		PROGRESS("Failed opening the SCM.");
		goto lblCleanup;
	}

	hDriverService = OpenServiceW(hServiceControlManager,
								  L"Drink",
								  SERVICE_STOP | SERVICE_QUERY_CONFIG);
	if (NULL == hDriverService)
	{
		hrResult = HRESULT_FROM_WIN32(GetLastError());
		PROGRESS("Failed opening the driver service.");
		goto lblCleanup;
	}

	if (QueryServiceConfigW(hDriverService, NULL, 0, &cbServiceConfig))
	{
		hrResult = E_UNEXPECTED;
		PROGRESS("QueryServiceConfigW unexpectedly succeeded.");
		goto lblCleanup;
	}
	if (ERROR_INSUFFICIENT_BUFFER != GetLastError())
	{
		hrResult = E_UNEXPECTED;
		PROGRESS("QueryServiceConfigW returned unexpected error %lu.", GetLastError());
		goto lblCleanup;
	}

	ptServiceConfig = HEAPALLOC(cbServiceConfig);
	if (NULL == ptServiceConfig)
	{
		hrResult = E_OUTOFMEMORY;
		PROGRESS("Failed allocating service config buffer.");
		goto lblCleanup;
	}

	if (!QueryServiceConfigW(hDriverService, ptServiceConfig, cbServiceConfig, &cbServiceConfig))
	{
		hrResult = HRESULT_FROM_WIN32(GetLastError());
		PROGRESS("Failed querying the driver service configuration.");
		goto lblCleanup;
	}

	if (!ControlService(hDriverService,
						SERVICE_CONTROL_STOP,
						&tStatus))
	{
		hrResult = HRESULT_FROM_WIN32(GetLastError());
		PROGRESS("Failed stopping the driver service.");
		goto lblCleanup;
	}

	PROGRESS("Driver unloaded.");

	(VOID)DeleteFileW(ptServiceConfig->lpBinaryPathName);

	hrResult = S_OK;

lblCleanup:
	HEAPFREE(ptServiceConfig);
	CLOSE(hDriverService, CloseServiceHandle);
	CLOSE(hServiceControlManager, CloseServiceHandle);

	return hrResult;
}

HRESULT
DRINKCONTROL_ControlDriver(
	DWORD	eControlCode,
	PVOID	pvInputBuffer,
	DWORD	cbInputBuffer,
	PVOID	pvOutputBuffer,
	DWORD	cbOutputBuffer,
	PDWORD	pcbWritten
)
{
	HRESULT	hrResult		= E_FAIL;
	HANDLE	hDrinkDevice	= INVALID_HANDLE_VALUE;
	DWORD	cbReturned		= 0;

	PROGRESS("Sending control code 0x%08lX to driver.", eControlCode);

	hDrinkDevice = CreateFileW(L"\\\\.\\" DRINK_DEVICE_NAME,
							   GENERIC_READ | GENERIC_WRITE,
							   0,
							   NULL,
							   OPEN_EXISTING,
							   FILE_ATTRIBUTE_NORMAL,
							   NULL);
	if (INVALID_HANDLE_VALUE == hDrinkDevice)
	{
		PROGRESS("Failed opening the driver's control device. Did you load the driver?");
		hrResult = HRESULT_FROM_WIN32(GetLastError());
		goto lblCleanup;
	}

	if (!DeviceIoControl(hDrinkDevice,
						 eControlCode,
						 pvInputBuffer, cbInputBuffer,
						 pvOutputBuffer, cbOutputBuffer,
						 &cbReturned,
						 NULL))
	{
		PROGRESS("Failed sending the control code.");
		hrResult = HRESULT_FROM_WIN32(GetLastError());
		goto lblCleanup;
	}

	PROGRESS("Control code sent.");

	if (NULL != pcbWritten)
	{
		*pcbWritten = cbReturned;
	}

	hrResult = S_OK;

lblCleanup:
	CLOSE_FILE_HANDLE(hDrinkDevice);

	return hrResult;
}
