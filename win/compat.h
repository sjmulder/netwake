#define WM_THEMECHANGED	0x031A
#define WM_DPICHANGED	0x02E0

/* user32 */
#define GetDpiForSystem GetDpiForSystem_Ptr

/* comctl32 */
#define InitCommonControls InitCommonControls_Ptr

int CompatInit(void);

/* user32 */
extern UINT (*GetDpiForSystem_Ptr)(void);

/* comctl32 */
extern void (*InitCommonControls_Ptr)(void);

