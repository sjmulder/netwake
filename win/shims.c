#include <windows.h>
#include "shims.h"

#undef GetDpiForSystem
#undef InitCommonControls

/* user32 */
static UINT (*sGetDpiForSystem)(void);
/* comctl32 */
static void (*sInitCommonControls)(void);

int
ShimsInit(void)
{
	HMODULE lib;

	if ((lib = LoadLibrary("user32.dll"))) {
		sGetDpiForSystem = (void *)GetProcAddress(lib,
		    "GetDpiForSystem");
	}

	if ((lib = LoadLibrary("comctl32.dll"))) {
		sInitCommonControls = (void *)GetProcAddress(lib,
		    "InitCommonControls");
	}

	return 0;
}

UINT
GetDpiForSystem_Shim(void)
{
	if (sGetDpiForSystem)
		return sGetDpiForSystem();
	else
		return 96;
}

void
InitCommonControls_Shim(void)
{
	if (sInitCommonControls)
		sInitCommonControls();
}
