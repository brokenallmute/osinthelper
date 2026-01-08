#ifndef IPINFO_H
#define IPINFO_H

typedef struct {
    char ip[46];
    char hostname[256];
    char city[128];
    char region[128];
    char country[64];
    char loc[64];
    char org[256];
    char postal[32];
    char timezone[64];
} ip_info_t;

int ipinfo_lookup(const char *ip, ip_info_t *info);
void ipinfo_print(const ip_info_t *info);

#endif
