typedef struct MacAddr {char bytes[6];} tMacAddr;

int ParseMacAddr(const char *str, tMacAddr *mac);
void FormatMacAddr(const tMacAddr *mac, char *str);
void SendWolPacket(const tMacAddr *mac);
