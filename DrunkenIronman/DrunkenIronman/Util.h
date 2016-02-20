#pragma once

#include <windows.h>
#include <tchar.h>


HRESULT
UTIL_IsWow64Process(
	_Out_	PBOOL	pbWow64Process
);
