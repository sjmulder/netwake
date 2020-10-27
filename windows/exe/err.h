void InitErr(HINSTANCE instance, const char *appTitle, const HWND *wndPtr);
void Info(int msgRes);
void Warn(int msgRes);
void WarnWin(int prefixRes);
void __declspec(noreturn) Err(int msgRes);
void __declspec(noreturn) ErrWin(int msgRes);

