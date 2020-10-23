#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock.h>
#include <commctrl.h>
#include "../util.h"
#include "../wol.h"
#include "resource.h"

#define CPAT_WM_THEMECHANGED	0x031A
#define CPAT_WM_DPICHANGED	0x02E0

#define ID_WAKEBTN	101
#define ID_SAVEBTN	102

static UINT (*sGetDpiForSystem)(void);
static UINT (*sInitCommonControls)(void);

static const char sClassName[] = "Netwake";
static const DWORD sWndStyle = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU |
    WS_MINIMIZEBOX;
static const RECT sWndSize = {0, 0, 456, 272};

static HINSTANCE sInstance;
static HFONT sFont;
static WORD sBaseDpi = 96, sDpi = 96;
static LONG sFontSize = 11; /* system font, in pts */

static char sAppTitle[128];

static HWND sWnd;
static HWND sInfoFrame, sInfoLabel;
static HWND sMacLabel, sMacField;
static HWND sWakeBtn;
static HWND sNameLabel, sNameField;
static HWND sFavLabel, sFavList;
static HWND sQuitBtn, sDelBtn, sSaveBtn;

static const struct {
	HWND *wnd;
	int x, y, w, h; /* 1/8th of font size (1px on Windows 95-XP) */
	const char *className;
	int id, textRes;
	DWORD style, exStyle;
} sCtrlDefs[] = {
	{&sInfoFrame, -8, -16, 168, 298, "BUTTON", 0, 0, WS_GROUP | BS_GROUPBOX,
	    0},
	{&sInfoLabel, 8, 8, 140, 256, "STATIC", 0, IDS_INFOTEXT, 0, 0},
	{&sMacLabel, 168, 10, 96, 17, "STATIC", 0, IDS_MACLABEL, 0, 0},
	{&sMacField, 264, 8, 184, 21, "EDIT", 0, 0, WS_TABSTOP,
	    WS_EX_CLIENTEDGE},
	{&sWakeBtn, 373, 33, 75, 23, "BUTTON", ID_WAKEBTN, IDS_WAKEBTN,
	    WS_TABSTOP | BS_DEFPUSHBUTTON, 0},
	{&sNameLabel, 168, 74, 96, 17, "STATIC", 0, IDS_NAMELABEL, 0, 0},
	{&sNameField, 264, 72, 184, 21, "EDIT", 0, 0, WS_TABSTOP ,
	    WS_EX_CLIENTEDGE},
	{&sFavLabel, 168, 98, 96, 17, "STATIC", 0, IDS_FAVLABEL, 0, 0},
	{&sFavList, 264, 96, 184, 140, "LISTBOX", 0, 0, WS_TABSTOP | WS_VSCROLL |
	    LBS_SORT | LBS_NOTIFY | LBS_WANTKEYBOARDINPUT |
	    LBS_NOINTEGRALHEIGHT, WS_EX_CLIENTEDGE},
	{&sQuitBtn, 8, 240, 75, 23, "BUTTON", 0, IDS_QUITBTN, WS_TABSTOP, 0},
	{&sDelBtn, 293, 240, 75, 23, "BUTTON", 0, IDS_DELBTN, WS_TABSTOP |
	    WS_DISABLED, 0},
	{&sSaveBtn, 373, 240, 75, 23, "BUTTON", ID_SAVEBTN, IDS_SAVEBTN,
	    WS_TABSTOP, 0}
};


static void
warn(int msgRes)
{
	char msg[1024];

	if (!LoadString(sInstance, msgRes, msg, sizeof(msg))) {
		MessageBox(sWnd,  "An error occured, but the description is "
		    "unavailable.", sAppTitle, MB_OK | MB_ICONEXCLAMATION);
		return;
	}

	MessageBox(sWnd, msg, sAppTitle, MB_OK | MB_ICONEXCLAMATION);
}

static void __declspec(noreturn)
err(int msgRes)
{
	warn(msgRes);
	ExitProcess(1);
}

static void
info(int msgRes)
{
	char msg[1024];

	if (!LoadString(sInstance, msgRes, msg, sizeof(msg))) {
		warn(IDS_ERESMISS);
		return;
	}

	MessageBox(sWnd, msg, sAppTitle, MB_OK | MB_ICONINFORMATION);
}

static int
scale(int magnitude)
{
	/*
	 * 1. Convert from pt to px:                    72/sBaseDpi
	 * 2. Apply display scale factor (96 as base):  sDpi/96
	 * 3. Scale by font size (8 as base):           sFontSize/8
	 */
	return MulDiv(magnitude, 72 * sDpi * sFontSize, sBaseDpi * 96 * 8);
}

static void
loadFunctions(void)
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
}

static void
setupWinsock(void)
{
	WSADATA data;

	if (WSAStartup(MAKEWORD(1, 1), &data))
		err(IDS_ESOCKSETUP);
}

static void
loadFont(void)
{
	NONCLIENTMETRICS metrics;
	LOGFONT *lfont;
	BOOL bret;

	ZeroMemory(&metrics, sizeof(metrics));
	metrics.cbSize = sizeof(metrics);

	bret = SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(metrics),
	    &metrics, 0);
	if (!bret)
		return; /* sFontSize has a suitable default */

	sFontSize = -metrics.lfMessageFont.lfHeight;

	lfont = &metrics.lfMessageFont;
	lfont->lfHeight = MulDiv(lfont->lfHeight, sDpi, sBaseDpi);
	lfont->lfWidth = MulDiv(lfont->lfWidth, sDpi, sBaseDpi);

	if (sFont)
		DeleteObject(sFont);

	sFont = CreateFontIndirect(lfont);
}

static void
createControls(void)
{
	int i;
	char str[512];

	for (i=0; i < (int)LEN(sCtrlDefs); i++) {
		str[0] = '\0';
		LoadString(sInstance, sCtrlDefs[i].textRes, str, sizeof(str));

		*sCtrlDefs[i].wnd = CreateWindowEx(
		    sCtrlDefs[i].exStyle, sCtrlDefs[i].className, str,
		    sCtrlDefs[i].style | WS_VISIBLE | WS_CHILD,
		    scale(sCtrlDefs[i].x), scale(sCtrlDefs[i].y),
		    scale(sCtrlDefs[i].w), scale(sCtrlDefs[i].h),
		    sWnd, (HMENU)sCtrlDefs[i].id, sInstance, NULL);
		if (!*sCtrlDefs[i].wnd)
			err(IDS_ECREATEWND);

		if (sFont)
			SendMessage(*sCtrlDefs[i].wnd, WM_SETFONT,
			    (WPARAM)sFont, MAKELPARAM(FALSE, 0));
	}
}

static void
relayoutControls(void)
{
	int i;

	for (i=0; i < (int)LEN(sCtrlDefs); i++) {
		if (sFont)
			SendMessage(*sCtrlDefs[i].wnd, WM_SETFONT,
			    (WPARAM)sFont, MAKELPARAM(FALSE, 0));

		MoveWindow(*sCtrlDefs[i].wnd,
		    scale(sCtrlDefs[i].x), scale(sCtrlDefs[i].y),
		    scale(sCtrlDefs[i].w), scale(sCtrlDefs[i].h), TRUE);
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
		else {
			warn(IDS_EREGREAD);
			break;
		}
	}

	RegCloseKey(key);
}

static void
saveFav(void)
{
	char macStr[256], name[256];
	tMacAddr mac;
	HKEY key;
	LONG lret;

	GetWindowText(sMacField, macStr, sizeof(macStr));
	GetWindowText(sNameField, name, sizeof(name));

	if (!strlen(macStr) || !strlen(name)) {
		warn(IDS_REQNAMEMAC);
		return;
	}

	if (ParseMacAddr(macStr, &mac) == -1) {
		warn(IDS_BADMAC);
		return;
	}

	FormatMacAddr(&mac, macStr);
	SetWindowText(sMacField, macStr);

	lret = RegCreateKeyExA(HKEY_CURRENT_USER,
	    "Software\\Netwake\\Favourites", 0, NULL, REG_OPTION_NON_VOLATILE,
	    KEY_ALL_ACCESS, NULL, &key, NULL);
	if (lret != ERROR_SUCCESS) {
		RegCloseKey(key);
		warn(IDS_EREGWRITE);
		return;
	}

	lret = RegSetValueEx(key, name, 0, REG_SZ, (BYTE *)macStr,
	    strlen(macStr)+1);
	if (lret != ERROR_SUCCESS) {
		RegCloseKey(key);
		warn(IDS_EREGWRITE);
		return;
	}

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
	if (!len || len >= (LRESULT)sizeof(name) ||
	    SendMessage(sFavList, LB_GETTEXT, idx, (LPARAM)name) == LB_ERR) {
		warn(IDS_ENAMEREAD);
		return -1;
	}

	name[len] = '\0';

	lret = RegOpenKeyEx(HKEY_CURRENT_USER, "Software\\Netwake\\Favourites", 0,
	    KEY_READ, &key);
	if (lret != ERROR_SUCCESS)
		return -1;

	sz = (DWORD)sizeof(macStr);
	lret = RegQueryValueEx(key, name, NULL, &type, (BYTE *)macStr, &sz);
	if (lret != ERROR_SUCCESS || type != REG_SZ) {
		RegCloseKey(key);
		warn(IDS_EREGREAD);
		return -1;
	}

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
	if (!len || len >= (LRESULT)sizeof(name) ||
	    SendMessage(sFavList, LB_GETTEXT, idx, (LPARAM)name) == LB_ERR) {
		warn(IDS_ENAMEREAD);
		return;
	}

	name[len] = '\0';

	lret = RegOpenKeyEx(HKEY_CURRENT_USER, "Software\\Netwake\\Favourites",
	    0, KEY_SET_VALUE, &key);
	if (lret != ERROR_SUCCESS ||
	    RegDeleteValue(key, name) != ERROR_SUCCESS) {
		RegCloseKey(key);
		warn(IDS_EREGWRITE);
		return;
	}

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
		warn(IDS_BADMAC);
		return;
	}

	FormatMacAddr(&mac, buf);
	SetWindowText(sMacField, buf);

	if (SendWolPacket(&mac) == -1) {
		warn(IDS_ESOCKERR);
		return;
	}

	info(IDS_WOLSENT);

	lret = RegCreateKeyExA(HKEY_CURRENT_USER, "Software\\Netwake", 0, NULL,
	    REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &key, NULL);
	if (lret == ERROR_SUCCESS) {
		RegSetValueEx(key, "LastAddress", 0, REG_SZ, (BYTE *)buf,
		    strlen(buf)+1);
		RegCloseKey(key);
	}
}

static void
updateDefBtn(void)
{
	WORD id;

	id = LOWORD(SendMessage(sWnd, DM_GETDEFID, 0, 0));
	SendMessage(sWakeBtn, BM_SETSTYLE,
	    id == ID_WAKEBTN ? BS_DEFPUSHBUTTON : 0, MAKELPARAM(TRUE, 0));
	SendMessage(sSaveBtn, BM_SETSTYLE,
	    id == ID_SAVEBTN ? BS_DEFPUSHBUTTON : 0, MAKELPARAM(TRUE, 0));
}

static LRESULT CALLBACK
wndProc(HWND wnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	RECT *rectp, frame, client;
	HWND sender, focus;
	WORD cmd, event;

	switch (msg) {
	case WM_SYSCOLORCHANGE:
	case CPAT_WM_THEMECHANGED:
		loadFont();
		relayoutControls();

		GetWindowRect(sWnd, &frame);
		GetClientRect(sWnd, &client);

		frame.right += scale(sWndSize.right) - client.right;
		frame.bottom += scale(sWndSize.bottom) - client.bottom;

		AdjustWindowRect(&client, sWndStyle, FALSE);
		MoveWindow(sWnd, frame.left, frame.top,
		    frame.right - frame.left,
		    frame.bottom - frame.top, TRUE);
		return 0;

	case CPAT_WM_DPICHANGED:
		sDpi = HIWORD(wparam);
		rectp = (RECT *)lparam;

		loadFont();
		relayoutControls();

		MoveWindow(wnd, rectp->left, rectp->top,
		    rectp->right - rectp->left,
		    rectp->bottom - rectp->top, TRUE);
		return 0;

	case DM_GETDEFID:
		focus = GetFocus();
		if (focus == sMacField || focus == sFavList)
			return MAKELRESULT(ID_WAKEBTN, DC_HASDEFID);
		else if (focus == sNameField)
			return MAKELRESULT(ID_SAVEBTN, DC_HASDEFID);
		else
			return 0;

	case WM_COMMAND:
		sender = (HWND)lparam;
		focus = GetFocus();
		cmd = LOWORD(wparam);
		event = HIWORD(wparam);

		if (sender == sQuitBtn)
			DestroyWindow(sWnd);
		else if (sender == sWakeBtn)
			sendWol();
		else if (sender == sFavList && event == LBN_SELCHANGE)
			loadFav();
		else if (sender == sFavList && event == LBN_DBLCLK) {
			if (loadFav() != -1)
				sendWol();
		} else if (sender == sDelBtn)
			deleteFav();
		else if (sender == sSaveBtn)
			saveFav();
		else if (event == EN_SETFOCUS || event == LBN_SETFOCUS)
			updateDefBtn();
		else if (sender) /* accelerators from here on */
			break;
		else if (cmd == IDC_ENTER && focus == sMacField)
			sendWol();
		else if (cmd == IDC_ENTER && focus == sNameField)
			saveFav();
		else if (cmd == IDC_ENTER && focus == sFavList) {
			if (loadFav() != -1)
				sendWol();
		} else if (cmd == IDC_DELETE && focus == sFavList)
			deleteFav();
		else if (cmd == IDC_DELETE) /* ugh */
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

	if (sInitCommonControls)
		sInitCommonControls();
	if (sGetDpiForSystem)
		sBaseDpi = sDpi = sGetDpiForSystem();

	loadFont();

	accel = LoadAccelerators(sInstance, MAKEINTRESOURCE(IDA_ACCELS));
	if (!accel)
		err(IDS_ERESMISS);

	if (!LoadString(sInstance, IDS_APPTITLE, sAppTitle, sizeof(sAppTitle)))
		err(IDS_ERESMISS);

	ZeroMemory(&wc, sizeof(wc));
	wc.hInstance = instance;
	wc.lpszClassName = sClassName;
	wc.lpfnWndProc = wndProc;
	wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE+1);

	if (!RegisterClass(&wc))
		err(IDS_EREGCLASS);

	rect = sWndSize;
	rect.right = scale(rect.right);
	rect.bottom = scale(rect.bottom);

	AdjustWindowRect(&rect, sWndStyle, FALSE);

	sWnd = CreateWindow(sClassName, sAppTitle, sWndStyle,
	    CW_USEDEFAULT, CW_USEDEFAULT,
	    rect.right - rect.left, rect.bottom - rect.top,
	    NULL, NULL, instance, NULL);
	if (!sWnd)
		err(IDS_ECREATEWND);

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
	HINSTANCE instance;
	int st;

	instance = GetModuleHandle(NULL);
	st = WinMain(instance, NULL, "", SW_SHOWNORMAL);
	ExitProcess(st);
}
#endif
