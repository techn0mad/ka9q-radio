/*
 * mdns_browse.c
 *
 * DNS-SD (dns_sd.h) based replacement for avahi_browse.c
 *
 * Implements synchronous browse + resolve and fills struct service_tab[]
 * similarly to parsing avahi-browse output.
 *
 * Platforms:
 *   macOS    : native mDNSResponder
 *   FreeBSD  : net/mDNSResponder
 *   Linux    : avahi-compat-libdns_sd
 */

#include "mdns.h"

#include <dns_sd.h>

#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/time.h>
#include <unistd.h>

/* ---------- helpers ---------- */

static int wait_and_process(DNSServiceRef ref, int timeout_ms) {
  int fd = DNSServiceRefSockFD(ref);
  if (fd < 0)
    return -1;

  fd_set rfds;
  FD_ZERO(&rfds);
  FD_SET(fd, &rfds);

  struct timeval tv;
  tv.tv_sec  = timeout_ms / 1000;
  tv.tv_usec = (timeout_ms % 1000) * 1000;

  int r = select(fd + 1, &rfds, NULL, NULL, &tv);
  if (r < 0)
    return -1;
  if (r == 0)
    return 0;

  if (FD_ISSET(fd, &rfds)) {
    DNSServiceErrorType e = DNSServiceProcessResult(ref);
    return (e == kDNSServiceErr_NoError) ? 1 : -1;
  }
  return 0;
}

static char *txt_to_string(uint16_t txtLen, const unsigned char *txt) {
  if (!txt || txtLen == 0)
    return strdup("");

  size_t cap = txtLen * 2 + 1;
  char *out = calloc(1, cap);
  if (!out)
    return NULL;

  size_t i = 0, w = 0;
  int first = 1;

  while (i < txtLen) {
    uint8_t l = txt[i++];
    if (i + l > txtLen)
      break;

    if (!first && w + 1 < cap)
      out[w++] = ' ';
    first = 0;

    for (uint8_t j = 0; j < l && w + 1 < cap; j++) {
      unsigned char c = txt[i + j];
      if (c < 32 || c > 126)
        c = '.';
      out[w++] = (char)c;
    }
    i += l;
  }
  out[w] = '\0';
  return out;
}

/* ---------- browse state ---------- */

struct browse_ctx {
  struct service_tab *table;
  int tabsize;
  int count;
};

struct resolve_ctx {
  struct browse_ctx *bctx;
  char name[256];
  char type[128];
  char domain[64];
};

/* ---------- resolve callback ---------- */

static void DNSSD_API resolve_cb(
    DNSServiceRef sdRef,
    DNSServiceFlags flags,
    uint32_t ifIndex,
    DNSServiceErrorType errorCode,
    const char *fullname,
    const char *hosttarget,
    uint16_t port,
    uint16_t txtLen,
    const unsigned char *txt,
    void *context) {

  (void)sdRef; (void)flags; (void)ifIndex; (void)fullname;

  struct resolve_ctx *rctx = context;
  struct browse_ctx *bctx = rctx->bctx;

  if (errorCode != kDNSServiceErr_NoError)
    return;
  if (bctx->count >= bctx->tabsize)
    return;

  struct service_tab *ent = &bctx->table[bctx->count];

  /* resolve hostname -> IPv4 */
  char addrbuf[INET_ADDRSTRLEN] = "";
  struct addrinfo hints, *res = NULL;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;

  if (getaddrinfo(hosttarget, NULL, &hints, &res) == 0 && res) {
    struct sockaddr_in *sin = (struct sockaddr_in *)res->ai_addr;
    inet_ntop(AF_INET, &sin->sin_addr, addrbuf, sizeof(addrbuf));
    freeaddrinfo(res);
  }

  char portbuf[16];
  snprintf(portbuf, sizeof(portbuf), "%u", ntohs(port));

  char *txtstr = txt_to_string(txtLen, txt);

  /* allocate one backing buffer (same style as avahi_browse.c) */
  size_t buflen =
      strlen(rctx->name) + strlen(rctx->type) + strlen(rctx->domain) +
      strlen(hosttarget) + strlen(addrbuf) + strlen(portbuf) +
      (txtstr ? strlen(txtstr) : 0) + 64;

  char *buf = calloc(1, buflen);
  char *p = buf;

  ent->buffer   = buf;
  ent->line_type = "=";

  ent->interface = p; strcpy(p, "0"); p += 2;
  ent->protocol  = p; strcpy(p, "IPv4"); p += 5;

  ent->name   = p; strcpy(p, rctx->name);   p += strlen(p) + 1;
  ent->type   = p; strcpy(p, rctx->type);   p += strlen(p) + 1;
  ent->domain = p; strcpy(p, rctx->domain); p += strlen(p) + 1;

  ent->dns_name = p; strcpy(p, hosttarget); p += strlen(p) + 1;
  ent->address  = p; strcpy(p, addrbuf);    p += strlen(p) + 1;
  ent->port     = p; strcpy(p, portbuf);    p += strlen(p) + 1;
  ent->txt      = p; strcpy(p, txtstr ? txtstr : "");

  free(txtstr);
  bctx->count++;
}

/* ---------- browse callback ---------- */

static void DNSSD_API browse_cb(
    DNSServiceRef sdRef,
    DNSServiceFlags flags,
    uint32_t ifIndex,
    DNSServiceErrorType errorCode,
    const char *name,
    const char *type,
    const char *domain,
    void *context) {

  (void)sdRef; (void)ifIndex;

  struct browse_ctx *bctx = context;

  if (errorCode != kDNSServiceErr_NoError)
    return;
  if (!(flags & kDNSServiceFlagsAdd))
    return;
  if (bctx->count >= bctx->tabsize)
    return;

  DNSServiceRef resolveRef = NULL;
  struct resolve_ctx rctx;

  memset(&rctx, 0, sizeof(rctx));
  rctx.bctx = bctx;
  strncpy(rctx.name, name, sizeof(rctx.name) - 1);
  strncpy(rctx.type, type, sizeof(rctx.type) - 1);
  strncpy(rctx.domain, domain, sizeof(rctx.domain) - 1);

  if (DNSServiceResolve(&resolveRef,
                         0,
                         ifIndex,
                         name,
                         type,
                         domain,
                         resolve_cb,
                         &rctx) != kDNSServiceErr_NoError)
    return;

  /* wait briefly for resolve */
  for (int i = 0; i < 10; i++) {
    int r = wait_and_process(resolveRef, 100);
    if (r != 0)
      break;
  }

  DNSServiceRefDeallocate(resolveRef);
}

/* ---------- public API ---------- */

int mdns_browse(struct service_tab *table, int tabsize, char const *service_name) {
  if (!table || tabsize <= 0 || !service_name)
    return 0;

  memset(table, 0, sizeof(*table) * tabsize);

  struct browse_ctx ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.table = table;
  ctx.tabsize = tabsize;

  DNSServiceRef browseRef = NULL;
  if (DNSServiceBrowse(&browseRef,
                       0,
                       0,
                       service_name,
                       NULL,
                       browse_cb,
                       &ctx) != kDNSServiceErr_NoError)
    return 0;

  /* collect results for ~1 second total */
  for (int i = 0; i < 10; i++)
    wait_and_process(browseRef, 100);

  DNSServiceRefDeallocate(browseRef);
  return ctx.count;
}

void mdns_free_service_table(struct service_tab *table, int tabsize) {
  if (!table)
    return;
  for (int i = 0; i < tabsize; i++) {
    free(table[i].buffer);
    memset(&table[i], 0, sizeof(table[i]));
  }
}

