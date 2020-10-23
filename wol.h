typedef struct MacAddr {char bytes[6];} tMacAddr;

int ParseMacAddr(const char *str, tMacAddr *mac);
void SendWolPacket(const tMacAddr *mac);
