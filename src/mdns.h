#pragma once
/*
 * mdns.h - minimal, implementation-neutral mDNS/DNS-SD interface for ka9q-radio
 *
 * This replaces the old avahi.h (Avahi-native) interface and is intended to be
 * implemented using the DNS-SD API (dns_sd.h):
 *   - macOS: system mDNSResponder
 *   - FreeBSD: net/mDNSResponder
 *   - Linux: avahi-compat-libdns_sd (provides libdns_sd + dns_sd.h)
 */

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Keep the old "service_tab" shape because other code may already consume it.
 * All pointers (except buffer) point into buffer; free with mdns_free_service_table().
 */
struct service_tab {
  char *buffer;     /* owning buffer; free()'d by mdns_free_service_table() */
  char *line_type;  /* "=" for a resolved service record (mirrors avahi-browse output convention) */
  char *interface;  /* interface name or index as string */
  char *protocol;   /* "IPv4" (this implementation currently emits IPv4 only) */
  char *name;       /* instance name */
  char *type;       /* regtype, e.g. "_ka9q-ctl._udp" */
  char *domain;     /* usually "local" */
  char *dns_name;   /* resolved host target (FQDN-ish) */
  char *address;    /* numeric IPv4 address */
  char *port;       /* numeric port as string */
  char *txt;        /* txt as a single string (best-effort) */
};

/* Compatibility knob used by old avahi.c. You can delete once callers are migrated. */
extern bool Static_avahi;

/*
 * Synchronously browse and resolve services.
 *
 * service_name is the DNS-SD regtype (same string you used with avahi-browse),
 * e.g. "_ka9q-ctl._udp" or "_ka9q-radio._tcp".
 *
 * Returns number of table entries filled (0 on none/error).
 */
int mdns_browse(struct service_tab *table, int tabsize, char const *service_name);

/* Free buffers allocated by mdns_browse() */
void mdns_free_service_table(struct service_tab *table, int tabsize);

/*
 * Publish a service (and optionally return a sockaddr in *sock).
 *
 * service_name: instance name
 * service_type: regtype (e.g. "_ka9q-ctl._udp")
 * service_port: port number
 * dns_name: host name to advertise (NULL/"" to let daemon choose)
 * address: IPv4 address as host-order uint32 (0 to not publish address)
 * description: optional TXT-ish description (may be NULL)
 *
 * Returns 0 on success, -1 on error.
 */
int mdns_start(char const *service_name,
               char const *service_type,
               int const service_port,
               char const *dns_name,
               int address,
               char const *description,
               void *sock,
               size_t *socksize);

/* -----------------------------------------------------------------------
 * Transitional compatibility shims (optional but convenient during refactor)
 * --------------------------------------------------------------------- */
#define avahi_browse mdns_browse
#define avahi_free_service_table mdns_free_service_table
#define avahi_start mdns_start

#ifdef __cplusplus
} /* extern "C" */
#endif

