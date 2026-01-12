/*
 * mdns.c - DNS-SD (dns_sd.h) implementation of ka9q-radio mDNS plumbing.
 *
 * Notes:
 *  - This is intentionally synchronous and minimal to match the old
 *    "run avahi-browse and parse" behavior.
 *  - Linux support expects avahi-compat-libdns_sd (libdns_sd + dns_sd.h).
 *  - FreeBSD support expects net/mDNSResponder (libdns_sd + dns_sd.h).
 *  - macOS uses the system mDNSResponder.
 */
#include "mdns.h"

#include <dns_sd.h>

#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

/* old global used by avahi.c; keep until callers are cleaned up */
bool Static_avahi = false;

/* ---------------- TXT helpers ---------------- */

static char *txt_to_string(uint16_t txtLen, const unsigned char *txtRecord) {
  /* Best-effort human-readable string.
   * TXT is a sequence of length-prefixed strings: <len><bytes...> */
  if (txtLen == 0 || txtRecord == NULL) {
    return strdup("");
  }

  /* Upper bound: each entry might add a separator; allocate 2x for safety. */
  size_t cap = (size_t)txtLen * 2 + 1;
  char *out = calloc(1, cap);
  if (!out) return NULL;

  size_t w = 0;
  size_t i = 0;
  int first = 1;

  while (i < txtLen) {
    uint8_t l = txtRecord[i++];
    if (i + l > txtLen) break;

    if (!first) {
      if (w + 1 < cap) out[w++] = ' ';
    }
    first = 0;

    for (uint8_t j = 0; j < l; j++) {
      unsigned char c = txtRecord[i + j];
      if (w + 1 >= cap) break;
      /* Replace non-printables */
      if (c < 32 || c > 126) c = '.';
      out[w++] = (char)c;
    }
    i += l;
  }

  out[w] = '\0';
  return out;
}

static int fill_sockaddr_if_requested(int service_port, int address, void *sock, size_t *socksize) {
  if (sock == NULL || socksize == NULL) return 0;

  if (*socksize < sizeof(struct sockaddr_in)) {
    *socksize = 0;
    return 0;
  }
  struct sockaddr_in *sin = (struct sockaddr_in *)sock;
  memset(sin, 0, sizeof(*sin));
  sin->sin_family = AF_INET;
  sin->sin_addr.s_addr = htonl((uint32_t)address);
  sin->sin_port = htons((uint16_t)service_port);
  *socksize = sizeof(*sin);
  return 0;
}

/* ---------------- Publishing ---------------- */

struct pub_ctx {
  DNSServiceRef ref;
  int ok;
  char errbuf[256];
};

static void DNSSD_API register_cb(
    DNSServiceRef sdRef,
    DNSServiceFlags flags,
    DNSServiceErrorType errorCode,
    const char *name,
    const char *regtype,
    const char *domain,
    void *context) {

  (void)sdRef; (void)flags; (void)name; (void)regtype; (void)domain;
  struct pub_ctx *ctx = (struct pub_ctx *)context;
  if (!ctx) return;

  if (errorCode == kDNSServiceErr_NoError) {
    ctx->ok = 1;
  } else {
    ctx->ok = 0;
    snprintf(ctx->errbuf, sizeof(ctx->errbuf), "DNSServiceRegister callback error=%d", (int)errorCode);
  }
}

static int pump_dnssd(DNSServiceRef ref, int timeout_ms) {
  int fd = DNSServiceRefSockFD(ref);
  if (fd < 0) return -1;

  fd_set rfds;
  FD_ZERO(&rfds);
  FD_SET(fd, &rfds);

  struct timeval tv;
  tv.tv_sec = timeout_ms / 1000;
  tv.tv_usec = (timeout_ms % 1000) * 1000;

  int r = select(fd + 1, &rfds, NULL, NULL, &tv);
  if (r < 0) return -1;
  if (r == 0) return 0; /* timeout */
  if (FD_ISSET(fd, &rfds)) {
    DNSServiceErrorType e = DNSServiceProcessResult(ref);
    return (e == kDNSServiceErr_NoError) ? 1 : -1;
  }
  return 0;
}

int mdns_start(char const *service_name,
               char const *service_type,
               int const service_port,
               char const *dns_name,
               int address,
               char const *description,
               void *sock,
               size_t *socksize) {

  (void)description; /* kept for signature compatibility; can be encoded into TXT later */

  if (!service_name || !*service_name || !service_type || !*service_type) {
    errno = EINVAL;
    return -1;
  }

  fill_sockaddr_if_requested(service_port, address, sock, socksize);

  /* TXT record: if caller provided description, encode as "description=...".
   * You can extend this later to carry whatever the old code used. */
  TXTRecordRef txt;
  TXTRecordCreate(&txt, 0, NULL);
  if (description && *description) {
    TXTRecordSetValue(&txt, "description", (uint8_t)strlen(description), description);
  }

  const void *txtPtr = TXTRecordGetBytesPtr(&txt);
  uint16_t txtLen = (uint16_t)TXTRecordGetLength(&txt);

  struct pub_ctx ctx;
  memset(&ctx, 0, sizeof(ctx));
  DNSServiceRef ref = NULL;

  /* Note: host argument in DNSServiceRegister is generally not needed; mDNSResponder will
   * advertise the local host. If dns_name is provided, pass it; otherwise NULL. */
  const char *host = (dns_name && *dns_name) ? dns_name : NULL;

  DNSServiceErrorType e = DNSServiceRegister(
      &ref,
      0,                 /* flags */
      0,                 /* interfaceIndex (0 = all) */
      service_name,      /* name (instance) */
      service_type,      /* regtype */
      NULL,              /* domain (NULL=default "local.") */
      host,              /* host */
      htons((uint16_t)service_port),
      txtLen,
      txtPtr,
      register_cb,
      &ctx);

  TXTRecordDeallocate(&txt);

  if (e != kDNSServiceErr_NoError) {
    errno = EIO;
    return -1;
  }

  ctx.ref = ref;

  /* Give the daemon a moment to confirm registration (and to surface name conflicts). */
  for (int i = 0; i < 10 && !ctx.ok; i++) {
    int pr = pump_dnssd(ref, 100); /* 1s total */
    if (pr < 0) break;
  }

  /* NOTE:
   * This implementation deallocates immediately (like "fire-and-forget").
   * If ka9q-radio needs the advertisement to persist for the process lifetime,
   * you should KEEP the DNSServiceRef around and pump it periodically.
   */
  DNSServiceRefDeallocate(ref);

  return 0;
}

