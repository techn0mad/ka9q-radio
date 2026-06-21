# ka9q-radio Portability Project Plan

Cross-platform port of ka9q-radio to Ubuntu, macOS, and FreeBSD, with CI/CD,
testing, and architectural improvements. Goals are numbered 1–11 as defined in
the project brief; phases are ordered to deliver working builds early and keep
upstream-sync risk low throughout.

---

## Phase overview

| Phase | Goals | Theme | Upstream risk |
|-------|-------|-------|---------------|
| 1 | 1,2,3,10 | Complete the CMake build — all three platforms compile | Low |
| 2 | 9,11 | CI/CD pipelines for all three platforms | None |
| 3 | 4,10 | Native mDNS via `dns_sd.h` (Bonjour compatibility) | Low |
| 4 | 8-L1 | Unit tests for pure DSP functions | None |
| 5 | 5 | Repo structure — separate build/CI/packaging from source | Low |
| 6 | 6,7 | Data/control plane separation + structured logging | Medium |
| 7 | 8-L2,L3,11 | Integration and system tests using `sig_gen` | Low |

Goals 10 and 11 are cross-cutting — they shape decisions in every phase
rather than belonging to a single phase:

- **Goal 10** (minimize dependencies; prefer native platform facilities) — see
  "Dependency minimization" under Cross-cutting concerns.
- **Goal 11** (builds and tests work identically on developer machines and in
  CI) — see "Local development parity" under Cross-cutting concerns.

Phases 1 and 2 are the critical path. Everything else depends on having a
clean, verified build on all three platforms before deeper changes begin.

---

## Phase 1 — Complete the CMake build (Goals 1, 2, 3)

**Theme:** Get `cmake --build` to produce a full, correct set of binaries on
Ubuntu, macOS, and FreeBSD. No source-code changes beyond minimal platform
guards. This phase keeps the upstream `src/Makefile` as-is and adds our
`CMakeLists.txt` alongside it.

### Tasks

#### 1.1 Add missing `add_executable()` targets

The upstream `src/Makefile` defines these targets; `CMakeLists.txt` currently
defines none of them:

**Daemons** (install to `sbin`):
- `radiod` — `main.o audio.o fm.o wfm.o linear.o spectrum.o radio.o radio_status.o rtcp.o fcd.o hid-libusb.o` + `libradio.a`
- `aprs` — `aprs.o` + `libradio.a`
- `aprsfeed` — `aprsfeed.o` + `libradio.a`
- `cwd` — `cwd.o` + `libradio.a`
- `packetd` — `packetd.o` + `libradio.a`

**User tools** (install to `bin`):
- `monitor` — `monitor.o monitor-data.o monitor-display.o monitor-repeater.o` + `libradio.a`
- `control` — `control.o` + `libradio.a` + iniparser + ncurses
- `metadump` — `metadump.o` + `libradio.a`
- `pcmrecord` — `pcmrecord.o` + `libradio.a`
- `opussend` — `opussend.o` + `libradio.a`
- `jt-decoded` — `jt-decoded.o` + `libradio.a`
- `tune`, `pl`, `powers`, `fft-gen`, `wd-record` — already present in CMakeLists.txt; verify they link correctly
- Secondary tools (no install by default): `setfilt`, `show-pkt`, `show-sig`, `opusd`, `pcmcat`, `pcmspawn`, `pcmsend`, `rdsd`, `stereod`, `sig-gen`

**Dynamic driver plugins** (`.so`/`.dylib`, loaded via `dlopen`):
- `sig_gen.so` — always built (no hardware dep; used for testing)
- `airspy.so`, `airspyhf.so`, `rtlsdr.so`, `hackrf.so` — conditional on library detection
- `rx888.so`, `funcube.so` — conditional on libusb + portaudio
- `sdrplay.so`, `fobos.so`, `hydrasdr.so`, `bladerf.so` — optional, off by default

#### 1.2 Fix `libbsd` platform guard

`pkg_check_modules(BSD REQUIRED libbsd)` currently fails on FreeBSD and macOS
because `libbsd` is a Linux-only compatibility shim. BSD functions are native
on FreeBSD; macOS has most of them in `<string.h>` / `<stdlib.h>`.

Change:
```cmake
if(LINUX)
    pkg_check_modules(BSD REQUIRED libbsd)
    target_link_libraries(radio PUBLIC ${BSD_LIBRARIES})
    target_include_directories(radio PUBLIC ${BSD_INCLUDE_DIRS})
endif()
```

The `#if defined(linux) / #include <bsd/string.h>` guards already in the
source files handle the include side correctly.

#### 1.3 Add FreeBSD `#elif` path in `multicast.c`

`multicast.c` already has `#if defined(linux)` guards around the Linux-specific
includes (`<linux/if_packet.h>`, `<linux/capability.h>`), but there is no
FreeBSD code path below them. Add:
```c
#elif defined(__FreeBSD__)
#include <net/if_dl.h>
#include <net/bpf.h>
```
and corresponding FreeBSD equivalents for the raw-socket/capability sections
inside the function bodies (lines 24–32 and 609+). This is the only source file
requiring a `#elif __FreeBSD__` addition in Phase 1.

#### 1.4 Fix FreeBSD rc.d template path

`CMakeLists.txt` references `freebsd/radiod.in` but the file is at
`freebsd-radiod.in` (top level). Either rename the file to
`freebsd/radiod.in` (better organization, Phase 5 territory) or fix the
CMakeLists.txt reference to match.

#### 1.5 Handle `arc4random` portability

`arc4random` and `arc4random_uniform` are used in `control.c`, `metadump.c`,
`sig_gen.c`, and `tune.c`. They are native on macOS and FreeBSD. On Linux the
situation is version-dependent:

- glibc ≥ 2.36 (Ubuntu 24.04+): available natively, no shim needed
- glibc < 2.36 (Ubuntu 22.04 / jammy, glibc 2.35): requires `libbsd`

Strategy: CMake checks for `arc4random` availability with
`check_function_exists(arc4random HAVE_ARC4RANDOM)`. If not found, add a
small `src/compat/arc4random.c` implementing it via `getrandom(2)` (Linux
3.17+, FreeBSD 12+) — a two-function shim, ~30 lines. This eliminates the
`libbsd` dependency on Linux entirely while maintaining compatibility with
Ubuntu 22.04 LTS.

The `#if defined(linux) / #include <bsd/string.h>` guards in the source
already handle `strlcpy`/`strlcat` — those are in glibc 2.38+ (Ubuntu 24.04)
or macOS/FreeBSD natively. Same CMake detection approach: if not available,
provide `src/compat/strlcpy.c` (20 lines, standard implementation).

#### 1.6 Add FreeBSD pkg dependency list

`CMakeLists.txt` has a placeholder `CPACK_FREEBSD_PACKAGE_DEPS` that needs
the actual FreeBSD port names:
```
fftw3-float, opus, iniparser, avahi-app (or mDNSResponder), ncurses, portaudio
```
Note: `libbsd` is not listed — it is not needed on FreeBSD.

#### 1.7 Verify builds

- Ubuntu: run existing CI locally (`cmake -B build && cmake --build build`)
- macOS: same, confirm Homebrew paths resolve
- FreeBSD: build in a jail or VM; document any remaining missing pkg names

### Success criteria

- `cmake --build` succeeds on all three platforms
- All daemons, user tools, and `sig_gen.so` are present in the build output
- `libbsd` is not required on macOS or FreeBSD
- `arc4random` resolves on all three platforms without `libbsd`
- No upstream `src/*.c` or `src/*.h` files modified except `multicast.c` (one
  `#elif` block) and any `compat/` shims added alongside, not within, upstream files

### Upstream sync risk: Low

Only `multicast.c` is touched in upstream source. The `src/compat/` shim files
are additive. If upstream changes `multicast.c`, the merge conflict is isolated
and obvious.

---

## Phase 2 — CI/CD pipelines (Goal 9)

**Theme:** Every push and PR gets build-and-test feedback on all three
platforms. FreeBSD runs via `vmactions/freebsd-vm`
(https://github.com/vmactions/freebsd-vm), a GitHub Action maintained by
Neil Pang (`neilpang`, author of `acme.sh`) that boots a FreeBSD VM in QEMU
on a standard Ubuntu runner. Ubuntu and macOS use GitHub Actions natively.
A tagged release automatically publishes platform packages as GitHub Release
assets.

`vmactions/freebsd-vm` was chosen over the alternative
`cross-platform-actions/action` (https://github.com/cross-platform-actions/action)
based on project health indicators as of June 2026: 346 vs 191 stars,
6 vs 36 open issues, and a systematic template-based release process.

**Important constraint:** Both QEMU-based approaches use NAT networking inside
the VM. Loopback multicast (needed for Phase 7 integration tests) is unlikely
to work reliably. Phase 7 will need to revisit FreeBSD CI options — either a
self-hosted native FreeBSD runner (if GitHub runner support has landed by then)
or a third-party CI system with native FreeBSD VMs.

### Tasks

#### 2.1 Upgrade GitHub Actions workflow

Replace the current `make`-only jobs with CMake + CTest jobs:

```yaml
- name: Configure
  run: cmake -B build -DCMAKE_BUILD_TYPE=Release
- name: Build
  run: cmake --build build --parallel
- name: Test
  run: ctest --test-dir build --output-on-failure
- name: Package
  run: cd build && cpack
```

Keep dependency install steps; remove the Avahi-from-source build on macOS
(that moves to Phase 3 when native dns_sd replaces it).

#### 2.2 Add FreeBSD job via `vmactions/freebsd-vm`

Add a new job to `.github/workflows/ci.yml` — no separate CI config file
needed:

```yaml
freebsd-build:
  name: FreeBSD CI
  runs-on: ubuntu-latest
  steps:
    - uses: actions/checkout@v4
    - uses: vmactions/freebsd-vm@v1
      with:
        release: '14.2'
        usesh: true
        prepare: |
          pkg install -y cmake pkgconf fftw3 opus iniparser \
            avahi-app ncurses portaudio libusb airspy airspyhf \
            rtl-sdr hackrf
        run: |
          cmake -B build -DCMAKE_BUILD_TYPE=Release
          cmake --build build --parallel
          ctest --test-dir build --output-on-failure -L unit
          cd build && cpack -G TXZ
```

The `-L unit` flag on `ctest` runs only the unit-test label (Phase 4), which
has no multicast dependency. Integration tests (Phase 7) are excluded here
pending a solution to the NAT networking constraint.

#### 2.3 Publish release artifacts on tag

Add a `release` workflow triggered by `push: tags: ['v*']` that:
1. Runs all three platform builds
2. Uploads `.deb` (Ubuntu), `.tar.gz` (macOS), `.txz` (FreeBSD) as GitHub
   Release assets using `actions/upload-release-asset`

#### 2.4 Make all three jobs required status checks

In GitHub repo Settings → Branches → Branch protection rules: require
`ubuntu-build`, `macos-build`, and `freebsd-build` to pass before merge.

### Success criteria

- PR status shows three green checks (Ubuntu, macOS, FreeBSD)
- Tag push produces a GitHub Release with three downloadable packages
- A build failure on any platform blocks merge

### Upstream sync risk: None

CI configuration files are not part of the upstream source tree.

---

## Phase 3 — Native mDNS via `dns_sd.h` (Goal 4)

**Theme:** Replace the current approach (forking `avahi-browse`/`avahi-publish`
subprocesses, writing to `/etc/avahi/` files) with the `dns_sd.h` API —
Apple's Bonjour/mDNSResponder interface that Avahi implements as a
compatibility layer. This gives native mDNS on macOS without installing Avahi,
and a uniform API across all three platforms.

### Background

| Platform | Provider | Library/framework |
|----------|----------|-------------------|
| macOS | System mDNSResponder | Built-in; `#include <dns_sd.h>` |
| Linux | `avahi-compat-libdns_sd` | `libavahi-compat-libdns_sd-dev` |
| FreeBSD | `net/avahi` port or `net/mDNSResponder` | Either provides `dns_sd.h` |

### Tasks

#### 3.1 Write `src/mdns.c`

Implement the same interface declared in `src/avahi.h` — `avahi_start()`,
`avahi_browse()`, etc. — using `DNSServiceRegister()`, `DNSServiceBrowse()`,
and `DNSServiceResolve()` from `<dns_sd.h>`.

The existing `avahi.c` and `avahi_browse.c` stay in the tree unchanged
(upstream sync integrity). The build system selects which implementation to
compile based on platform.

#### 3.2 Update `CMakeLists.txt` to select implementation

```cmake
if(MACOS)
    # dns_sd.h is part of the macOS SDK; no package needed
    set(MDNS_SOURCES src/mdns.c)
    target_compile_definitions(radio PUBLIC HAVE_DNSSD=1)
elseif(LINUX)
    pkg_check_modules(AVAHI_COMPAT libavahi-compat-libdns_sd)
    if(AVAHI_COMPAT_FOUND)
        set(MDNS_SOURCES src/mdns.c)
        target_link_libraries(radio PUBLIC ${AVAHI_COMPAT_LIBRARIES})
    else()
        set(MDNS_SOURCES src/avahi.c src/avahi_browse.c)  # fallback
    endif()
elseif(FREEBSD)
    find_library(DNSSD_LIB dns_sd)
    if(DNSSD_LIB)
        set(MDNS_SOURCES src/mdns.c)
        target_link_libraries(radio PUBLIC ${DNSSD_LIB})
    endif()
endif()
```

#### 3.3 Update CI dependency lists

- Ubuntu: add `libavahi-compat-libdns_sd-dev` to apt install; keep
  `libavahi-client-dev` for the fallback path
- macOS: **remove** the Avahi-from-source build (15+ line block); add nothing
  (dns_sd.h is in the SDK)
- FreeBSD: `pkg install avahi` already provides `dns_sd.h`

#### 3.4 Test service registration and discovery

On each platform: start `radiod` with `siggen.conf`, verify that
`avahi-browse -a` (Linux/FreeBSD) or `dns-sd -B _ka9q-radio._udp` (macOS)
shows the service. Verify `avahi_browse.c` equivalent (`avahi_browse` binary)
discovers the service correctly.

### Success criteria

- macOS CI no longer builds Avahi from source
- `radiod` registers its mDNS service correctly on all three platforms
- `avahi.c` and `avahi_browse.c` are unmodified from upstream

### Upstream sync risk: Low

`avahi.h` interface is unchanged. If upstream modifies `avahi.h`, `mdns.c`
needs a corresponding update — but the diff is bounded and obvious.

---

## Phase 4 — Unit tests: pure DSP layer (Goal 8, Layer 1)

**Theme:** Test the mathematical core of the demodulator without hardware,
network, or any daemon infrastructure. These tests run in under 30 seconds on
any build host, including CI.

### Tasks

#### 4.1 Add `tests/unit/` directory and CTest wiring

```cmake
enable_testing()

function(add_unit_test name sources)
    add_executable(${name} ${sources})
    target_link_libraries(${name} PRIVATE radio)
    add_test(NAME ${name} COMMAND ${name})
endfunction()
```

#### 4.2 Write test programs

Each is a standalone `main()` returning 0 on pass, non-zero on failure.
No test framework dependency — plain C with `assert()` and `fprintf(stderr)`.

| Test | File | What it validates |
|------|------|-------------------|
| `test_filter` | `tests/unit/test_filter.c` | Low-pass filter passes DC, rejects Nyquist/2; known attenuation at 3 dB point |
| `test_iir` | `tests/unit/test_iir.c` | Step response settles; Butterworth design matches expected poles |
| `test_osc` | `tests/unit/test_osc.c` | Phase accumulator error < 1 ULP after 10M steps; frequency accuracy |
| `test_rtp` | `tests/unit/test_rtp.c` | RTP packet encode → decode round-trip; sequence number wrap |
| `test_modes` | `tests/unit/test_modes.c` | Mode name ↔ enum round-trip; all known modes resolve |
| `test_fm_demod` | `tests/unit/test_fm_demod.c` | Synthetic FM IF at known deviation → correct audio frequency out |
| `test_ax25` | `tests/unit/test_ax25.c` | Known AX.25 frame encodes/decodes correctly |
| `test_bandplan` | `tests/unit/test_bandplan.c` | Frequency → band name lookups |

#### 4.3 Wire into CI

Add `ctest --test-dir build --output-on-failure` to all three platform CI jobs
(GitHub Actions Ubuntu/macOS, `vmactions/freebsd-vm` FreeBSD). Tests must pass before
package step runs.

### Success criteria

- `ctest` reports all tests passing on all three platforms
- Test suite completes in < 30 seconds
- No test requires network, hardware, avahi daemon, or portaudio

### Upstream sync risk: None

All test code is in `tests/unit/` — no upstream files modified.

---

## Phase 5 — Repo structure cleanup (Goal 5)

**Theme:** Separate build/CI/packaging artifacts from upstream source files.
Reduce root-directory clutter introduced by this branch without moving any file
that exists in the upstream repo.

### Tasks

#### 5.1 Move our added documentation into `docs/`

The following files were added by this branch (not upstream) and belong in
`docs/`:
- `CMAKE_README.md`, `COMPONENT_GUIDE.md`, `COMPONENTS.md`
- `DIRECTORY_STRUCTURE.md`, `FILE_STRUCTURE.txt`, `FINAL_SUMMARY.md`
- `INDEX.md`, `INIT_SYSTEMS.md`, `MACOS.md`, `QUICKSTART.md`
- `START_HERE.md`, `STATIC_VS_TEMPLATE.md`, `SYSTEMD_MIGRATION.md`
- `PROJECT_PLAN.md` (this file)

Keep upstream files (`README.md`, `README_OLD.md`, `LICENSE`, `Makefile`) at
root.

#### 5.2 Organize our packaging additions

```
packaging/
├── freebsd/
│   ├── radiod.in          (move from freebsd-radiod.in at root)
│   └── pkg-plist          (new: package file list for pkg create)
├── rpm/
│   ├── postinstall        (move from rpm-postinstall at root)
│   └── systemd-postinstall
└── macos/
    ├── homebrew-formula.rb    (move from macos-homebrew-formula.rb)
    └── macports-portfile      (move from macos-macports-portfile)
```

Upstream `debian/` and `systemd/` stay where they are.

#### 5.3 Update all path references

Update `CMakeLists.txt` and `.cirrus.yml` to reference new paths.

### Success criteria

- Root directory contains only upstream files plus `CMakeLists.txt`,
  `.cirrus.yml`, `PROJECT_PLAN.md`, and the `packaging/` and `tests/`
  directories we own
- CI still passes after path updates

### Upstream sync risk: Low

We are only moving files we added, not files from upstream.

---

## Phase 6 — Data/control plane separation + logging (Goals 6, 7)

**Theme:** Make the architectural boundary between DSP and control explicit.
Add structured logging to the control plane. Neither change alters the
external behavior of any binary.

### Tasks

#### 6.1 Define the boundary

Audit `radio.c`, `main.c`, and `radio.h` to document which functions belong
to which plane:

| Plane | Criterion | Examples |
|-------|-----------|---------|
| Data | Called from DSP threads; latency-sensitive; no I/O | `filter_*`, `fm_*`, `osc_*`, `iir_*` |
| Control | Configuration, mDNS, status, channel setup | `loadconfig`, `avahi_start`, `setup_hardware`, `send_radio_status` |
| Mixed | Currently straddles both | `radio.c` (setup + run loop), `multicast.c` (socket setup + send) |

The goal is a **documented interface**, not a physical split of `radio.c` yet.
Add a comment block to `radio.h` defining the boundary and thread-ownership
rules.

#### 6.2 Add `src/log.h` and `src/log.c`

```c
typedef enum { LOG_DEBUG, LOG_INFO, LOG_WARN, LOG_ERR } log_level_t;
void log_init(const char *subsystem, bool use_syslog);
void log_msg(log_level_t level, const char *subsystem, const char *fmt, ...);

#define LOG_DBG(sub, fmt, ...)  log_msg(LOG_DEBUG, sub, fmt, ##__VA_ARGS__)
#define LOG_INF(sub, fmt, ...)  log_msg(LOG_INFO,  sub, fmt, ##__VA_ARGS__)
#define LOG_WRN(sub, fmt, ...)  log_msg(LOG_WARN,  sub, fmt, ##__VA_ARGS__)
#define LOG_ERR(sub, fmt, ...)  log_msg(LOG_ERR,   sub, fmt, ##__VA_ARGS__)
```

Backend: `syslog(3)` when running as a daemon (detected by `!isatty(STDERR_FILENO)`),
`fprintf(stderr)` otherwise. No third-party logging library.

#### 6.3 Wire logging into control-plane files

Replace `fprintf(stderr, ...)` with structured log calls in:
- `src/avahi.c` / `src/mdns.c` — `[mdns]` subsystem
- `src/config.c` — `[config]` subsystem
- `src/status.c` — `[status]` subsystem
- `src/multicast.c` (setup/teardown paths only) — `[net]` subsystem
- `src/radio.c` (setup and config-reload paths only) — `[radio]` subsystem

Do **not** add logging to the hot DSP path (`filter.c`, `fm.c`, `iir.c`,
`osc.c`, etc.) — those functions are called millions of times per second.

#### 6.4 Add thread-plane documentation

Add a comment in `main.c` listing the threads, their priorities, and which
plane they belong to. This is input to Phase 7 integration tests which verify
that control-plane events (config reload, mDNS re-registration) do not block
or measurably affect the DSP path.

### Success criteria

- `radiod` logs config load, mDNS registration, and channel setup to syslog
  at `LOG_INFO` level when run as daemon
- `radiod -v` increases verbosity to `LOG_DEBUG`
- No new log calls in DSP hot-path code
- Existing `fprintf(stderr, ...)` in non-control-plane code left unchanged

### Upstream sync risk: Medium

`radio.c`, `config.c`, `status.c`, and `multicast.c` are all actively
maintained upstream. Strategy: keep each change to a single-line substitution
(`fprintf` → `LOG_INF`) so merge conflicts are mechanical and resolvable by
script if needed.

---

## Phase 7 — Integration and system tests (Goal 8, Layers 2 & 3)

**Theme:** Validate the full DSP pipeline end-to-end using `sig_gen` as a
hardware-free input source. No physical SDR hardware required.

### Prerequisites

- Phase 1 complete (`sig_gen.so` builds on all platforms)
- Phase 2 complete (CI can run tests)
- Phase 4 complete (CTest wiring established)

### Tasks

#### 7.1 Fix `sig_gen.c` portaudio dependency

`sig_gen.c` includes `<portaudio.h>` unconditionally but only uses portaudio
when `source = "/path/to/program"` is configured (audio injection from an
external process). Guard it:

```c
#ifdef HAVE_PORTAUDIO
#include <portaudio.h>
#endif
```

This makes `sig_gen.so` buildable on headless CI without portaudio, which is
important for FreeBSD CI where portaudio may not be installed.

#### 7.2 Loopback multicast setup

Each platform requires one command to enable multicast on the loopback
interface. These run as a CI pre-test step:

| Platform | Command |
|----------|---------|
| Linux | `ip link set lo multicast on && ip route add 224.0.0.0/4 dev lo` |
| macOS | `route add -net 224.0.0.0/4 -interface lo0` |
| FreeBSD | `route add -net 224.0.0.0/4 -interface lo0` |

#### 7.3 Layer 2: Integration tests

```
tests/integration/
├── test_siggen_powers.sh     # sig_gen → radiod → powers → verify SNR
├── test_siggen_pcmcat.sh     # sig_gen → radiod → pcmcat → verify PCM output
└── run_integration.sh        # orchestrator: start/stop radiod, cleanup
```

Test flow for `test_siggen_powers.sh`:
1. Start `radiod` with `config/radiod@siggen.conf` (10 MHz carrier at -20 dBFS)
2. Wait for mDNS registration (or poll `avahi-browse` / `dns-sd`)
3. Run `powers` for 5 seconds, capture JSON/text output
4. Assert reported SNR within expected range (carrier at -20 dBFS + noise at
   -20 dBFS → SNR ≈ 0 dB ± 3 dB)
5. Kill `radiod`, check exit status

Register with CTest using `add_test()` with a `LABELS integration` tag so
unit tests and integration tests can be run separately:
```
ctest -L unit          # fast, no network
ctest -L integration   # requires loopback multicast
```

#### 7.4 Layer 3: System tests (demodulation correctness)

```
tests/system/
├── test_am_demod.sh     # AM modulated sig_gen → verify audio frequency
├── test_fm_demod.sh     # FM modulated sig_gen → verify deviation
└── test_ssb_demod.sh    # LSB/USB → verify audio frequency shift
```

These use `pcmrecord` to capture a few seconds of demodulated audio, then a
small C analysis tool (`tests/system/analyze_pcm.c`) to FFT the captured audio
and verify the dominant frequency is within 1% of expected. This validates the
full chain: `sig_gen` → DSP pipeline → multicast → `pcmrecord` → audio
correctness.

#### 7.5 CI integration

- Unit tests: run on every push (< 30 seconds)
- Integration tests: run on every push after unit tests pass
- System tests: run nightly (scheduled workflow) or on release tags

### Success criteria

- `ctest -L integration` passes on all three platforms in CI
- `powers` reports correct SNR for a known sig_gen configuration
- `pcmrecord` captures PCM whose FFT matches the configured carrier offset
- No physical SDR hardware required for any test

### Upstream sync risk: Low

All test code is in `tests/` — no upstream files modified except the single
`#ifdef HAVE_PORTAUDIO` guard in `sig_gen.c`.

---

## Cross-cutting concerns

### Upstream sync strategy

- Maintain this work on `cmake-build-system` (or a successor branch)
- Rebase onto upstream `main` regularly (monthly, or when upstream has a
  significant commit burst)
- Keep `git diff upstream/main -- src/` as small as possible; the diff should
  be explainable in a paragraph
- Upstream files modified in total across all phases:
  - `src/multicast.c` — one `#elif __FreeBSD__` block (Phase 1)
  - `src/sig_gen.c` — one `#ifdef HAVE_PORTAUDIO` guard (Phase 7)
  - `src/avahi.c`, `src/config.c`, `src/status.c` — `fprintf` → `LOG_*`
    substitutions (Phase 6, mechanical)

### Files added by this project (not upstream)

```
CMakeLists.txt
.github/workflows/ci.yml    (upgraded; FreeBSD job added)
src/compat/arc4random.c     (Phase 1, only compiled when needed)
src/compat/strlcpy.c        (Phase 1, only compiled when needed)
src/mdns.c                  (Phase 3)
src/log.h                   (Phase 6)
src/log.c                   (Phase 6)
tests/                      (Phases 4, 7)
packaging/                  (Phase 5)
docs/                       (Phase 5)
```

### Local development parity (Goal 11)

The principle: **CI is an automated developer machine.** Every build and test
command that runs in CI must work identically on a developer's local machine.
No CI-only environment variables, hardcoded runner paths, or services that
only exist in hosted runners.

#### What this means per layer

**Building** — already satisfied by CMake. The same three commands work
everywhere:
```sh
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
cmake --install build --prefix ~/.local   # optional local install
```

**Unit tests** — already satisfied. Pure C executables with no external
dependencies:
```sh
ctest --test-dir build -L unit --output-on-failure
```

**Integration tests** — the main challenge. These require loopback multicast,
which is off by default on most systems. Provide a CMake target that sets it
up with a single command:
```sh
cmake --build build --target setup-test-network   # requires sudo
ctest --test-dir build -L integration --output-on-failure
cmake --build build --target teardown-test-network
```
The target shells out to the platform-appropriate command (see Phase 7 §7.2).
In CI the same target is called in a pre-test step; `sudo` is available on
all hosted runners.

**System tests** — require `radiod` and mDNS to be running. Provide a
`tests/system/run_local.sh` that starts and stops everything, matching what
the CI job does.

#### Local FreeBSD development

Developers working on macOS or Linux who need to validate FreeBSD-specific
changes have three options, in order of preference:

1. **FreeBSD VM via QEMU/UTM** — full native environment; same `cmake`/`ctest`
   commands. UTM (macOS) or `virt-manager` (Linux) with a FreeBSD 14 image.
2. **`act`** (https://github.com/nektos/act) — runs GitHub Actions workflows
   locally in Docker. Useful for testing CI configuration changes without a
   push. Does not emulate FreeBSD; use for Ubuntu job validation only.
3. **Cross-compile check only** — `cmake -DCMAKE_TOOLCHAIN_FILE=cmake/freebsd-cross.cmake`
   verifies FreeBSD-specific code paths compile; cannot run tests. Acceptable
   for quick iteration on `#ifdef __FreeBSD__` changes.

#### Rules enforced by design

- Tests must not reference absolute paths outside the build tree
- Tests must not require environment variables set only in CI (use CMake
  `configure_file` to embed paths at build time instead)
- Tests must not hardcode hostnames, multicast addresses beyond `224.0.0.0/4`,
  or port numbers without a compile-time default with an override
- `ctest -L unit` must pass with no network interfaces active (airplane mode)
- Any test that requires `sudo` must document it and degrade gracefully when
  run without it (skip with a clear message, not fail)

### Dependency minimization (Goal 10)

The principle: where the platform already provides a facility natively, use
it. Where a dependency is unavoidable, prefer widely-packaged libraries over
source builds or niche alternatives. Avoid compile-time or runtime dependencies
on platform-specific tools (binaries that must be present at runtime).

#### Inventory and strategy

| Dependency | Current status | Strategy |
|------------|---------------|----------|
| `libbsd` | Required on all platforms | Linux-only (Phase 1); replaced by `src/compat/` shims where glibc < 2.36 |
| `arc4random` | Via `libbsd` on Linux | CMake feature-detect; `src/compat/arc4random.c` shim via `getrandom(2)` if absent (Phase 1) |
| `strlcpy`/`strlcat` | Via `libbsd` on Linux | CMake feature-detect; `src/compat/strlcpy.c` shim if absent (Phase 1) |
| `avahi-browse`/`avahi-publish` | Runtime subprocess exec | Replaced by `dns_sd.h` API in `src/mdns.c` — no tool dependency (Phase 3) |
| `avahi-daemon` | Must be running on Linux | Replaced by native mDNSResponder on macOS; retained on Linux/FreeBSD where Avahi is the standard mDNS daemon |
| mDNS | `avahi-client` (Linux subprocess) | `dns_sd.h` natively on macOS; `avahi-compat-libdns_sd` on Linux; `avahi` or `mDNSResponder` port on FreeBSD (Phase 3) |
| `liquid-dsp` | Optional; `#if LIQUID` already guards it | CMake detects and sets flag; fallback Kaiser filter used when absent. No change needed. |
| `iniparser` | Required; no fallback | Make optional with embedded fallback: bundle `iniparser.c`+`iniparser.h` in `src/vendor/` (2 files, ~2000 lines, MIT). CMake prefers system package; falls back to vendored copy. Eliminates a packaging dependency on all three platforms. |
| `portaudio` | Required for `monitor`, `funcube.so`, `sig_gen` audio source | Already optional for `sig_gen` (Phase 7 guard). `monitor` and `funcube.so` legitimately need it; use platform-native package. Not a portability problem — portaudio itself abstracts CoreAudio/ALSA/OSS. |
| `fftw3f` | Core requirement | Universal; no alternative. Keep as-is. |
| `opus` | Core requirement | Universal; no alternative. Keep as-is. |
| `ncurses` | Required for `monitor`, `control` | Built-in on FreeBSD; packaged on Ubuntu/macOS. No change needed. |
| `libusb` | Required for `rx888.so`, `funcube.so` | Optional (hardware drivers only). No change needed. |
| `/proc/asound` | Used in `fcd.c` to find Funcube USB audio | Linux-only path; contained to `funcube.so` plugin. Acceptable — Funcube is a Linux USB audio device. Add `#ifdef __linux__` guard with a stub for other platforms. |

#### Native facilities by platform

The goal is not uniformity — it is using what the platform provides rather
than fighting it:

| Facility | Linux (Ubuntu) | macOS | FreeBSD |
|----------|---------------|-------|---------|
| mDNS | Avahi (`avahi-daemon`) via `dns_sd.h` compat | mDNSResponder (native, `dns_sd.h`) | Avahi or mDNSResponder via `dns_sd.h` |
| Audio | ALSA/PulseAudio via portaudio | CoreAudio via portaudio | OSS via portaudio |
| Service mgmt | systemd (optional) | launchd (not used) | rc.d (template provided) |
| Capabilities | `setcap` (optional, graceful fallback) | not applicable | not applicable |
| Package format | `.deb` | `.tar.gz` | `.txz` |

portaudio is the right abstraction for audio precisely because it maps to
CoreAudio, ALSA, and OSS natively on each platform — it is not a layer of
added complexity but a thin portability shim with no runtime daemon dependency.

### Dependency summary by platform

| Library | Ubuntu | macOS | FreeBSD | Notes |
|---------|--------|-------|---------|-------|
| fftw3f | `libfftw3-dev` | `brew install fftw` | `pkg install fftw3` | Required |
| opus | `libopus-dev` | `brew install opus` | `pkg install opus` | Required |
| libbsd | `libbsd-dev` (glibc < 2.36 only) | not needed | not needed | Phase 1 |
| iniparser | system or vendored | system or vendored | system or vendored | Goal 10 |
| ncurses | `libncurses5-dev` | `brew install ncurses` | built-in | Required |
| portaudio | `portaudio19-dev` | `brew install portaudio` | `pkg install portaudio` | Optional |
| libusb | `libusb-1.0-0-dev` | `brew install libusb` | `pkg install libusb` | Optional (HW drivers) |
| mDNS | `libavahi-compat-libdns_sd-dev` | SDK built-in | `pkg install avahi` | Phase 3 |
| liquid-dsp | `libliquid-dev` (optional) | `brew install liquid-dsp` (optional) | `pkg install liquid-dsp` (optional) | Already guarded |
| airspy | `libairspy-dev` | `brew install airspy` | `pkg install airspy` | Optional (HW) |
| rtlsdr | `librtlsdr-dev` | `brew install librtlsdr` | `pkg install rtl-sdr` | Optional (HW) |
| hackrf | `libhackrf-dev` | `brew install hackrf` | `pkg install hackrf` | Optional (HW) |
