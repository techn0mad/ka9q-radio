# Portable service management (init systems)

ka9q-radio ships long-running daemons — `radiod` and helpers such as `packetd`,
`aprsfeed`, and the decoders — that a host normally runs as managed services
that start at boot and restart on failure. Each operating system supervises
services with its own native manager. This document describes how service
definitions are organized so the same daemons can be installed as services on
Linux, macOS, and the BSDs.

---

## 1. What is and isn't platform-specific

The daemons themselves and their configuration are portable — a `radiod`
instance is launched the same way everywhere:

```
radiod -N <instance> /etc/radio/radiod@<instance>.conf
```

Only the **service-manager integration** — the file format and tooling that
tells the OS how to start, stop, supervise, and enable that command — is
platform-specific. That integration is therefore kept out of the portable
source and grouped by platform, and the build installs only the form
appropriate to the target OS.

---

## 2. Layout

Service-manager definitions live in two places, one per family:

```
service/                     systemd units (.service.in)     ← Linux (systemd)
platform/
  freebsd/rc.d/              rc.subr scripts                 ← FreeBSD
  openbsd/rc.d/              rcctl scripts                   ← OpenBSD
  macos/launchd/            launchd .plist templates         ← macOS
  linux/openrc/  runit/     (optional non-systemd Linux)
```

`service/` (systemd) predates this layout and is left in place; the `platform/`
tree holds the equivalents for the other systems. Each directory has a small
Makefile that expands `@bindir@` / `@sbindir@` / `@sysconfdir@` placeholders and
installs into the location that platform's manager expects.

---

## 3. Selecting the init system

The top-level build selects one integration automatically and lets you override
it, mirroring the `MDNS_BACKEND` mechanism used for service discovery:

```make
INIT_SYSTEM ?= auto
# auto resolves by uname:
#   Linux   -> systemd      (service/)
#   Darwin  -> launchd      (platform/macos)
#   FreeBSD -> freebsd-rc   (platform/freebsd)
#   OpenBSD -> openbsd-rc   (platform/openbsd)
#   other   -> none
```

Only the selected subdirectory is built and installed, so `make install` on a
platform never tries to drop a systemd unit onto a system that has no systemd.

| Platform | `auto` selection | Definition installed to |
|----------|------------------|-------------------------|
| Linux (systemd) | `systemd` | `/etc/systemd/system/` |
| macOS | `launchd` | launchd template under `$(pkgdatadir)/launchd/` |
| FreeBSD | `freebsd-rc` | `/usr/local/etc/rc.d/` |
| OpenBSD | `openbsd-rc` | `/etc/rc.d/` |
| other / minimal | `none` | (nothing installed) |

Override examples: `make INIT_SYSTEM=none install` (package build that installs
units separately), or `make INIT_SYSTEM=openrc install` on a non-systemd Linux
distribution once that backend is added.

---

## 4. How the `radiod` service maps across managers

The systemd template `radiod@.service` is the reference definition. The other
managers express the same intent with their own idioms:

| Concern | systemd | launchd (macOS) | rc.d (FreeBSD/OpenBSD) |
|---------|---------|-----------------|------------------------|
| Launch command | `ExecStart=` | `ProgramArguments` array | `command=` |
| Run as user/group | `User=` / `Group=` | `UserName` / `GroupName` | `${name}_user` + `daemon -u` |
| Start at boot | `WantedBy=multi-user.target` | `RunAtLoad` | `radiod_enable="YES"` in `rc.conf` |
| Restart on failure | `Restart=always` | `KeepAlive` | `daemon(8) -r` (no native supervision) |
| Priority | `Nice=-10` | `Nice` / `ProcessType` | `daemon`/`rtprio` or launcher nice |
| Enable/manage | `systemctl` | `launchctl` | `service` / `rcctl` |

Two things do **not** translate cleanly, and the ports handle them explicitly:

- **Instance templating.** systemd's `radiod@%i` turns one template into many
  services (`radiod@ka9q-hf`, `radiod@ka9q-2m`, …). launchd and rc.d have no
  `@` mechanism. Because the launch command already takes the instance name as
  an argument, the ports parameterize on an instance/config name — one rc.conf
  variable (`radiod_instance=`) or one plist per instance — rather than a single
  auto-instantiated unit.
- **Sandboxing / hardening.** The systemd unit's `CapabilityBoundingSet`,
  `NoNewPrivileges`, `Protect*`, `RestrictNamespaces`, and `StateDirectory`
  keys are Linux/systemd-specific and have no portable equivalent. The non-Linux
  services provide the same *operational* behavior (run unprivileged as the
  `radio` user, restart on failure) but are security-thinner; on FreeBSD a
  **jail** can supply isolation where required. launchd offers only a subset
  (`UserName`, `GroupName`, `Umask`, `Nice`). This is an inherent platform gap,
  not a defect of the port.

---

## 5. Installing and enabling

The `radio` service account and the config file (`/etc/radio/radiod@<instance>.conf`)
are prerequisites on every platform. After `make install`:

**Linux (systemd)**
```sh
sudo systemctl enable --now radiod@ka9q-hf
```

**FreeBSD**
```sh
sysrc radiod_enable=YES
sysrc radiod_instance=ka9q-hf
service radiod start
```

**OpenBSD**
```sh
rcctl enable radiod
rcctl set radiod flags -N ka9q-hf
rcctl start radiod
```

**macOS (launchd)** — instantiate a plist per receiver from the installed
template, then load it:
```sh
sudo cp $(pkgdatadir)/launchd/net.ka9q.radiod.plist \
        /Library/LaunchDaemons/net.ka9q.radiod.ka9q-hf.plist
# edit the Label and @INSTANCE@ occurrences for this receiver
sudo launchctl bootstrap system /Library/LaunchDaemons/net.ka9q.radiod.ka9q-hf.plist
```

---

## 6. Source layout

| Path | Role |
|------|------|
| `service/` | systemd unit templates (Linux) |
| `platform/freebsd/rc.d/radiod.in` | FreeBSD rc.subr service script |
| `platform/openbsd/rc.d/radiod.in` | OpenBSD rc.d service script |
| `platform/macos/launchd/net.ka9q.radiod.plist.in` | macOS launchd template |
| `platform/*/Makefile` | expand placeholders and install for that platform |
| top-level `Makefile` | `INIT_SYSTEM` selection of the service subdirectory |

The systemd integration is left intact; the other managers are added alongside
it and chosen at build time, so the portable daemons and configuration never
change.

---

## 7. Not yet covered

Two other pockets of Linux/distro coupling remain outside this document's scope
and are handled elsewhere or still pending:

- `aux/` mixes portable helper scripts with Linux-only artifacts (`*.sysusers`,
  `*.tmpfiles`, `*.rotate` for logrotate, sysctl drop-ins). Its Makefile should
  skip the Linux-only pieces off-Linux.
- `debian/` is Debian packaging; other platforms' packaging (a FreeBSD port, a
  Homebrew formula) would live under `packaging/`.
