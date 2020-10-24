void InitPrefs(int is95Up);

BOOL (*ReadLastMac)(char *buf, DWORD sz);
BOOL (*WriteLastMac)(const char *val);

BOOL (*ReadFavNames)(void (*callback)(const char *));
BOOL (*ReadFav)(const char *name, char *buf, DWORD sz);
BOOL (*WriteFav)(const char *name, const char *val);
BOOL (*DeleteFav)(const char *name);
