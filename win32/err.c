#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include "err.h"
#include "resource.h"

static HINSTANCE sInstance;
static const char *sAppTitle;
static const HWND *sWndPtr;

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

void
Info(int msgRes)
{
	char msg[1024];

	if (!LoadString(sInstance, msgRes, msg, sizeof(msg))) {
		WarnWin(IDS_ERESMISS);
		return;
	}

	MessageBox(*sWndPtr, msg, sAppTitle, MB_OK | MB_ICONINFORMATION);
}

void
InitErr(HINSTANCE instance, const char *appTitle, const HWND *wndPtr)
{
	sInstance = instance;
	sAppTitle = appTitle;
	sWndPtr = wndPtr;
}

void
Warn(int msgRes)
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

		MessageBox(*sWndPtr, msg, sAppTitle, MB_OK |
		    MB_ICONEXCLAMATION);
		return;
	}

	MessageBox(*sWndPtr, msg, sAppTitle, MB_OK | MB_ICONEXCLAMATION);
}

void
WarnWin(int prefixRes)
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

		MessageBox(*sWndPtr, msg, sAppTitle, MB_OK |
		    MB_ICONEXCLAMATION);
		return;
	}

	strAppend(msg, &len, sizeof(msg), "\r\n\r\n");
	FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, err, 0,
	    &msg[len], sizeof(msg) - len, NULL);

	MessageBox(*sWndPtr, msg, sAppTitle, MB_OK | MB_ICONEXCLAMATION);
}

void __declspec(noreturn)
Err(int msgRes)
{
	Warn(msgRes);
	ExitProcess(1);
}

void __declspec(noreturn)
ErrWin(int msgRes)
{
	WarnWin(msgRes);
	ExitProcess(1);
}
