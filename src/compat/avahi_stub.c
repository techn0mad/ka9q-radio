/* avahi_stub.c — no-op mDNS stubs for platforms without avahi-client.
 * Used on macOS and FreeBSD until Phase 3 replaces these with dns_sd.h. */
#ifndef HAVE_AVAHI_CLIENT
#include <stddef.h>
#include <stdlib.h>
#include <stdbool.h>
#include "avahi.h"

bool Static_avahi = false;

int avahi_start(char const *service_name, char const *service_type,
                int service_port, char const *dns_name,
                int base_address, char const *description) {
    (void)service_name; (void)service_type; (void)service_port;
    (void)dns_name; (void)base_address; (void)description;
    return 0;
}

int avahi_browse(struct service_tab *table, int tabsize,
                 char const *service_name) {
    (void)table; (void)tabsize; (void)service_name;
    return 0;
}

void avahi_free_service_table(struct service_tab *table, int tabsize) {
    (void)table; (void)tabsize;
}
#endif /* !HAVE_AVAHI_CLIENT */
