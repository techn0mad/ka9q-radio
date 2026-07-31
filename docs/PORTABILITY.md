# Front-end driver support by operating system

`radiod` loads each SDR front end as a separate `.so` plugin, gated by an
`ENABLE_*` variable in the top-level `Makefile`. A driver can therefore be
built on one platform and skipped on another without touching the core. What
decides it is almost always whether that platform packages the vendor library
the driver links against.

This table records which drivers are available where, and how confident we are
about each cell.

---

## How to read the table

| Mark | Meaning |
|------|---------|
| **built** | Compiled and installed by `.github/workflows/portable-build.yml`. Verified. |
| **pkg** | The library is packaged for this platform, but no build has been run. Availability only. |
| **no** | Not available. The reason is given in the notes below. |
| **off** | Disabled by default on every platform. |

"**pkg**" is not a promise that the driver builds — only that the dependency
can be installed. Cells move to "**built**" when CI has actually compiled them.

---

## Matrix

| Driver | Links against | Linux (Debian/Ubuntu) | macOS (MacPorts) | FreeBSD (ports) | OpenBSD (ports) |
|--------|---------------|-----------------------|------------------|-----------------|-----------------|
| `airspy`   | `libairspy`     | built | pkg | pkg `comms/airspy`   | no [^1] |
| `airspyhf` | `libairspyhf`   | built | pkg | pkg `comms/airspyhf` | no [^1] |
| `bladerf`  | `libbladeRF`    | built | no [^2] | pkg `comms/bladerf` | no [^1] |
| `fobos`    | `libfobos`      | built [^3] | no [^1] | no [^5] | no [^1] |
| `funcube`  | `portaudio`, `libusb` | built | pkg | pkg | pkg |
| `hackrf`   | `libhackrf`, `libusb` | built | pkg | pkg `comms/hackrf` | pkg `comms/hackrf` |
| `hydrasdr` | `libhydrasdr`   | built [^3] | no [^1] | no [^6] | no [^1] |
| `rtlsdr`   | `librtlsdr`     | built | pkg | pkg `comms/rtl-sdr`  | pkg `comms/rtl-sdr` |
| `rx888`    | `libusb`        | built | pkg | pkg | pkg |
| `sdrplay`  | `libsdrplay_api`| off [^4] | off [^4] | off [^4] | off [^4] |
| `sig_gen`  | `libsamplerate` | built | pkg | pkg | pkg |

[^1]: No port exists in that platform's ports tree.
[^2]: A port exists, but MacPorts' `bladeRF` lists `tecla` in `depends_lib` and
    passes `-DENABLE_LIBTECLA=ON` unconditionally, with no variant to opt out.
    `tecla` has no prebuilt archive and fails to build from source. ka9q needs
    only `libbladeRF`; `tecla` is wanted by `bladeRF-cli`, which the port builds
    alongside it.
[^3]: `libfobos-dev` and `libhydrasdr-dev` are not in the stock Debian/Ubuntu
    archive. They come from KA9Q's own repository — see `docs/INSTALL.md`.
[^5]: The library is packaged (`comms/libfobos`), but it is version 2.3.2,
    whose `fobos_rx_close()` takes a second `do_reset` argument. `src/fobos.c`
    is written against the older one-argument API that Ubuntu gets from KA9Q's
    repository, so all six call sites fail to compile. Disabled in CI; see
    `TODO.md`.

[^6]: Packaged as `comms/hydrasdr`, but at version 1.0.3. `src/hydrasdr.c`
    requires 1.1.0 -- it says so itself, as `MIN_LIB_VERSION
    HYDRASDR_MAKE_VERSION(1, 1, 0)` -- and uses symbols absent from 1.0.3, so
    it cannot compile far enough to reach its own runtime version check.
    Disabled in CI; see `TODO.md`.

[^4]: `ENABLE_SDRPLAY` defaults to `0` everywhere. The SDRplay API is
    proprietary and not redistributable through any packaging system.

---

## Platform notes

**Linux (Debian/Ubuntu)** is the reference platform and the only one where the
whole matrix is verified. The CI job adds KA9Q's apt repository so the full
default driver set builds.

**macOS** uses MacPorts, the environment `docs/notes.md` records upstream as
using, and the one `src/Makefile` expects on Darwin (`DARWIN_PREFIX`, default
`/opt/local`). Three drivers are unavailable. Nothing in the macOS column is
build-verified yet — the CI job has not gotten past dependency installation, and
`docs/notes.md` notes that `radiod` itself has never been run on macOS.

**FreeBSD** has the broadest non-Linux library coverage: every driver library is
in the ports tree, including the three missing on macOS. Two of them are at versions this
source does not match -- `libfobos` newer, `hydrasdr` older -- so those drivers
are disabled pending the fixes noted in `TODO.md`.

**OpenBSD** packages only `hackrf` and `rtl-sdr` among the vendor libraries, so
it is limited to those plus the drivers that need nothing beyond `libusb`,
`portaudio`, and `libsamplerate`.

Service discovery and init-system integration vary by platform too, and are
covered separately in `docs/PORTABLE-MDNS.md` and `docs/PORTABLE-INIT.md`.

---

## Keeping this accurate

CI is the source of truth for "**built**". A cell may only say **built** if
`.github/workflows/portable-build.yml` compiles that driver on that platform;
anything else is **pkg** or **no**. When a job's `ENABLE_*` flags change, or a
platform is added, this table changes with it.

The "**pkg**" cells were established by checking each platform's ports tree
directly, not by building. Treat them as a starting point for the work, not as
a support claim.
