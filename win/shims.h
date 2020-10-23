/* user32 */
#define GetDpiForSystem(x) GetDpiForSystem_Shim(x)
/* comctl32 */
#define InitCommonControls() InitCommonControls_Shim()

int ShimsInit(void);

/* user32 */
UINT GetDpiForSystem_Shim(void);
/* comctl32 */
void InitCommonControls_Shim(void);
