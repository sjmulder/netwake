#ifndef PREFS_EXTERN
# define PREFS_EXTERN extern
#endif

void InitPrefs(int is95Up);

PREFS_EXTERN BOOL (*ReadLastMac)(char *buf, DWORD sz);
PREFS_EXTERN BOOL (*WriteLastMac)(const char *val);

PREFS_EXTERN BOOL (*ReadFavNames)(void (*callback)(const char *));
PREFS_EXTERN BOOL (*ReadFav)(const char *name, char *buf, DWORD sz);
PREFS_EXTERN BOOL (*WriteFav)(const char *name, const char *val);
PREFS_EXTERN BOOL (*DeleteFav)(const char *name);
