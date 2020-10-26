#define WIN32_LEAN_AND_MEAN
#define PREFS_EXTERN

#include <windows.h>
#include "prefs.h"

static const char sIniName[] = "netwake.ini";
static const char sRegRoot[] = "Software\\Netwake";
static const char sRegFavs[] = "Software\\Netwake\\Favourites";

static BOOL
readLastMacReg(char *buf, DWORD sz)
{
	HKEY key;
	DWORD type, err;
	LONG lret;

	lret = RegOpenKeyEx(HKEY_CURRENT_USER, sRegRoot, 0, KEY_READ, &key);
	if (lret != ERROR_SUCCESS)
		return FALSE;

	lret = RegQueryValueEx(key, "LastAddress", NULL, &type, (BYTE *)buf,
	    &sz);
	if (lret != ERROR_SUCCESS && type != REG_SZ) {
		err = GetLastError();
		RegCloseKey(key);
		SetLastError(err);
		return FALSE;
	}

	RegCloseKey(key);
	return TRUE;
}

static BOOL
writeLastMacReg(const char *val)
{
	HKEY key;
	DWORD err;
	LONG lret;

	lret = RegCreateKeyExA(HKEY_CURRENT_USER, sRegRoot, 0, NULL,
	    REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &key, NULL);
	if (lret != ERROR_SUCCESS)
		return FALSE;

	lret = RegSetValueEx(key, "LastAddress", 0, REG_SZ, (BYTE *)val,
	    strlen(val)+1);
	if (lret != ERROR_SUCCESS) {
		err = GetLastError();
		RegCloseKey(key);
		SetLastError(err);
		return FALSE;
	}

	RegCloseKey(key);
	return TRUE;
}

static BOOL
readFavNamesReg(void (*callback)(const char *))
{
	HKEY key;
	char buf[256];
	DWORD sz, i, err;
	LONG lret;

	lret = RegOpenKeyEx(HKEY_CURRENT_USER, sRegFavs, 0, KEY_READ, &key);
	if (lret != ERROR_SUCCESS)
		return FALSE;

	for (i=0; ; i++) {
		sz = (DWORD)sizeof(buf);
		lret = RegEnumValue(key, i, buf, &sz, NULL, NULL, NULL, NULL);
		if (lret == ERROR_SUCCESS)
			callback(buf);
		else if (lret == ERROR_NO_MORE_ITEMS)
			break;
		else {
			err = GetLastError();
			RegCloseKey(key);
			SetLastError(err);
			return FALSE;
		}
	}

	RegCloseKey(key);
	return TRUE;
}

static BOOL
readFavReg(const char *name, char *buf, DWORD sz)
{
	LONG lret;
	HKEY key;
	DWORD type, err;

	lret = RegOpenKeyEx(HKEY_CURRENT_USER, sRegFavs, 0, KEY_READ, &key);
	if (lret != ERROR_SUCCESS)
		return FALSE;

	lret = RegQueryValueEx(key, name, NULL, &type, (BYTE *)buf, &sz);
	if (lret != ERROR_SUCCESS || type != REG_SZ) {
		err = GetLastError();
		RegCloseKey(key);
		SetLastError(err);
		return FALSE;
	}

	RegCloseKey(key);
	return TRUE;
}

static BOOL
writeFavReg(const char *name, const char *val)
{
	HKEY key;
	DWORD err;
	LONG lret;

	lret = RegCreateKeyExA(HKEY_CURRENT_USER, sRegFavs, 0, NULL,
	    REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &key, NULL);
	if (lret != ERROR_SUCCESS)
		goto fail;

	lret = RegSetValueEx(key, name, 0, REG_SZ, (BYTE *)val, strlen(val)+1);
	if (lret != ERROR_SUCCESS)
		goto fail;

	RegCloseKey(key);
	return TRUE;

fail:
	err = GetLastError();
	RegCloseKey(key);
	SetLastError(err);
	return FALSE;
}

static BOOL
deleteFavReg(const char *name)
{
	HKEY key;
	DWORD err;
	LONG lret;

	lret = RegOpenKeyEx(HKEY_CURRENT_USER, sRegFavs, 0, KEY_SET_VALUE,
	    &key);
	if (lret != ERROR_SUCCESS ||
	    RegDeleteValue(key, name) != ERROR_SUCCESS) {
		err = GetLastError();
		RegCloseKey(key);
		SetLastError(err);
		return FALSE;
	}

	RegCloseKey(key);
	return TRUE;
}

static BOOL
readLastMacIni(char *buf, DWORD sz)
{
	buf[0] = '\0';
	GetPrivateProfileString("Main", "LastAddress", NULL, buf, sz, sIniName);

	return TRUE;
}

static BOOL
writeLastMacIni(const char *val)
{
	return WritePrivateProfileString("Main", "LastAddress", val, sIniName);
}

static BOOL
readFavNamesIni(void (*callback)(const char *))
{
	char buf[1024], *name;

	buf[0] = '\0';
	buf[1] = '\0';
	GetPrivateProfileString("Favourites", NULL, NULL, buf, sizeof(buf),
	    sIniName);

	for (name = buf; *name; name += strlen(name)+1)
		callback(name);

	return TRUE;
}

static BOOL
readFavIni(const char *name, char *buf, DWORD sz)
{
	buf[0] = '\0';
	GetPrivateProfileString("Favourites", name, NULL, buf, sz, sIniName);

	return TRUE;
}

static BOOL
writeFavIni(const char *name, const char *val)
{
	return WritePrivateProfileString("Favourites", name, val, sIniName);
}

static BOOL
deleteFavIni(const char *name)
{
	return WritePrivateProfileString("Favourites", name, NULL, sIniName);
}

void
InitPrefs(int is95Up)
{
	ReadLastMac = is95Up ? readLastMacReg : readLastMacIni;
	WriteLastMac = is95Up ? writeLastMacReg : writeLastMacIni;

	ReadFavNames = is95Up ? readFavNamesReg : readFavNamesIni;
	ReadFav = is95Up ? readFavReg : readFavIni;
	WriteFav = is95Up ? writeFavReg : writeFavIni;
	DeleteFav = is95Up ? deleteFavReg : deleteFavIni;
}
