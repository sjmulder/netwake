#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <commctrl.h>
#include "../util.h"
#include "../wol.h"

#define CPAT_WM_THEMECHANGED	0x031A
#define CPAT_WM_DPICHANGED	0x02E0

static UINT (*sGetDpiForSystem)(void);

static const char sClassName[] = "Netwake";
static const DWORD sWndStyle = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU |
    WS_MINIMIZEBOX;
static const RECT sWndSize = {0, 0, 456, 272};

static const char sInfoText[] =
"Turn on a computer by sending it a Wake-on-LAN packet.\r\n\r\n"
"The computer must be on the same network and have WOL enabled in its "
"firmware.\r\n\r\n"
"By Sijmen J. Mulder\r\n"
"Contact: ik@sjmulder.nl";

static HINSTANCE sInstance;
static HFONT sFont;
static WORD sBaseDpi = 96, sDpi = 96;

static HWND sWnd;
static HWND sInfoFrame, sInfoLabel;
static HWND sMacLabel, sMacField;
static HWND sWakeBtn;
static HWND sNameLabel, sNameField;
static HWND sFavLabel, sFavList;
static HWND sQuitBtn, sDelBtn, sSaveBtn;

static const struct {
	HWND *wnd;
	int x, y, w, h;
	const char *className;
	const char *text;
	DWORD style, exStyle;
} sCtrlDefs[] = {
	{&sInfoFrame, -8, -16, 168, 298, "BUTTON", "About",
	    WS_GROUP | BS_GROUPBOX, 0},
	{&sInfoLabel, 8, 8, 140, 256, "STATIC", sInfoText, 0, 0},
	{&sMacLabel, 168, 10, 96, 17, "STATIC", "MAC &Address:", 0, 0},
	{&sMacField, 264, 8, 184, 21, "EDIT", NULL, WS_TABSTOP,
	    WS_EX_CLIENTEDGE},
	{&sWakeBtn, 373, 33, 75, 23, "BUTTON", "&Wake", WS_TABSTOP, 0},
	{&sNameLabel, 168, 74, 96, 17, "STATIC", "&Name:", 0, 0},
	{&sNameField, 264, 72, 184, 21, "EDIT", NULL, WS_TABSTOP ,
	    WS_EX_CLIENTEDGE},
	{&sFavLabel, 168, 98, 96, 17, "STATIC", "&Favourites:", 0, 0},
	{&sFavList, 264, 96, 184, 140, "LISTBOX", NULL,
	    WS_TABSTOP | LBS_NOINTEGRALHEIGHT, WS_EX_CLIENTEDGE},
	{&sQuitBtn, 8, 240, 75, 23, "BUTTON", "&Quit", WS_TABSTOP, 0},
	{&sDelBtn, 293, 240, 75, 23, "BUTTON", "&Delete", WS_TABSTOP, 0},
	{&sSaveBtn, 373, 240, 75, 23, "BUTTON", "&Save", WS_TABSTOP, 0}
};

void __declspec(noreturn)
err(int code, const char *msg)
{
	MessageBox(sWnd, msg, "Netwake", MB_OK | MB_ICONEXCLAMATION);
	ExitProcess(code);
}

static void
loadFunctions(void)
{
	HMODULE user32;

	if (!(user32 = LoadLibrary("user32.dll")))
		err(1, "LoadLibrary(user32.dl) failed");

	sGetDpiForSystem = (void *)GetProcAddress(user32, "GetDpiForSystem");
}

static void
setupWinsock(void)
{
	WSADATA data;

	if (WSAStartup(MAKEWORD(2, 0), &data))
		err(1, "WSAStartup failed");
}

static void
applySystemFont(void)
{
	NONCLIENTMETRICS metrics;
	LOGFONT *lfont;
	int i;

	if (sFont)
		DeleteObject(sFont);

	ZeroMemory(&metrics, sizeof(metrics));
	metrics.cbSize = sizeof(metrics);

	if (!SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(metrics),
	    &metrics, 0))
		err(1, "SystemParametersInfo failed");

	lfont = &metrics.lfMessageFont;
	lfont->lfHeight = MulDiv(lfont->lfHeight, sDpi, sBaseDpi);
	lfont->lfWidth = MulDiv(lfont->lfWidth, sDpi, sBaseDpi);

	if (!(sFont = CreateFontIndirect(lfont)))
		err(1, "CreateFontIndirect failed");

	for (i=0; i < (int)LEN(sCtrlDefs); i++)
		SendMessage(*sCtrlDefs[i].wnd, WM_SETFONT, (WPARAM)sFont,
		    MAKELPARAM(FALSE, 0));
}

static void
createControls(void)
{
	int i;

	for (i=0; i < (int)LEN(sCtrlDefs); i++) {
		*sCtrlDefs[i].wnd = CreateWindowEx(
		    sCtrlDefs[i].exStyle,
		    sCtrlDefs[i].className, sCtrlDefs[i].text,
		    sCtrlDefs[i].style | WS_VISIBLE | WS_CHILD,
		    MulDiv(sCtrlDefs[i].x, sDpi, 96),
		    MulDiv(sCtrlDefs[i].y, sDpi, 96),
		    MulDiv(sCtrlDefs[i].w, sDpi, 96),
		    MulDiv(sCtrlDefs[i].h, sDpi, 96),
		    sWnd, NULL, sInstance, NULL);
		if (!*sCtrlDefs[i].wnd)
			err(1, "Failed to create window");
	} 
}

static void
relayout(void)
{
	int i;

	for (i=0; i < (int)LEN(sCtrlDefs); i++)
		MoveWindow(*sCtrlDefs[i].wnd,
		    MulDiv(sCtrlDefs[i].x, sDpi, 96),
		    MulDiv(sCtrlDefs[i].y, sDpi, 96),
		    MulDiv(sCtrlDefs[i].w, sDpi, 96),
		    MulDiv(sCtrlDefs[i].h, sDpi, 96), TRUE);
}

static void
sendWol(void)
{
	char buf[256];
	tMacAddr mac;
	HKEY key;

	GetWindowText(sMacField, buf, sizeof(buf));

	if (ParseMacAddr(buf, &mac) == -1) {
		MessageBox(sWnd, "Invalid MAC address.\r\n\r\n"
		    "The address should consist of 8 pairs of hexadecimal "
		    "digits (0-A), optionally separated by colons.\r\n\r\n"
		    "Example: d2:32:e4:87:85:24", "Netwake",
		    MB_ICONEXCLAMATION | MB_OK);
		return;
	}

	FormatMacAddr(&mac, buf);
	SetWindowText(sMacField, buf);
	SendWolPacket(&mac);

	MessageBox(sWnd, "Wake-on-LAN packet sent!", "Netwake",
	    MB_ICONINFORMATION | MB_OK);
}

static LRESULT CALLBACK
wndProc(HWND wnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	RECT *rectp;

	switch (msg) {
	case WM_SYSCOLORCHANGE:
	case CPAT_WM_THEMECHANGED:
		applySystemFont();
		return 0;
	case CPAT_WM_DPICHANGED:
		sDpi = HIWORD(wparam);
		rectp = (RECT *)lparam;

		relayout();
		applySystemFont();
		MoveWindow(wnd, rectp->left, rectp->top,
		    rectp->right - rectp->left,
		    rectp->bottom - rectp->top, TRUE);
		return 0;
	case WM_COMMAND:
		if ((HWND)lparam == sQuitBtn)
			DestroyWindow(sWnd);
		else if ((HWND)lparam == sWakeBtn)
			sendWol();
		else
			break;
		return 0;
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}

	return DefWindowProc(wnd, msg, wparam, lparam);
}

int WINAPI
WinMain(HINSTANCE instance, HINSTANCE prev, LPSTR cmd, int showCmd)
{
	WNDCLASS wc;
	RECT rect;
	MSG msg;
	BOOL ret;

	(void)prev;
	(void)cmd;
	
	sInstance = instance;

	loadFunctions();
	setupWinsock();
	InitCommonControls();

	ZeroMemory(&wc, sizeof(wc));
	wc.hInstance = instance;
	wc.lpszClassName = sClassName;
	wc.lpfnWndProc = wndProc;
	wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE+1);

	if (!RegisterClass(&wc))
		err(1, "RegisterClass failed");

	if (sGetDpiForSystem)
		sBaseDpi = sDpi = sGetDpiForSystem();

	rect = sWndSize;
	rect.right  = MulDiv(rect.right, sDpi, 96);
	rect.bottom = MulDiv(rect.bottom, sDpi, 96);

	if (!AdjustWindowRect(&rect, sWndStyle, FALSE))
		err(1, "AdjustWindowRect failed");

	sWnd = CreateWindow(sClassName, "Netwake", sWndStyle,
	    CW_USEDEFAULT, CW_USEDEFAULT,
	    rect.right - rect.left, rect.bottom - rect.top,
	    NULL, NULL, instance, NULL);
	if (!sWnd)
		err(1, "CreateWindow failed");

	createControls();
	applySystemFont();
	SetFocus(sMacField);
	ShowWindow(sWnd, showCmd);

	while ((ret = GetMessage(&msg, NULL, 0, 0)) > 0) {
		if (IsDialogMessage(sWnd, &msg))
			continue;
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	DeleteObject(sFont);
	WSACleanup();

	if (ret == -1)
		return ret;

	return msg.wParam;
}
