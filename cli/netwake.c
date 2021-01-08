#include <stdio.h>
#include "../common/wol.h"

int
main(int argc, char **argv)
{
	struct mac_addr mac;

	if (argc != 2)
		fputs("usage: netwake <mac-addr>\n", stderr);
	else if (mac_parse(argv[1], &mac) == -1)
		fputs("netwake: invalid MAC address\n", stderr);
	else if (wol_send(&mac) == -1)
		fputs("netwake: failed tom send packet\n", stderr);
	else {
		puts("WOL packet sent");
		return 0;
	}

	return 1;
}
