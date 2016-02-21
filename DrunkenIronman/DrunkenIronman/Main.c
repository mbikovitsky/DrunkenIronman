#include "Precomp.h"
#include "Util.h"


INT
_tmain(VOID)
{
	HRESULT	hrResult		= E_FAIL;
	BOOL	bWow64Process	= FALSE;

	hrResult = UTIL_IsWow64Process(&bWow64Process);
	if (FAILED(hrResult))
	{
		goto lblCleanup;
	}

	if (bWow64Process)
	{
		hrResult = HRESULT_FROM_WIN32(ERROR_WOW_ASSERTION);
		goto lblCleanup;
	}

	hrResult = S_OK;

lblCleanup:
	return (INT)hrResult;
}
