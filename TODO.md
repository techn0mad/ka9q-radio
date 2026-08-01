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

- [ ] **String functions declared only by accident** — 12 files, commits
  `ecaf310e` and `293fa1ea`. Eleven of them -- `airspy.c`, `bladerf.c`,
  `fobos.c`, `funcube.c`, `misc.h` and six others -- call `strncpy`, `strcmp`,
  `strstr`, `memset` and `strlcpy` while getting `<string.h>` only transitively
  through `<bsd/string.h>`, which sits behind `#if defined(linux)`. Harmless on
  Debian, where that include is always taken, but the declarations are arriving
  by accident rather than by design. Anywhere the guard excludes it, the
  compiler falls back to implicit declarations and assumes `int` returns --
  truncating every pointer-returning call to 32 bits on a 64-bit target. Adding
  `<string.h>` unconditionally is correct on every platform and costs Linux
  nothing.

  `rtlsdr.c` is the twelfth, found by auditing all of `src/` rather than only
  the files that had failed to compile. It is the purest case: it has no
  `#if defined(linux)` block of its own and gets `<string.h>` solely through
  `misc.h`. The same audit cleared `fft-gen.c`, `filter.c` and `osc.c`, which
  call `memset`/`memcpy`/`strerror` without `<string.h>` but include
  `<memory.h>`, which is `#include <string.h>` and nothing else; and `radio.h`,
  whose only `memcpy` is in a comment.

- [ ] **Stale `#ifdef __FreeBSD__` libusb workaround** — `src/hid-libusb.c`,
  commit `7f69cc8d`. A local `static inline libusb_get_string_descriptor()`
  defined on the premise that "the FreeBSD version of libusb doesn't have this
  funciton". FreeBSD base has declared it at `lib/libusb/libusb.h:598` and
  implemented it in `lib/libusb/libusb10_desc.c` for years, so the workaround
  now collides with the system declaration and breaks the build on the one
  platform it was written to support.

---

## `src/` surface touched on this branch

45 files, +202/-69 against ka9q's `main` tip (`fb4863db`). Almost all of it is
one-line header changes; the substance is in a handful of places. Recorded here
so the upstream split above can be carved out without re-deriving what each
change was for. Measured against ka9q rather than this fork's `main`, which is
stale and would add two of ka9q's own commits to the count.

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
| `<string.h>` / `<strings.h>` declarations | 12 | 1-2 lines each | upstream candidate |

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

- **`<string.h>` declarations** — 12 driver and utility files got an
  unconditional `<string.h>`, and `bladerf.c` and `sdrplay.c` also `<strings.h>`.
  Eleven had been relying on `<bsd/string.h>` to pull in `<string.h>`
  transitively, but that include sits behind `#if defined(linux)`, so on FreeBSD
  `strncpy`, `strcmp`, `strstr`, `memset` and `strlcpy` were all implicitly
  declared. This is listed as an upstream candidate rather than mere
  portability: an implicitly declared function is assumed to return `int`, so
  every pointer-returning call had its result truncated to 32 bits on any
  64-bit platform where `<bsd/string.h>` was not in play. The declarations are
  correct on Linux too; they were simply arriving by accident. The twelfth,
  `rtlsdr.c`, has no guard of its own and reached `<bsd/string.h>` only through
  `misc.h`.

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

- [x] **Finish the FreeBSD build.** Done — FreeBSD builds and installs cleanly
  in CI, with eight of the eleven drivers enabled (`fobos` and `hydrasdr`
  disabled for the version divergences above, `sdrplay` off everywhere).
  `docs/PORTABILITY.md`'s FreeBSD column has moved from `pkg` to `built`.

- [ ] **Resolve two vendor-library version divergences on FreeBSD.** Both
  drivers are packaged there, just not at versions this source matches, and
  both are disabled in the FreeBSD CI job for now. Ubuntu gets both from KA9Q's
  own repository at matching versions, which is why neither has surfaced
  before.

  - `libfobos` — the port is 2.3.2, whose `fobos_rx_close()` takes a second
    `do_reset` argument; `src/fobos.c` calls it with one at six sites. There is
    no version macro in `fobos.h` to guard on, so resolving it means either
    pinning a libfobos version or detecting the arity at build time.
  - `hydrasdr` — the port is 1.0.3, but `src/hydrasdr.c` requires 1.1.0 and
    says so itself (`MIN_LIB_VERSION HYDRASDR_MAKE_VERSION(1, 1, 0)`). It uses
    symbols absent from 1.0.3 — `hydrasdr_device_info_t`,
    `hydrasdr_get_device_info`, the `HYDRASDR_CAP_*` constants — so it cannot
    compile far enough to reach its own runtime version check. The cleanest fix
    is probably to guard that block on the library version, so older libraries
    build with reduced functionality rather than not at all.

- [ ] **Detect MacPorts or Homebrew on macOS rather than assuming MacPorts.**
  `src/Makefile` has `DARWIN_PREFIX ?= /opt/local` as a hook, but a prefix alone
  is not enough for Homebrew: its prefix varies by architecture
  (`/opt/homebrew` vs `/usr/local`, both reported by `brew --prefix`) and it
  keeps keg-only formulae outside that prefix entirely. `ncurses` is the
  blocker — keg-only, and macOS ships no `libncursesw` of its own, so
  `control`, `monitor`, `show-pkt` and `show-sig` fail to link without an
  explicit `-I`/`-L` for it. `libiconv` is also keg-only but macOS provides its
  own, so it likely resolves anyway.

  Shape: detect `/opt/local/bin/port` first, else `brew --prefix`, and add the
  keg-only pairs in the Homebrew case. Keep the two concerns separate — the
  Makefile should *detect and adapt*, never install a package manager; only the
  CI workflow should install MacPorts when neither is present, as it does now.
  Unverified and needing a CI round trip: whether Homebrew's `fftw` ships
  `libfftw3f_threads` under that name, and whether its `iniparser` matches what
  the build expects.

- [ ] **Decouple the per-platform build from the workflow.** The build commands
  are already plain `make` / `gmake`, but everything around them —
  dependency installation, the `ENABLE_*` flags, and the verification steps —
  lives as inline YAML, seven `run:` blocks across jobs of 59 to 82 lines. A
  developer cannot reproduce a CI run without transcribing it by hand.

  Shape: extract per-platform scripts (`platform/ci/deps-<os>.sh`,
  `platform/ci/verify-<os>.sh`, and the `ENABLE_*` set as data) and reduce each
  job to calling them, so the same scripts run locally. The
  `cmake-freebsd-build` branch already has a `scripts/setup.sh` worth looking at
  as prior art. Note the honest limit: this makes the *commands* identical, not
  the *environment* — runner images and package versions still differ, and the
  libfobos and hydrasdr divergences above are exactly that class of difference.

- [ ] **Add a smoke test after each build.** Linking successfully proves very
  little; none of the three platforms has ever run a single binary. Worth
  staging by cost and flakiness:

  1. *Binaries execute.* `radiod -V` prints the version and exits `EX_OK`
     (`src/main.c`, the `V` in its getopt string). Catches loader failures a
     successful link does not — missing dylib, wrong rpath, unresolved lazy
     symbol. Nearly free and not flaky; worth doing first and on every platform.
  2. *`radiod` starts and stops.* `config/examples/radiod@siggen.conf` drives
     the software signal generator, so no hardware is needed. Needs multicast
     on loopback (`aux/set_lo_multicast` exists for this) and `advertise=no` to
     avoid depending on a running mDNS daemon. Multicast behaviour differs
     across the three platforms, so expect this to be where the flakiness is.
  3. *A client observes data.* Point `powers` or `metadump` at the running
     instance and assert non-empty output. Highest value — it proves the DSP
     path works, not just that the process starts — and highest flakiness.

- [ ] **Add an OpenBSD CI job**, mirroring the FreeBSD one via
  `vmactions/openbsd-vm`. Expect a smaller driver set: `docs/PORTABILITY.md`
  records only `hackrf` and `rtl-sdr` among the vendor libraries in OpenBSD
  ports.

- [x] **Update `docs/PORTABILITY.md` as CI proves cells.** Done for the three
  platforms with CI: Linux (10 drivers), FreeBSD (8) and macOS (7) all read
  `built`. Only OpenBSD still shows `pkg`, and it has no CI job yet.

---

## Smaller items

- [ ] **`-fcx-limited-range` warning noise.** Recent clang reports
  `overriding '' option with '-fcx-limited-range'` on every compile, on both
  macOS and FreeBSD; GCC does not, which is why Debian never sees it. Benign,
  but it clutters the logs. Silencing means adding `-Wno-overriding-option` to
  the shared `COPTS`, so it is really a question about the whole
  `-funsafe-math-optimizations` / `-fno-trapping-math` group and belongs with
  whoever revisits those flags.

- [ ] **`iconv` prototype mismatch in `hid-libusb.c`.** FreeBSD's `iconv()`
  takes `char **inbuf`; the call passes a `const char **`, producing
  `-Wincompatible-pointer-types-discards-qualifiers`. A warning only, and the
  conversion is harmless in practice, but it is the kind of thing that becomes
  an error under a stricter compiler default.

- [ ] **`table_compare` is an inconsistent comparator** — `src/mdns.c` on
  `portable-dns_sd`, copied verbatim from `src/avahi_browse.c` upstream. It
  returns `-1` for both `(a,b)` and `(b,a)` when both names are NULL, which is
  undefined behaviour for `qsort`. Only reachable on the post-de-dupe sort.
  Belongs upstream rather than as a divergence in a portability branch.

- [ ] **Consider a `user.useConfigOnly` note for contributors.** Not a code
  issue, but commits on these branches were briefly misattributed to a
  different account because git silently fell back to a global identity.
