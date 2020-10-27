#include <stdint.h>
#include "wol.h"

#ifdef _WIN32
# include <winsock.h>
# define close(x)	closesocket(x)
#else
# include <string.h>
# include <unistd.h>
# include <sys/types.h>
# include <sys/socket.h>
# include <netinet/in.h>
# include <netinet/udp.h>
# define INVALID_SOCKET	-1
# define SOCKET_ERROR	-1
typedef int SOCKET;
#endif

#define LEN(a) (sizeof(a)/sizeof(*(a)))

typedef struct WolPacket {char zeroes[6]; tMacAddr macs[16];} tWolPacket;

int
ParseMacAddr(const char *str, tMacAddr *mac)
{
	int inPos, outPos=0;
	char c;

	memset(mac, 0, sizeof(*mac));

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

int
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
	*(uint32_t *)&addr.sin_addr = 0xFFFFFFFF;

	if ((sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == INVALID_SOCKET)
		return -1;
	if (connect(sock, (struct sockaddr *)&addr, sizeof(addr))
	    == SOCKET_ERROR)
		return -1;
	if (send(sock, (void *)&wol, sizeof(wol), 0) == SOCKET_ERROR)
		return -1;

	close(sock);

	return 0;
}
