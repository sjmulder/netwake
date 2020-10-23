#include <windows.h>
#include "compat.h"

#undef GetDpiForSystem
#undef InitCommonControls

/* user32 */
UINT (*GetDpiForSystem_Ptr)(void);

/* comctl32 */
void (*InitCommonControls_Ptr)(void);

int
CompatInit(void)
{
	HMODULE lib;

	if ((lib = LoadLibrary("user32.dll"))) {
		GetDpiForSystem_Ptr = (void *)GetProcAddress(lib,
		    "GetDpiForSystem");
	}

	if ((lib = LoadLibrary("comctl32.dll"))) {
		InitCommonControls_Ptr = (void *)GetProcAddress(lib,
		    "InitCommonControls");
	}

	return 0;
}
