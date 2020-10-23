#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock.h>
#include <commctrl.h>
#include "../wol.h"
#include "resource.h"

#define LEN(a) (sizeof(a)/sizeof(*(a)))

#define CPAT_WM_THEMECHANGED	0x031A
#define CPAT_WM_DPICHANGED	0x02E0

#define ID_WAKEBTN	101
#define ID_SAVEBTN	102

static DWORD (*sGetVersion)(void);
static UINT (*sGetDpiForSystem)(void);
static UINT (*sInitCommonControls)(void);

static const char sClassName[] = "Netwake";
static const char sIniName[] = "netwake.ini";
static const char sRegRoot[] = "Software\\Netwake";
static const char sRegFavs[] = "Software\\Netwake\\Favourites";

static const DWORD sWndStyle = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU |
    WS_MINIMIZEBOX;
static const RECT sWndSize = {0, 0, 456, 272};

static HINSTANCE sInstance;
static HFONT sFont;
static WORD sBaseDpi = 96, sDpi = 96;
static LONG sFontSize = 11; /* system font, in pts */
static BOOL sIs95Up;

static char sAppTitle[128];

static HWND sWnd;
static HWND sInfoFrame, sInfoLabel;
static HWND sMacLabel, sMacField;
static HWND sWakeBtn;
static HWND sNameLabel, sNameField;
static HWND sFavLabel, sFavList;
static HWND sQuitBtn, sDelBtn, sSaveBtn;

/* sort of declarative layout, see createControls() */
static const struct {
	HWND *wnd;
	int x, y, w, h;		/* 1/8th of font size (1px on Windows 95-XP) */
	const char *className;
	int id, textRes;	/* both optional */
	DWORD style, exStyle;	/* some logic applies, see createControls() */
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
strAppend(char *dst, size_t *len, size_t sz, char *src)
{
	size_t i;

	for (i = 0; src[i] && *len < sz-1; i++)
		dst[(*len)++] = src[i];
	dst[*len] = '\0';
}

static void
strAppendInt(char *dst, size_t *len, size_t sz, int num)
{
	char numStr[16];
	size_t numLen=0, i;

	for (; num && numLen < sizeof(numStr); num /= 10)
		numStr[numLen++] = '0' + (num % 10);
	for (i = numLen; i && *len < sz-1; i--)
		dst[(*len)++] = numStr[i-1];
}

static void
warn(int msgRes)
{
	char msg[1024];
	DWORD err;
	size_t len;

	if (!LoadString(sInstance, msgRes, msg, sizeof(msg))) {
		err = GetLastError();

		strAppend(msg, &len, sizeof(msg), "An error occured, but the "
		    "description no. ");
		strAppendInt(msg, &len, sizeof(msg), msgRes);
		strAppend(msg, &len, sizeof(msg), " could not be loaded:"
		    "\r\n\r\n");

		FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, err, 0,
		    &msg[len], sizeof(msg) - len, NULL);

		MessageBox(sWnd, msg, sAppTitle, MB_OK | MB_ICONEXCLAMATION);
		return;
	}

	MessageBox(sWnd, msg, sAppTitle, MB_OK | MB_ICONEXCLAMATION);
}

static void
warnWin(int prefixRes)
{
	char msg[1024];
	DWORD err, err2;
	size_t len;

	err = GetLastError();

	if (!(len = LoadString(sInstance, prefixRes, msg, sizeof(msg)))) {
		err2 = GetLastError();

		strAppend(msg, &len, sizeof(msg), "An error occured, but the "
		    "description no. ");
		strAppendInt(msg, &len, sizeof(msg), prefixRes);
		strAppend(msg, &len, sizeof(msg), " could not be loaded:"
		    "\r\n\r\n");

		len += FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, err2, 0,
		    &msg[len], sizeof(msg) - len, NULL);

		strAppend(msg, &len, sizeof(msg), "\r\nThe original error was:"
		    "\r\n\r\n");
		FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, err, 0,
		    &msg[len], sizeof(msg) - len, NULL);

		MessageBox(sWnd, msg, sAppTitle, MB_OK | MB_ICONEXCLAMATION);
		return;
	}

	strAppend(msg, &len, sizeof(msg), "\r\n\r\n");
	FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, err, 0,
	    &msg[len], sizeof(msg) - len, NULL);

	MessageBox(sWnd, msg, sAppTitle, MB_OK | MB_ICONEXCLAMATION);
}

static void __declspec(noreturn)
err(int msgRes)
{
	warn(msgRes);
	ExitProcess(1);
}

static void __declspec(noreturn)
errWin(int msgRes)
{
	warnWin(msgRes);
	ExitProcess(1);
}

static void
info(int msgRes)
{
	char msg[1024];

	if (!LoadString(sInstance, msgRes, msg, sizeof(msg))) {
		warnWin(IDS_ERESMISS);
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

	if (!(lib = LoadLibrary("kernel32.dll")))
		errWin(IDS_ELOADLIB);

	/*
	 * GetVersion() is obsolete and will yield a compile error on modern
	 * SDKs if invoked directly, but it's exactly what we need.
	 */
	if (!(sGetVersion = (void *)GetProcAddress(lib, "GetVersion")))
		errWin(IDS_ELOADLIB);

	/* not pretty to set here but we need it immediately */
	sIs95Up = LOBYTE(LOWORD(sGetVersion())) >= 4;

	if (!(lib = LoadLibrary("user32.dll")))
		errWin(IDS_ELOADLIB);

	/* optional, we'll assume 96 if unavailable */
	sGetDpiForSystem = (void *)GetProcAddress(lib, "GetDpiForSystem");

	if (sIs95Up) {
		/* attempting this on Win 3.11 yields a message box */
		if (!(lib = LoadLibrary("comctl32.dll")))
			errWin(IDS_ELOADLIB);

		sInitCommonControls = (void *)GetProcAddress(lib,
		    "InitCommonControls");
	}
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

	/* system returns a negative, which means points instead of pixels */
	sFontSize = -metrics.lfMessageFont.lfHeight;

	lfont = &metrics.lfMessageFont;
	lfont->lfHeight = MulDiv(lfont->lfHeight, sDpi, sBaseDpi);
	lfont->lfWidth = MulDiv(lfont->lfWidth, sDpi, sBaseDpi);

	if (sFont)
		DeleteObject(sFont);

	sFont = CreateFontIndirect(lfont);
}

static void
getControlRect(int idx, int *x, int *y, int *w, int *h)
{
	HWND *wnd;

	*x = scale(sCtrlDefs[idx].x);
	*y = scale(sCtrlDefs[idx].y);
	*w = scale(sCtrlDefs[idx].w);
	*h = scale(sCtrlDefs[idx].h);

	wnd = sCtrlDefs[idx].wnd;

	if (sIs95Up)
		;
	else if (wnd == &sMacField || wnd == &sNameField) {
		/* little bit larger on 3.11 to prevent clipping */
		*y -= 1;
		*h += 2;
	} else if (wnd == &sFavList) {
		/*
		 * On 95+ list boxes with WS_EX_CLIENTEDGE are 2 px smaller
		 * than edit controls with the same style. On 3.11 with
		 * WS_BORDER they aren't.
		 */
		*x += 1; *y += 1;
		*w -= 2; *h -= 2;
	}
}

static void
createControls(void)
{
	int i, x, y, w, h;
	char str[512];
	DWORD style;

	for (i=0; i < (int)LEN(sCtrlDefs); i++) {
		str[0] = '\0';
		LoadString(sInstance, sCtrlDefs[i].textRes, str, sizeof(str));

		style = sCtrlDefs[i].style | WS_VISIBLE | WS_CHILD;

		/*
		 * Windows 3.11 doesn't have ES_EX_CLIENTEDGE, so use WS_BORDER.
		 * Just applying WS_BORDER always doesn't work because that
		 * yields double borders sometimes.
		 */
		if (!sIs95Up && (sCtrlDefs[i].exStyle & WS_EX_CLIENTEDGE))
			style |= WS_BORDER;

		getControlRect(i, &x, &y, &w, &h);

		*sCtrlDefs[i].wnd = CreateWindowEx(
		    sCtrlDefs[i].exStyle, sCtrlDefs[i].className, str, style,
		    x, y, w, h, sWnd, (HMENU)sCtrlDefs[i].id, sInstance, NULL);
		if (!*sCtrlDefs[i].wnd)
			errWin(IDS_ECREATEWND);

		if (sFont)
			SendMessage(*sCtrlDefs[i].wnd, WM_SETFONT,
			    (WPARAM)sFont, MAKELPARAM(FALSE, 0));
	}
}

static void
relayoutControls(void)
{
	int i, x, y, w, h;

	for (i=0; i < (int)LEN(sCtrlDefs); i++) {
		if (sFont)
			SendMessage(*sCtrlDefs[i].wnd, WM_SETFONT,
			    (WPARAM)sFont, MAKELPARAM(FALSE, 0));

		getControlRect(i, &x, &y, &w, &h);
		MoveWindow(*sCtrlDefs[i].wnd, x, y, w, h, TRUE);
	}
}

static void
loadPrefsIni(void)
{
	char buf[1024], *name;

	buf[0] = '\0';
	GetPrivateProfileString("Main", "LastAddress", NULL, buf, sizeof(buf),
	    sIniName);
	SetWindowText(sMacField, buf);
	SendMessage(sMacField, EM_SETSEL, (WPARAM)0, (LPARAM)-1);

	buf[0] = '\0';
	buf[1] = '\0';
	GetPrivateProfileString("Favourites", NULL, NULL, buf, sizeof(buf),
	    sIniName);

	for (name = buf; *name; name += strlen(name)+1)
		SendMessage(sFavList, LB_ADDSTRING, 0, (LPARAM)name);
}

static void
loadPrefsReg(void)
{
	HKEY key;
	char buf[256];
	DWORD sz, type, i;
	LONG lret;

	lret = RegOpenKeyEx(HKEY_CURRENT_USER, sRegRoot, 0, KEY_READ, &key);
	if (lret != ERROR_SUCCESS)
		return;

	sz = (DWORD)sizeof(buf);
	lret = RegQueryValueEx(key, "LastAddress", NULL, &type, (BYTE *)buf,
	    &sz);
	if (lret == ERROR_SUCCESS && type == REG_SZ) {
		SetWindowText(sMacField, buf);
		SendMessage(sMacField, EM_SETSEL, (WPARAM)0, (LPARAM)-1);
	}

	RegCloseKey(key);

	lret = RegOpenKeyEx(HKEY_CURRENT_USER, sRegFavs, 0, KEY_READ, &key);
	if (lret != ERROR_SUCCESS)
		return;

	for (i=0; ; i++) {
		sz = (DWORD)sizeof(buf);
		lret = RegEnumValue(key, i, buf, &sz, NULL, NULL, NULL, NULL);
		if (lret == ERROR_SUCCESS)
			SendMessage(sFavList, LB_ADDSTRING, 0, (LPARAM)buf);
		else if (lret == ERROR_NO_MORE_ITEMS)
			break;
		else {
			warnWin(IDS_EREGREAD);
			break;
		}
	}

	RegCloseKey(key);
}

static void
loadPrefs(void)
{
	if (sIs95Up)
		loadPrefsReg();
	else
		loadPrefsIni();
}

static void
saveLastMac(const char *val)
{
	LONG lret;
	HKEY key;

	if (sIs95Up) {
		lret = RegCreateKeyExA(HKEY_CURRENT_USER, sRegRoot, 0, NULL,
		    REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &key, NULL);
		if (lret != ERROR_SUCCESS)
			return;

		RegSetValueEx(key, "LastAddress", 0, REG_SZ, (BYTE *)val,
		    strlen(val)+1);
		RegCloseKey(key);
	} else
		WritePrivateProfileString("Main", "LastAddress", val, sIniName);
}

static int
saveFavReg(const char *name, const char *val)
{
	LONG lret;
	HKEY key;

	lret = RegCreateKeyExA(HKEY_CURRENT_USER, sRegFavs, 0, NULL,
	    REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &key, NULL);
	if (lret != ERROR_SUCCESS) {
		warnWin(IDS_EREGWRITE);
		RegCloseKey(key);
		return -1;
	}

	lret = RegSetValueEx(key, name, 0, REG_SZ, (BYTE *)val, strlen(val)+1);
	if (lret != ERROR_SUCCESS) {
		warnWin(IDS_EREGWRITE);
		RegCloseKey(key);
		return -1;
	}

	RegCloseKey(key);
	return 0;
}

static int
saveFavIni(const char *name, const char *val)
{
	BOOL bret;

	bret = WritePrivateProfileString("Favourites", name, val, sIniName);
	if (!bret) {
		warnWin(IDS_EINIWRITE);
		return -1;
	}

	return 0;
}

static void
saveFav(void)
{
	char macStr[256], name[256];
	tMacAddr mac;

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

	if (sIs95Up) {
		if (saveFavReg(name, macStr) == -1)
			return;
	} else {
		if (saveFavIni(name, macStr) == -1)
			return;
	}

	if (SendMessage(sFavList, LB_FINDSTRING, 0, (LPARAM)name) == LB_ERR)
		SendMessage(sFavList, LB_ADDSTRING, 0, (LPARAM)name);
}

static int
loadFavReg(const char *name, char *val, size_t sz)
{
	LONG lret;
	HKEY key;
	DWORD sz2, type;

	lret = RegOpenKeyEx(HKEY_CURRENT_USER, sRegFavs, 0, KEY_READ, &key);
	if (lret != ERROR_SUCCESS)
		return -1;

	sz2 = sz;
	lret = RegQueryValueEx(key, name, NULL, &type, (BYTE *)val, &sz2);
	if (lret != ERROR_SUCCESS || type != REG_SZ) {
		warnWin(IDS_EREGREAD);
		RegCloseKey(key);
		return -1;
	}

	RegCloseKey(key);
	return 0;
}

static int
loadFavIni(const char *name, char *val, size_t sz)
{
	val[0] = '\0';

	GetPrivateProfileString("Favourites", name, NULL, val, sz, sIniName);
	if (!val[0]) {
		warn(IDS_EINIREAD);
		return -1;
	}

	return 0;
}

static int
loadFav(void)
{
	LRESULT idx, len;
	char name[256], macStr[256];

	if ((idx = SendMessage(sFavList, LB_GETCURSEL, 0, 0)) == LB_ERR)
		return -1;

	len = SendMessage(sFavList, LB_GETTEXTLEN, idx, 0);
	if (!len || len >= (LRESULT)sizeof(name)) {
		warn(IDS_ENAMEREAD);
		return -1;
	}

	if (SendMessage(sFavList, LB_GETTEXT, idx, (LPARAM)name) == LB_ERR) {
		warnWin(IDS_ENAMEREAD);
		return -1;
	}

	name[len] = '\0';

	if (sIs95Up) {
		if (loadFavReg(name, macStr, sizeof(macStr)) == -1)
			return -1;
	} else {
		if (loadFavIni(name, macStr, sizeof(macStr)) == -1)
			return -1;
	}

	SetWindowText(sMacField, macStr);
	SetWindowText(sNameField, name);
	EnableWindow(sDelBtn, TRUE);

	return 0;
}

static int
deleteFavReg(const char *name)
{
	LONG lret;
	HKEY key;

	lret = RegOpenKeyEx(HKEY_CURRENT_USER, sRegFavs, 0, KEY_SET_VALUE,
	    &key);
	if (lret != ERROR_SUCCESS ||
	    RegDeleteValue(key, name) != ERROR_SUCCESS) {
		warnWin(IDS_EREGWRITE);
		RegCloseKey(key);
		return -1;
	}

	RegCloseKey(key);
	return 0;
}

static int
deleteFavIni(const char *name)
{
	BOOL bret;

	bret = WritePrivateProfileString("Favourites", name, NULL, sIniName);
	if (!bret) {
		warnWin(IDS_EINIWRITE);
		return -1;
	}

	return 0;
}

static void
deleteFav()
{
	LRESULT idx, len;
	char name[256];

	if ((idx = SendMessage(sFavList, LB_GETCURSEL, 0, 0)) == LB_ERR) {
		EnableWindow(sDelBtn, FALSE);
		return;
	}

	len = SendMessage(sFavList, LB_GETTEXTLEN, idx, 0);
	if (!len || len >= (LRESULT)sizeof(name)) {
		warn(IDS_ENAMEREAD);
		return;
	}

	if (SendMessage(sFavList, LB_GETTEXT, idx, (LPARAM)name) == LB_ERR) {
		warnWin(IDS_ENAMEREAD);
		return;
	}

	name[len] = '\0';

	if (sIs95Up) {
		if (deleteFavReg(name) == -1)
			return;
	} else {
		if (deleteFavIni(name) == -1)
			return;
	}

	SendMessage(sFavList, LB_DELETESTRING, idx, 0);
	EnableWindow(sDelBtn, FALSE);
}

static void
sendWol(void)
{
	char buf[256];
	tMacAddr mac;

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
	saveLastMac(buf);
}

static void
updateDefBtn(void)
{
	WORD id;

	/*
	 * We want the 'Wake' button to be the default unless the cursor is in
	 * the 'Name' field for favourites because then you expect to save.
	 *
	 * Default state is lost when a button gets focus. Once the button loses
	 * focus again, it queries for what button should be the default
	 * (using DM_GETDEFID). We want that same behaviour for any focus,
	 * e.g. edit and list controls too, so that's what's implemented here.
	 */

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
		/*
		 * System font, color, theme, decoration etc. changed. We'll
		 * need to relayout, reload font, and update the window size
		 * because the client rect could have changed.
		 */
		loadFont();
		relayoutControls();

		GetWindowRect(sWnd, &frame);
		GetClientRect(sWnd, &client);

		/*
		 * Adjust frame by the difference between old client area size,
		 * and new desired client area size. It apppears that at this
		 * point AdjustWindowRect() doesn't use the new metrics yet.
		 */
		frame.right += scale(sWndSize.right) - client.right;
		frame.bottom += scale(sWndSize.bottom) - client.bottom;

		MoveWindow(sWnd, frame.left, frame.top,
		    frame.right - frame.left,
		    frame.bottom - frame.top, TRUE);
		return 0;

	case CPAT_WM_DPICHANGED:
		/*
		 * Here we get a new suggested window rect so no need to
		 * (possibly mis-)calculate one, but still need to relayout and
* 		 * reload fonts.
		 */
		sDpi = HIWORD(wparam);
		rectp = (RECT *)lparam;

		loadFont();
		relayoutControls();

		MoveWindow(wnd, rectp->left, rectp->top,
		    rectp->right - rectp->left,
		    rectp->bottom - rectp->top, TRUE);
		return 0;

	case DM_GETDEFID:
		/*
		 * Called by buttons and updateDefBtn() to learn what the
		 * default button ought to be.
* 		 */
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
			updateDefBtn(); /* see that function */
		else if (sender)
			break;
		/* accelerators (no sender) */
		else if (cmd == IDC_ENTER && focus == sMacField)
			sendWol();
		else if (cmd == IDC_ENTER && focus == sNameField)
			saveFav();
		else if (cmd == IDC_ENTER && focus == sFavList) {
			if (loadFav() != -1)
				sendWol();
		} else if (cmd == IDC_DELETE && focus == sFavList)
			deleteFav();
		else if (cmd == IDC_DELETE) {
			/*
			 * We've captured the delete key - if we don't forward
			 * it the key won't work in text boxes etc. There's
			 * probably a better way.
			 */
			SendMessage(focus, WM_KEYDOWN, VK_DELETE, 0);
		} else
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
	WSADATA data;

	(void)prev;
	(void)cmd;
	
	sInstance = instance;

	loadFunctions(); /* also sets sIs95Up */

	if (WSAStartup(MAKEWORD(1, 1), &data))
		err(IDS_ESOCKSETUP);
	if (sInitCommonControls)
		sInitCommonControls();
	if (sGetDpiForSystem)
		sBaseDpi = sDpi = sGetDpiForSystem();

	loadFont();

	accel = LoadAccelerators(sInstance, MAKEINTRESOURCE(IDA_ACCELS));
	if (!accel)
		errWin(IDS_ERESMISS);

	if (!LoadString(sInstance, IDS_APPTITLE, sAppTitle, sizeof(sAppTitle)))
		errWin(IDS_ERESMISS);

	ZeroMemory(&wc, sizeof(wc));
	wc.hInstance = instance;
	wc.lpszClassName = sClassName;
	wc.lpfnWndProc = wndProc;
	wc.hbrBackground = (HBRUSH)(sIs95Up ? COLOR_BTNFACE+1 : COLOR_WINDOW+1);

	if (!RegisterClass(&wc))
		errWin(IDS_EREGCLASS);

	rect = sWndSize;
	rect.right = scale(rect.right);
	rect.bottom = scale(rect.bottom);

	AdjustWindowRect(&rect, sWndStyle, FALSE);

	sWnd = CreateWindow(sClassName, sAppTitle, sWndStyle,
	    CW_USEDEFAULT, CW_USEDEFAULT,
	    rect.right - rect.left, rect.bottom - rect.top,
	    NULL, NULL, instance, NULL);
	if (!sWnd)
		errWin(IDS_ECREATEWND);

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
