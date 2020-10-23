#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <commctrl.h>
#include "../util.h"
#include "../wol.h"
#include "resource.h"

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

static const char sBadMacText[] =
"Invalid MAC address.\r\n\r\n"
"The address should consist of 8 pairs of hexadecimal digits (0-A), "
"optionally separated by colons.\r\n\r\n"
"Example: d2:32:e4:87:85:24";

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
	int x, y, w, h; /* in 1/8ths of font height, usually a pixel */
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
	    WS_TABSTOP | WS_VSCROLL |  LBS_SORT | LBS_NOTIFY |
	    LBS_WANTKEYBOARDINPUT | LBS_NOINTEGRALHEIGHT,
	    WS_EX_CLIENTEDGE},
	{&sQuitBtn, 8, 240, 75, 23, "BUTTON", "&Quit", WS_TABSTOP, 0},
	{&sDelBtn, 293, 240, 75, 23, "BUTTON", "&Delete",
	    WS_TABSTOP | WS_DISABLED, 0},
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
createControls(void)
{
	BOOL bret;
	NONCLIENTMETRICS metrics;
	LOGFONT *lfont;
	int i;

	ZeroMemory(&metrics, sizeof(metrics));
	metrics.cbSize = sizeof(metrics);

	bret = SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(metrics),
	    &metrics, 0);
	if (!bret)
		err(1, "SystemParametersInfo() failed.");

	/* no need to scale here, Dpi == BaseDpi at initial setup */
	lfont = &metrics.lfMessageFont;

	if (!(sFont = CreateFontIndirect(&metrics.lfMessageFont)))
		err(1, "CreateFontIndirect() failed.");

	for (i=0; i < (int)LEN(sCtrlDefs); i++) {
		*sCtrlDefs[i].wnd = CreateWindowEx(
		    sCtrlDefs[i].exStyle,
		    sCtrlDefs[i].className, sCtrlDefs[i].text,
		    sCtrlDefs[i].style | WS_VISIBLE | WS_CHILD,
		    MulDiv(sCtrlDefs[i].x, -lfont->lfHeight, 11),
		    MulDiv(sCtrlDefs[i].y, -lfont->lfHeight, 11),
		    MulDiv(sCtrlDefs[i].w, -lfont->lfHeight, 11),
		    MulDiv(sCtrlDefs[i].h, -lfont->lfHeight, 11),
		    sWnd, NULL, sInstance, NULL);

		if (!*sCtrlDefs[i].wnd)
			err(1, "CreateWindowEx() failed.");

		SendMessage(*sCtrlDefs[i].wnd, WM_SETFONT, (WPARAM)sFont,
		    MAKELPARAM(FALSE, 0));
	} 
}

static void
relayoutControls(void)
{
	BOOL bret;
	NONCLIENTMETRICS metrics;
	LOGFONT *lfont;
	int i;

	if (sFont)
		DeleteObject(sFont);

	ZeroMemory(&metrics, sizeof(metrics));
	metrics.cbSize = sizeof(metrics);

	bret = SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(metrics),
	    &metrics, 0);
	if (!bret)
		err(1, "SystemParametersInfo() failed.");

	lfont = &metrics.lfMessageFont;
	lfont->lfHeight = MulDiv(lfont->lfHeight, sDpi, sBaseDpi);
	lfont->lfWidth = MulDiv(lfont->lfWidth, sDpi, sBaseDpi);

	if (sFont)
		DeleteObject(sFont);
	if (!(sFont = CreateFontIndirect(lfont)))
		err(1, "CreateFontIndirect() failed.");

	for (i=0; i < (int)LEN(sCtrlDefs); i++) {
		SendMessage(*sCtrlDefs[i].wnd, WM_SETFONT, (WPARAM)sFont,
		    MAKELPARAM(FALSE, 0));

		MoveWindow(*sCtrlDefs[i].wnd,
		    MulDiv(sCtrlDefs[i].x, -lfont->lfHeight, 11),
		    MulDiv(sCtrlDefs[i].y, -lfont->lfHeight, 11),
		    MulDiv(sCtrlDefs[i].w, -lfont->lfHeight, 11),
		    MulDiv(sCtrlDefs[i].h, -lfont->lfHeight, 11), TRUE);
	}
}

static void
loadPrefs(void)
{
	HKEY key;
	char macStr[256], name[256];
	DWORD sz, type, i;
	LONG lret;

	lret = RegOpenKeyEx(HKEY_CURRENT_USER, "Software\\Netwake", 0,
	    KEY_READ, &key);
	if (lret != ERROR_SUCCESS)
		return;

	sz = (DWORD)sizeof(macStr);
	lret = RegQueryValueEx(key, "LastAddress", NULL, &type, (BYTE *)macStr,
	    &sz);
	if (lret == ERROR_SUCCESS && type == REG_SZ) {
		SetWindowText(sMacField, macStr);
		SendMessage(sMacField, EM_SETSEL, (WPARAM)0, (LPARAM)-1);
	}

	RegCloseKey(key);

	lret = RegOpenKeyEx(HKEY_CURRENT_USER, "Software\\Netwake\\Favourites", 0,
	    KEY_READ, &key);
	if (lret != ERROR_SUCCESS)
		return;

	for (i=0; ; i++) {
		sz = (DWORD)sizeof(name);
		lret = RegEnumValue(key, i, name, &sz, NULL, NULL, NULL, NULL);
		if (lret == ERROR_SUCCESS)
			SendMessage(sFavList, LB_ADDSTRING, 0, (LPARAM)name);
		else if (lret == ERROR_NO_MORE_ITEMS)
			break;
		else
			err(1, "RegEnumValue() failed");
	}

	RegCloseKey(key);
}

static void
saveFav(void)
{
	char macStr[256], name[256];
	tMacAddr mac;
	HKEY key;
	LSTATUS ls;

	GetWindowText(sMacField, macStr, sizeof(macStr));
	GetWindowText(sNameField, name, sizeof(name));

	if (!strlen(macStr) || !strlen(name)) {
		MessageBox(sWnd, "Enter a MAC and a name first to save it.",
		    "Netwake", MB_ICONEXCLAMATION | MB_OK);
		return;
	}

	if (ParseMacAddr(macStr, &mac) == -1) {
		MessageBox(sWnd, sBadMacText, "Netwake",
		    MB_ICONEXCLAMATION | MB_OK);
		return;
	}

	FormatMacAddr(&mac, macStr);
	SetWindowText(sMacField, macStr);

	lret = RegCreateKeyExA(HKEY_CURRENT_USER,
	    "Software\\Netwake\\Favourites", 0, NULL, REG_OPTION_NON_VOLATILE,
	    KEY_ALL_ACCESS, NULL, &key, NULL);
	if (lret != ERROR_SUCCESS)
		err(1, "RegCreateKeyExA() failed");

	lret = RegSetValueEx(key, name, 0, REG_SZ, (BYTE *)macStr,
	    strlen(macStr)+1);
	if (lret != ERROR_SUCCESS)
		err(1, "RegSetValueEx() failed");

	RegCloseKey(key);

	if (SendMessage(sFavList, LB_FINDSTRING, 0, (LPARAM)name) == LB_ERR)
		SendMessage(sFavList, LB_ADDSTRING, 0, (LPARAM)name);
}

static int
loadFav(void)
{
	LRESULT idx, len;
	LONG lret;
	char name[256], macStr[256];
	HKEY key;
	DWORD sz, type;

	if ((idx = SendMessage(sFavList, LB_GETCURSEL, 0, 0)) == LB_ERR) {
		EnableWindow(sDelBtn, FALSE);
		return -1;
	}

	len = SendMessage(sFavList, LB_GETTEXTLEN, idx, 0);
	if (!len || len >= (LRESULT)sizeof(name))
		err(1, "Invalid name.");
	if (SendMessage(sFavList, LB_GETTEXT, idx, (LPARAM)name) == LB_ERR)
		err(1, "LB_GETTEXT failed.");
	name[len] = '\0';

	lret = RegOpenKeyEx(HKEY_CURRENT_USER, "Software\\Netwake\\Favourites", 0,
	    KEY_READ, &key);
	if (lret != ERROR_SUCCESS)
		return -1;

	sz = (DWORD)sizeof(macStr);
	lret = RegQueryValueEx(key, name, NULL, &type, (BYTE *)macStr, &sz);
	if (lret != ERROR_SUCCESS)
		err(1, "RegGetValue() failed.");
	if (type != REG_SZ)
		err(1, "Registry value is of the wrong type.");

	RegCloseKey(key);

	SetWindowText(sMacField, macStr);
	SetWindowText(sNameField, name);
	EnableWindow(sDelBtn, TRUE);

	return 0;
}

static void
deleteFav()
{
	LRESULT idx, len;
	LONG lret;
	char name[256];
	HKEY key;

	if ((idx = SendMessage(sFavList, LB_GETCURSEL, 0, 0)) == LB_ERR) {
		EnableWindow(sDelBtn, FALSE);
		return;
	}

	len = SendMessage(sFavList, LB_GETTEXTLEN, idx, 0);
	if (!len || len >= (LRESULT)sizeof(name))
		err(1, "Invalid name.");
	if (SendMessage(sFavList, LB_GETTEXT, idx, (LPARAM)name) == LB_ERR)
		err(1, "LB_GETTEXT failed.");
	name[len] = '\0';

	lret = RegOpenKeyEx(HKEY_CURRENT_USER, "Software\\Netwake\\Favourites", 0,
	    KEY_SET_VALUE, &key);
	if (lret != ERROR_SUCCESS)
		err(1, "RegOpenKeyEx() failed.");
	if (RegDeleteValue(key, name) != ERROR_SUCCESS)
		err(1, "RegDeleteValue() failed.");
	RegCloseKey(key);

	SendMessage(sFavList, LB_DELETESTRING, idx, 0);
	EnableWindow(sDelBtn, FALSE);
}

static void
sendWol(void)
{
	char buf[256];
	tMacAddr mac;
	HKEY key;
	LONG lret;

	GetWindowText(sMacField, buf, sizeof(buf));

	if (ParseMacAddr(buf, &mac) == -1) {
		MessageBox(sWnd, sBadMacText, "Netwake",
		    MB_ICONEXCLAMATION | MB_OK);
		return;
	}

	FormatMacAddr(&mac, buf);
	SetWindowText(sMacField, buf);
	SendWolPacket(&mac);

	MessageBox(sWnd, "Wake-on-LAN packet sent!", "Netwake",
	    MB_ICONINFORMATION | MB_OK);

	lret = RegCreateKeyExA(HKEY_CURRENT_USER, "Software\\Netwake", 0, NULL,
	    REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &key, NULL);
	if (lret == ERROR_SUCCESS) {
		RegSetValueEx(key, "LastAddress", 0, REG_SZ, (BYTE *)buf,
		    strlen(buf)+1);
		RegCloseKey(key);
	}
}

static LRESULT CALLBACK
wndProc(HWND wnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	RECT *rectp;
	HWND focus;

	switch (msg) {
	case WM_SYSCOLORCHANGE:
	case CPAT_WM_THEMECHANGED:
		relayoutControls();
		return 0;
	case CPAT_WM_DPICHANGED:
		sDpi = HIWORD(wparam);
		rectp = (RECT *)lparam;

		relayoutControls();
		MoveWindow(wnd, rectp->left, rectp->top,
		    rectp->right - rectp->left,
		    rectp->bottom - rectp->top, TRUE);
		return 0;
	case WM_COMMAND:
		focus = GetFocus();
		if ((HWND)lparam == sQuitBtn)
			DestroyWindow(sWnd);
		else if ((HWND)lparam == sWakeBtn)
			sendWol();
		else if ((HWND)lparam == sFavList && HIWORD(wparam) == LBN_SELCHANGE)
			loadFav();
		else if ((HWND)lparam == sFavList && HIWORD(wparam) == LBN_DBLCLK) {
			if (loadFav() != -1)
				sendWol();
		} else if ((HWND)lparam == sDelBtn)
			deleteFav();
		else if ((HWND)lparam == sSaveBtn)
			saveFav();
		else if (lparam) /* accelerators from here on */
			break;
		else if (LOWORD(wparam) == IDC_ENTER && focus == sMacField)
			sendWol();
		else if (LOWORD(wparam) == IDC_ENTER && focus == sNameField)
			saveFav();
		else if (LOWORD(wparam) == IDC_ENTER && focus == sFavList) {
			if (loadFav() != -1)
				sendWol();
		} else if (LOWORD(wparam) == IDC_DELETE && focus == sFavList)
			deleteFav();
		else if (LOWORD(wparam) == IDC_DELETE) /* ugh */
			SendMessage(focus, WM_KEYDOWN, VK_DELETE, 0);
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
	HACCEL accel;
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

	accel = LoadAccelerators(sInstance, MAKEINTRESOURCE(IDA_ACCELS));
	if (!accel)
		err(1, "LoadAccelerators() failed.");

	ZeroMemory(&wc, sizeof(wc));
	wc.hInstance = instance;
	wc.lpszClassName = sClassName;
	wc.lpfnWndProc = wndProc;
	wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE+1);

	if (!RegisterClass(&wc))
		err(1, "RegisterClass failed.");

	if (sGetDpiForSystem)
		sBaseDpi = sDpi = sGetDpiForSystem();

	rect = sWndSize;
	rect.right  = MulDiv(rect.right, sDpi, 96);
	rect.bottom = MulDiv(rect.bottom, sDpi, 96);

	if (!AdjustWindowRect(&rect, sWndStyle, FALSE))
		err(1, "AdjustWindowRect() failed.");

	sWnd = CreateWindow(sClassName, "Netwake", sWndStyle,
	    CW_USEDEFAULT, CW_USEDEFAULT,
	    rect.right - rect.left, rect.bottom - rect.top,
	    NULL, NULL, instance, NULL);
	if (!sWnd)
		err(1, "CreateWindow failed");

	createControls();
	loadPrefs();
	SetFocus(sMacField);
	ShowWindow(sWnd, showCmd);

	while ((ret = GetMessage(&msg, NULL, 0, 0)) > 0) {
		if (TranslateAccelerator(sWnd, accel, &msg))
			continue;
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

#ifdef NOSTDLIB
void
WinMainCRTStartup(void)
{
	ExitProcess(WinMain(GetModuleHandle(NULL), NULL, "", 0));
}
#endif
