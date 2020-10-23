#include "wol.h"
#include "util.h"

#ifdef _WIN32
# include <winsock2.h>
#endif

typedef struct WolPacket {char zeroes[6]; tMacAddr macs[16];} tWolPacket;

int
ParseMacAddr(const char *str, tMacAddr *mac)
{
	int inPos, outPos=0;
	char c;

	ZeroMemory(mac, sizeof(*mac));

	for (inPos=0; str[inPos]; inPos++) {
		if (outPos >= (int)LEN(mac->bytes)*2)
			return -1;
		else if ((c = str[inPos]) == ':' || c == ' ')
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

void
FormatMacAddr(const tMacAddr *mac, char *str)
{
	int i;

	for (i=0; i < (int)LEN(mac->bytes); i++) {
		str[i*3+0] = "0123456789abcdef"[(mac->bytes[i] & 0xF0) >> 4];
		str[i*3+1] = "0123456789abcdef"[(mac->bytes[i] & 0x0F)];
		str[i*3+2] = ':';
	}

	str[i*3-1] = '\0';
}

void
SendWolPacket(const tMacAddr *mac)
{
	int i;
	tWolPacket wol;
	struct sockaddr_in addr;
	SOCKET sock;

	memset(&wol, 0, sizeof(wol));
	for (i=0; i < (int)LEN(wol.zeroes); i++)
		wol.zeroes[i] = (char)0xFF;
	for (i=0; i < (int)LEN(wol.macs); i++)
		wol.macs[i] = *mac;

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = 9;
	addr.sin_addr.S_un.S_addr = 0xFFFFFFFF;

	if ((sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == INVALID_SOCKET)
		err(1, "socket() failed");
	if (connect(sock, (struct sockaddr *)&addr, sizeof(addr))
	    == SOCKET_ERROR)
		err(1, "connect() failed");
	if (send(sock, (void *)&wol, sizeof(wol), 0) == SOCKET_ERROR)
		err(1, "send() failed");

	closesocket(sock);
}
