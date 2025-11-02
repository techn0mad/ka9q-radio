# Component-Based Packaging - Visual Guide

## Build Process Flow

```
┌─────────────────────────────────────────────────────────────┐
│  Build System (can be ANY Linux - systemd or not)          │
└─────────────────────────────────────────────────────────────┘
                            │
                            │ cmake .. && cmake --build .
                            ▼
┌─────────────────────────────────────────────────────────────┐
│  Built Artifacts                                            │
│  ├── radiod                                                 │
│  ├── control, monitor, etc.                                 │
│  ├── config files                                           │
│  └── systemd service files (always generated)              │
└─────────────────────────────────────────────────────────────┘
                            │
                            │ cpack -G DEB
                            ▼
┌──────────────────────────┬──────────────────────────────────┐
│  RUNTIME Component       │  SYSTEMD Component               │
│  ka9q-radio.deb          │  ka9q-radio-systemd.deb          │
│                          │                                  │
│  Contains:               │  Contains:                       │
│  • radiod daemon         │  • radiod@.service               │
│  • control/monitor       │  • 98-sockbuf.conf               │
│  • utilities             │                                  │
│  • config files          │  Dependencies:                   │
│  • udev rules            │  • ka9q-radio (= 1.0.0)          │
│                          │  • systemd                       │
│  Dependencies:           │                                  │
│  • libfftw3-single3      │                                  │
│  • libopus0              │                                  │
│  • libbsd0               │                                  │
│  • (NO systemd)          │                                  │
└──────────────────────────┴──────────────────────────────────┘
```

## Installation Scenarios

### Scenario A: systemd-based System (Debian, Ubuntu, Fedora)

```
User:                                System:
┌─────────────────┐                 ┌──────────────┐
│ Install both:   │                 │ Debian 12    │
│                 │                 │ with systemd │
│ apt install     │    ────────>    │              │
│  ka9q-radio     │                 │ ✓ radiod     │
│  ka9q-radio-    │                 │ ✓ systemctl  │
│   systemd       │                 │ ✓ service    │
└─────────────────┘                 └──────────────┘

Result:
$ sudo systemctl start radiod@hf
$ sudo systemctl status radiod@hf
● radiod@hf.service - KA9Q Radio Daemon (hf)
   Active: active (running)
```

### Scenario B: Non-systemd System (Devuan, Void Linux, etc.)

```
User:                                System:
┌─────────────────┐                 ┌──────────────┐
│ Install only:   │                 │ Devuan       │
│                 │                 │ with sysvinit│
│ apt install     │    ────────>    │              │
│  ka9q-radio     │                 │ ✓ radiod     │
│                 │                 │ ✗ systemd    │
│                 │                 │              │
└─────────────────┘                 └──────────────┘

Result:
$ /usr/local/sbin/radiod -v /etc/radio/radiod@hf.conf
# Or create /etc/init.d/radiod script
```

### Scenario C: Migration Path

```
Initial State: Non-systemd System
┌────────────────────────────────────────┐
│ apt install ka9q-radio                 │
│                                        │
│ Running: /usr/local/sbin/radiod ...    │
└────────────────────────────────────────┘

Later: System upgraded to systemd
┌────────────────────────────────────────┐
│ apt install ka9q-radio-systemd         │
│                                        │
│ Now available:                         │
│   systemctl start radiod@config        │
│                                        │
│ Old method still works too!            │
└────────────────────────────────────────┘
```

## Package Dependency Graph

```
┌──────────────────────────────────────────────────────────┐
│                    ka9q-radio                            │
│                   (runtime package)                      │
│                                                          │
│  Dependencies:                                           │
│  ├── libfftw3-single3                                    │
│  ├── libopus0                                            │
│  ├── libbsd0                                             │
│  ├── libiniparser1                                       │
│  ├── libavahi-client3                                    │
│  └── libc6                                               │
└──────────────────────────────────────────────────────────┘
                          ▲
                          │
                          │ Depends on
                          │
┌──────────────────────────────────────────────────────────┐
│              ka9q-radio-systemd                          │
│            (systemd support package)                     │
│                                                          │
│  Dependencies:                                           │
│  ├── ka9q-radio (= 1.0.0)  ◄─── Must match version     │
│  └── systemd                                             │
└──────────────────────────────────────────────────────────┘
```

## File Distribution

```
Runtime Package (ka9q-radio):
/usr/local/
├── sbin/
│   └── radiod                      ← Main daemon
├── bin/
│   ├── control                     ← Client programs
│   ├── monitor
│   ├── pcmrecord
│   ├── pcmplay
│   └── opussend
└── share/ka9q-radio/
    ├── presets.txt
    └── modes.txt

/etc/
├── radio/                          ← Configuration
│   └── [config files]
└── udev/rules.d/                   ← Hardware rules
    └── [udev rules]

/var/lib/ka9q-radio/                ← State directory

systemd Package (ka9q-radio-systemd):
/etc/
├── systemd/system/
│   └── radiod@.service             ← Service template
└── sysctl.d/
    └── 98-sockbuf.conf             ← Kernel tuning
```

## Comparison with Previous Approaches

### Old Approach: Monolithic Package with systemd

```
┌────────────────────────────────┐
│   ka9q-radio (monolithic)      │
│                                │
│   Includes: everything         │
│   Depends: ... systemd         │
└────────────────────────────────┘
        │
        ├──> Works: systemd systems ✓
        └──> Fails: non-systemd ✗
```

### Old Approach: Monolithic Package without systemd

```
┌────────────────────────────────┐
│   ka9q-radio (monolithic)      │
│                                │
│   Includes: everything         │
│   Depends: ... (no systemd)    │
└────────────────────────────────┘
        │
        ├──> Works: all systems ✓
        └──> Missing: systemd users need manual setup ⚠
```

### New Approach: Component-Based

```
┌─────────────────┐    ┌──────────────────┐
│   ka9q-radio    │    │ ka9q-radio-      │
│   (runtime)     │    │  systemd         │
│                 │    │  (optional)      │
│ Depends: core   │    │ Depends: runtime │
│          libs   │    │         +systemd │
└─────────────────┘    └──────────────────┘
        │                       │
        ├──> Works: everywhere ✓
        └──> Install both if using systemd ✓
             Install runtime only otherwise ✓
```

## Build Matrix

```
Build Host      │ systemd pkg? │ Can install on...
────────────────┼──────────────┼───────────────────────────
Debian+systemd  │ Yes (always) │ Any system
Devuan          │ Yes (always) │ Any system
Ubuntu          │ Yes (always) │ Any system
Docker Alpine   │ Yes (always) │ Any system

The systemd component is ALWAYS generated, regardless of
whether systemd is present on the build system!
```

## User Decision Tree

```
                  Installing ka9q-radio?
                           │
           ┌───────────────┴───────────────┐
           │                               │
    Does system use systemd?               │
           │                               │
    ┌──────┴──────┐                       │
    │             │                       │
   YES           NO                       │
    │             │                       │
    ▼             ▼                       │
Install both   Install runtime only      │
packages       package                   │
    │             │                       │
    │             │                       │
    ▼             ▼                       │
systemctl     Manual start                │
  start       or init script              │
radiod@hf                                 │
```

## Quick Command Reference

### Building
```bash
cmake .. && cmake --build . -j
cpack -G DEB                          # Both packages
cpack -G DEB -D CPACK_COMPONENTS_ALL=runtime  # Runtime only
cpack -G DEB -D CPACK_COMPONENTS_ALL=systemd  # systemd only
```

### Installing on systemd
```bash
sudo dpkg -i ka9q-radio_*.deb ka9q-radio-systemd_*.deb
sudo systemctl start radiod@config
```

### Installing on non-systemd
```bash
sudo dpkg -i ka9q-radio_*.deb
/usr/local/sbin/radiod -v /etc/radio/radiod@config.conf
```

### Checking what you have
```bash
dpkg -l | grep ka9q-radio
dpkg -L ka9q-radio                    # Show runtime files
dpkg -L ka9q-radio-systemd            # Show systemd files
```

## Summary

**Component-based packaging solves the init system problem:**

✅ Build once on any system  
✅ Works on all systems  
✅ Users choose what to install  
✅ No forced dependencies  
✅ Professional packaging  
✅ Easy to distribute  

This is the **recommended approach** for ka9q-radio.
