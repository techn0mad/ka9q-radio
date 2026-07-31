# TODO

Open work on the portability branches. Items are grouped by where they belong,
because that determines who should receive them.

---

## Upstream candidates

These are plain bugs in the existing tree rather than portability additions.
None depends on the init-system or service-discovery work, and each is small
and independently justified, so they would go to KA9Q as their own pull request
off `main` rather than riding along with a larger branch. Splitting them out
also shrinks what the portability branches have to carry.

- [ ] **Driver `.so` rules omit `$(LDFLAGS)`** — `src/Makefile`, commit
  `8c135976`. Every plugin linked with `$(CC) $(SOFLAGS) -o $@ $^ -lairspy
  $(LDLIBS)`, leaving out the variable that carries `-L` search paths, while
  executables have always included it. Invisible on Debian, where vendor
  libraries land in `/usr/lib/<triplet>` and resolve without any `-L`; breaks
  as soon as the prefix moves, whether that is FreeBSD's `/usr/local`, MacPorts'
  `/opt/local`, or a Linux install under `/opt`, Spack or Nix. Also silently
  defeats `SANITIZE=1`, which adds `-fsanitize=...` to `LDFLAGS` and so never
  reached any driver plugin.

- [ ] **`avahi.h` includes seven unused Avahi headers** — `src/avahi.h`, commit
  `3c1e5860`. Nothing in the tree references an Avahi library type or symbol;
  `avahi.c` and `avahi_browse.c` shell out to the `avahi-*` command-line tools.
  The includes force Avahi development headers onto every consumer of the
  header for nothing, and they are unguarded, so any platform without them
  fails to compile. The same commit fixes a broken include guard that tested
  `_AVAHI_H` while defining `AVAHI_H`, so it never guarded anything.

- [ ] **Vestigial `<uuid/uuid.h>` in `radio.c`** — commit `6d3e7aad`. The sole
  uuid reference in the tree, with no `uuid_*` call anywhere and no target
  linking `-luuid`. Costs Linux builds a `uuid-dev` dependency that was never
  real. If accepted, `uuid-dev` can also come out of `docs/INSTALL.md`.

- [ ] **String functions declared only by accident** — 11 files, commit
  `ecaf310e`. `airspy.c`, `bladerf.c`, `fobos.c`, `funcube.c`, `misc.h` and six
  others call `strncpy`, `strcmp`, `strstr`, `memset` and `strlcpy` while
  getting `<string.h>` only transitively through `<bsd/string.h>`, which sits
  behind `#if defined(linux)`. Harmless on Debian, where that include is always
  taken, but the declarations are arriving by accident rather than by design.
  Anywhere the guard excludes it, the compiler falls back to implicit
  declarations and assumes `int` returns -- truncating every pointer-returning
  call to 32 bits on a 64-bit target. Adding `<string.h>` unconditionally is
  correct on every platform and costs Linux nothing.

- [ ] **Stale `#ifdef __FreeBSD__` libusb workaround** — `src/hid-libusb.c`,
  commit `7f69cc8d`. A local `static inline libusb_get_string_descriptor()`
  defined on the premise that "the FreeBSD version of libusb doesn't have this
  funciton". FreeBSD base has declared it at `lib/libusb/libusb.h:598` and
  implemented it in `lib/libusb/libusb10_desc.c` for years, so the workaround
  now collides with the system declaration and breaks the build on the one
  platform it was written to support.

---

## `src/` surface touched on this branch

44 files, +191/-67 against `main`. Almost all of it is one-line header changes;
the substance is in a handful of places. Recorded here so the upstream split
above can be carved out without re-deriving what each change was for.

| Change | Files | Size | Nature |
|--------|-------|------|--------|
| `src/Makefile` — `DARWIN_PREFIX`, `$(LDFLAGS)` on `.so` rules, FreeBSD arm | 1 | +37/-14 | build system |
| New shim headers `compat_net.h`, `compat_libusb.h`, `compat_xattr.h` | 3 | +88 | new files |
| `compat_net.h` adoption | 24 | 1 line each | mechanical |
| `compat_libusb.h` adoption | 4 | 1 line each | mechanical |
| `avahi.h` provider-neutral interface | 1 | +10/-9 | upstream candidate |
| `hid-libusb.c` stale FreeBSD block removed | 1 | +8/-21 | upstream candidate |
| `radio.c` unused uuid include removed | 1 | -1 | upstream candidate |
| `attr.c` alloca/xattr portability | 1 | +8/-4 | portability |
| `<string.h>` / `<strings.h>` declarations | 11 | 1-2 lines each | upstream candidate |

Detail on the two mechanical groups, since they account for most of the file
count:

- **`compat_net.h`** — 15 files had `#include <sys/socket.h>` swapped for it;
  9 more had it added because they use socket types without including the
  header at all. Glibc pulls `<netinet/in.h>` and `<arpa/inet.h>` in
  transitively, so on Linux `<sys/socket.h>` alone happens to suffice; FreeBSD
  does not, and `aprs.c` failed on an undeclared `IPPROTO_UDP`. The nine-file
  addition list came from the `cmake-freebsd-build` branch, which hit the same
  wall.

- **`compat_libusb.h`** — 4 files (`ezusb.h`, `hid-libusb.c`, `rx888.c`,
  `rx888_boot.c`). Linux and MacPorts install the header as
  `<libusb-1.0/libusb.h>`; FreeBSD ships libusb in base at `<libusb.h>`, with
  no subdirectory.

- **`compat_xattr.h`** — used only by `attr.c`, but it is the largest of the
  three shims because the three platforms genuinely differ. Linux has the
  native xattr API; macOS has `<sys/xattr.h>` but its `f*` variants take two
  extra arguments; FreeBSD has no xattr at all and uses `extattr(2)` with a
  separate namespace argument. The header normalizes all three to the
  Linux-style signature, so `attr.c`'s non-Linux branch dropped its trailing
  `0,0` arguments. Lifted verbatim from `cmake-freebsd-build`, which needed no
  adaptation -- unlike the other two, it never depended on `ka9q_config.h`.

  `attr.c` also had an unconditional `<alloca.h>`, which is glibc-only; FreeBSD
  declares `alloca()` in `<stdlib.h>`, already included there. Now guarded with
  `__has_include`. Note the pre-existing `#else // mainly OSX, probably BSD`
  branch was wrong for FreeBSD on both counts -- no such header, and no
  `fgetxattr` at all -- so "probably BSD" had never been true.

- **`<string.h>` declarations** — 11 driver and utility files got an
  unconditional `<string.h>`, and `bladerf.c` and `sdrplay.c` also `<strings.h>`.
  They had been relying on `<bsd/string.h>` to pull in `<string.h>`
  transitively, but that include sits behind `#if defined(linux)`, so on FreeBSD
  `strncpy`, `strcmp`, `strstr`, `memset` and `strlcpy` were all implicitly
  declared. This is listed as an upstream candidate rather than mere
  portability: an implicitly declared function is assumed to return `int`, so
  every pointer-returning call had its result truncated to 32 bits on any
  64-bit platform where `<bsd/string.h>` was not in play. The declarations are
  correct on Linux too; they were simply arriving by accident.

All three shims are independent of `ka9q_config.h`, so they work in the
Makefile build without a generated configuration header. `compat_net.h` and
`compat_libusb.h` were rewritten to drop that dependency; `compat_xattr.h`
never had it and was lifted verbatim. If the CMake work is ever merged with
this branch, the differing versions need reconciling.

Whether the shims themselves belong upstream is a separate question from the
four bugs above. They fix real latent portability problems, but nothing upstream
currently builds on a platform that trips them, so upstream may reasonably not
want the churn across 29 files.

---

## Portability work in progress

- [ ] **Finish the FreeBSD build.** Compiling is nearly there; linking is
  untested. `src/Makefile` links `-lusb-1.0`, and FreeBSD ships libusb in base
  where the library is `libusb.so` — if there is no `-1.0` alias, that needs a
  FreeBSD-specific `-lusb`.

- [ ] **Resolve the libfobos API divergence.** `src/fobos.c` calls
  `fobos_rx_close(dev)` with one argument. FreeBSD's `comms/libfobos` is 2.3.2,
  whose header declares `fobos_rx_close(struct fobos_dev_t *dev, int do_reset)`,
  so all six call sites fail to compile there. Ubuntu gets an older libfobos
  from KA9Q's own repository, which is why this has never surfaced. The driver
  is disabled in the FreeBSD CI job for now. There is no version macro in
  `fobos.h` to guard on, so resolving it properly means either pinning a
  libfobos version or detecting the arity at build time.

- [ ] **Add an OpenBSD CI job**, mirroring the FreeBSD one via
  `vmactions/openbsd-vm`. Expect a smaller driver set: `docs/PORTABILITY.md`
  records only `hackrf` and `rtl-sdr` among the vendor libraries in OpenBSD
  ports.

- [ ] **Update `docs/PORTABILITY.md` as CI proves cells.** The macOS and
  FreeBSD columns still read `pkg` (library packaged, build unverified). They
  may only move to `built` once the workflow actually compiles those drivers on
  that platform — that rule is the whole point of the table.

---

## Smaller items

- [ ] **`-fcx-limited-range` warning noise.** Recent clang reports
  `overriding '' option with '-fcx-limited-range'` on every compile, on both
  macOS and FreeBSD; GCC does not, which is why Debian never sees it. Benign,
  but it clutters the logs. Silencing means adding `-Wno-overriding-option` to
  the shared `COPTS`, so it is really a question about the whole
  `-funsafe-math-optimizations` / `-fno-trapping-math` group and belongs with
  whoever revisits those flags.

- [ ] **`table_compare` is an inconsistent comparator** — `src/mdns.c` on
  `portable-dns_sd`, copied verbatim from `src/avahi_browse.c` upstream. It
  returns `-1` for both `(a,b)` and `(b,a)` when both names are NULL, which is
  undefined behaviour for `qsort`. Only reachable on the post-de-dupe sort.
  Belongs upstream rather than as a divergence in a portability branch.

- [ ] **Consider a `user.useConfigOnly` note for contributors.** Not a code
  issue, but commits on these branches were briefly misattributed to a
  different account because git silently fell back to a global identity.
