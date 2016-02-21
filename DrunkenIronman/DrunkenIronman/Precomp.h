#pragma once

#pragma warning(push)
#pragma warning(disable: 4201)	// nonstandard extension used: nameless struct/union

#define WIN32_NO_STATUS
#include <windows.h>
#include <tchar.h>
#include <winternl.h>
#undef WIN32_NO_STATUS

#include <ntstatus.h>

#pragma warning(pop)
