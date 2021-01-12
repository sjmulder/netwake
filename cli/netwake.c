#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <err.h>
#include "../common/wol.h"
#include "config.h"

#ifndef PREFIX
#define PREFIX "/usr/local"
#endif

static char *
lookup_in(const char *path, const char *name)
{
	static char l[1024], val[64], valname[64];
	char *p;
	FILE *f;

	if (!(f = fopen(path, "r")))
		return NULL;

	while ((p = fgets(l, sizeof(l), f))) {
		if ((p = strchr(l, '#')))
			*p = '\0';
		if (sscanf(l, " %64s %64s", val, valname) != 2)
			continue;
		if (!strcmp(valname, name)) {
			fclose(f);
			return val;
		}
	}

	fclose(f);
	return NULL;
}

static char *
lookup(const char *name)
{
	static char p[1024];
	char *home, *home_cfg, *val;

	home = getenv("HOME");
	home_cfg = getenv("XDG_CONFIG_HOME");

	if (home_cfg) {
		snprintf(p, sizeof(p), "%s/woltab", home_cfg);
		if ((val = lookup_in(p, name)))
			return val;
	} else if (home) {
		snprintf(p, sizeof(p), "%s/.config/woltab", home);
		if ((val = lookup_in(p, name)))
			return val;
	}

	if (home) {
		snprintf(p, sizeof(p), "%s/.woltab", home);
		if ((val = lookup_in(p, name)))
			return val;
	}

	return lookup_in(SYSCONFDIR "/woltab", name);
}

int
main(int argc, char **argv)
{
	struct mac_addr mac;
	const char *s;

	if (argc != 2)
		errx(1, "usage: netwake <mac-addr>");

	if (mac_parse(argv[1], &mac) != -1)
		;
	else if (!(s = lookup(argv[1])))
		errx(1, "invalid MAC address and not in woltab: %s",
		    argv[1]);
	else if (mac_parse(s, &mac) == -1)
		errx(1, "invaid MAC in woltab: %s", s);

	if (wol_send(&mac) == -1)
		errx(1, "failed to send packet");

	printf("WOL packet sent!\n");
	return 0;
}
