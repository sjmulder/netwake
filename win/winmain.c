#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <commctrl.h>

#define CPAT_WM_THEMECHANGED	0x031A
#define CPAT_WM_DPICHANGED	0x02E0

#define LEN(a) (sizeof(a)/sizeof(*(a)))

typedef struct MacAddr {char bytes[6];} tMacAddr;
typedef struct WolPacket {char zeroes[6]; tMacAddr macs[16];} tWolPacket;

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

static void __declspec(noreturn)
err(const char *msg)
{
	MessageBox(sWnd, msg, "Netwake", MB_OK | MB_ICONEXCLAMATION);
	ExitProcess(1);
}

static void
loadFunctions(void)
{
	HMODULE user32;

	if (!(user32 = LoadLibrary("user32.dll")))
		err("LoadLibrary(user32.dl) failed");
	sGetDpiForSystem = (void *)GetProcAddress(user32, "GetDpiForSystem");
}

static void
setupWinsock(void)
{
	WSADATA data;

	if (WSAStartup(MAKEWORD(2, 0), &data))
		err("WSAStartup failed");
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
	    	err("SystemParametersInfo failed");

	lfont = &metrics.lfMessageFont;
	lfont->lfHeight = MulDiv(lfont->lfHeight, sDpi, sBaseDpi);
	lfont->lfWidth = MulDiv(lfont->lfWidth, sDpi, sBaseDpi);

	if (!(sFont = CreateFontIndirect(lfont)))
		err("CreateFontIndirect failed");

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
			err("Failed to create window");
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

static int
parseMac(const char *str, tMacAddr *mac)
{
	int inPos, outPos=0;
	char c;

	ZeroMemory(mac, sizeof(*mac));

	for (inPos=0; str[inPos]; inPos++) {
		if (outPos >= (int)LEN(mac->bytes)*2)
			return -1;
		else if ((c = str[inPos]) == ':')
			continue;
		else if (c >= '0' && c <= '9') c -= '0';
		else if (c >= 'a' && c <= 'f') c-= 'a'-10;
		else if (c >= 'A' && c <= 'F') c-= 'A'-10;
		else
			return -1;

		mac->bytes[outPos/2] |= c << (!(outPos & 1) * 4);
		outPos++;
	}

	if (outPos < (int)LEN(mac->bytes)*2)
		return -1;

	return 0;
}

static void
sendWol(void)
{
	char buf[256];
	int i;
	tMacAddr mac;
	tWolPacket wol;
	struct sockaddr_in addr;
	SOCKET sock;

	GetWindowText(sMacField, buf, sizeof(buf));

	if (parseMac(buf, &mac) == -1) {
		MessageBox(sWnd, "Invalid MAC address.\r\n\r\n"
		    "The address should consist of 8 pairs of hexadecimal "
		    "digits (0-A), optionally separated by colons.\r\n\r\n"
		    "Example: d2:32:e4:87:85:24", "Netwake",
		    MB_ICONEXCLAMATION | MB_OK);
		return;
	}

	ZeroMemory(&wol, sizeof(wol));
	for (i=0; i < (int)LEN(wol.zeroes); i++)
		wol.zeroes[i] = (char)0xFF;
	for (i=0; i < (int)LEN(wol.macs); i++)
		wol.macs[i] = mac;

	ZeroMemory(&addr, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = 9;
	addr.sin_addr.S_un.S_addr = 0xFFFFFFFF;

	if ((sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == INVALID_SOCKET)
		err("socket() failed");
	if (connect(sock, (struct sockaddr *)&addr, sizeof(addr))
	    == SOCKET_ERROR)
		err("connect() failed");
	if (send(sock, (void *)&wol, sizeof(wol), 0) == SOCKET_ERROR)
		err("send() failed");

	closesocket(sock);

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
		err("RegisterClass failed");

	if (sGetDpiForSystem)
		sBaseDpi = sDpi = sGetDpiForSystem();

	rect = sWndSize;
	rect.right  = MulDiv(rect.right, sDpi, 96);
	rect.bottom = MulDiv(rect.bottom, sDpi, 96);

	if (!AdjustWindowRect(&rect, sWndStyle, FALSE))
		err("AdjustWindowRect failed");

	sWnd = CreateWindow(sClassName, "Netwake", sWndStyle,
	    CW_USEDEFAULT, CW_USEDEFAULT,
	    rect.right - rect.left, rect.bottom - rect.top,
	    NULL, NULL, instance, NULL);
	if (!sWnd)
		err("CreateWindow failed");

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
