#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sysexits.h>
#include <err.h>
#include "../common/wol.h"
#include "config.h"

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
	char *woltab, *home, *home_cfg, *val;

	if ((woltab = getenv("WOLTAB")) &&
	    (val = lookup_in(woltab, name)))
		return val;

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

static int
wake(const char *arg)
{
	const char *tabval;
	struct mac_addr mac;

	if (mac_parse(arg, &mac) != -1)
		;
	else if (!(tabval = lookup(arg))) {
		warnx("%s: invalid MAC address and not in woltab", arg);
		return -1;
	} else if (mac_parse(tabval, &mac) == -1) {
		warnx("%s: invaid MAC in woltab", tabval);
		return -1;
	}

	if (wol_send(&mac) == -1) {
		warnx("%s: failed to send packet", arg);
		return -1;
	}

	return 0;
}

int
main(int argc, char **argv)
{
	int i, nfail=0;

	if (argc < 2) {
		fprintf(stderr, "usage: netwake <mac-addr> ...\n");
		return EX_USAGE;
	}

	for (i=1; i < argc; i++)
		if (wake(argv[i]) == -1)
			nfail++;

	return nfail;
}
