struct mac_addr { char bytes[6]; };

int mac_parse(const char *str, struct mac_addr *mac);
void mac_fmt(const struct mac_addr *mac, char *str);
int wol_send(const struct mac_addr *mac);
