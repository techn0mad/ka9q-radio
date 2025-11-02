# START HERE - ka9q-radio CMake Build System

Welcome! This package provides a complete, modern CMake build system for ka9q-radio with **component-based packaging** that works across all init systems.

## What's New?

The key innovation is **component-based packaging**: one build creates two packages:
1. **ka9q-radio** - Runtime (works everywhere)
2. **ka9q-radio-systemd** - Optional systemd support

This means you can:
- ✅ Build once on ANY system (even without systemd)
- ✅ Support systemd AND non-systemd users from the same build
- ✅ Let users install only what they need
- ✅ No dependency conflicts

## Quick Start (TL;DR)

```bash
# 1. Get ka9q-radio
git clone https://github.com/ka9q/ka9q-radio.git
cd ka9q-radio

# 2. Add CMake files (copy from this package)
cp /path/to/CMakeLists.txt .
mkdir -p systemd debian freebsd
cp /path/to/radiod@.service.in systemd/
cp /path/to/debian-postinst debian/postinst
cp /path/to/debian-systemd-postinst debian/systemd-postinst

# 3. Build
mkdir build && cd build
cmake ..
cmake --build . -j

# 4. Create packages
cpack -G DEB

# Result: Two .deb files
# - ka9q-radio-1.0.0-Linux.deb (runtime, works everywhere)
# - ka9q-radio-systemd-1.0.0-Linux.deb (optional systemd support)
```

## Files in This Package

### Essential Files
- **CMakeLists.txt** - Main build configuration
- **README.md** - Complete file listing and overview (start here for details)
- **CMAKE_README.md** - Build instructions and options

### Key Concept Guides  
- **COMPONENTS.md** - Component-based packaging explained
- **COMPONENT_GUIDE.md** - Visual guide with diagrams
- **INIT_SYSTEMS.md** - systemd vs non-systemd scenarios

### Platform-Specific
- **MACOS.md** - macOS with Homebrew/MacPorts
- **QUICKSTART.md** - Integration steps
- **systemd/radiod@.service.in** - systemd service template
- **debian/postinst** - Runtime package install script
- **debian/systemd-postinst** - systemd package install script
- **freebsd/radiod.in** - FreeBSD rc.d script

## Which Document Should I Read?

### I want to...

**...understand the component approach**
→ Read **COMPONENT_GUIDE.md** (visual guide with diagrams)

**...build packages right now**
→ Read **QUICKSTART.md** (step-by-step integration)

**...understand systemd vs non-systemd**
→ Read **INIT_SYSTEMS.md** (detailed scenarios)

**...build on macOS**
→ Read **MACOS.md** (Homebrew/MacPorts support)

**...see all available options**
→ Read **CMAKE_README.md** (complete reference)

**...understand what's included**
→ Read **README.md** (file listing and descriptions)

## The Core Innovation: Component-Based Packaging

### Old Way (Problems)
```
Build on systemd → Package requires systemd → Won't install on Devuan ✗
Build on Devuan  → Package has no systemd → systemd users manual setup ⚠
```

### New Way (Solution)
```
Build on ANY system → Two packages:
  1. Runtime (no systemd dep) → Works everywhere ✓
  2. systemd support (optional) → systemd users install both ✓
```

### Visual Summary
```
                    ONE BUILD
                        │
        ┌───────────────┴───────────────┐
        │                               │
   ka9q-radio.deb             ka9q-radio-systemd.deb
   (Runtime only)             (Optional systemd)
        │                               │
        ├─> Devuan: Install this only  │
        └─> Debian: Install both <──────┘
```

## Installation Scenarios

### On Debian/Ubuntu (systemd)
```bash
sudo dpkg -i ka9q-radio_1.0.0_amd64.deb
sudo dpkg -i ka9q-radio-systemd_1.0.0_all.deb
sudo systemctl start radiod@hf
```

### On Devuan (sysvinit)
```bash
sudo dpkg -i ka9q-radio_1.0.0_amd64.deb
/usr/local/sbin/radiod -v /etc/radio/radiod@hf.conf
```

### On Void Linux (runit)
```bash
# Install runtime .deb or convert with alien
# Create runit service (see INIT_SYSTEMS.md)
```

## Key Features

### Cross-Platform
- ✅ Linux (any init system)
- ✅ FreeBSD
- ✅ macOS (with auto Homebrew/MacPorts detection)

### Smart Dependencies
- ✅ Auto-detects available hardware (airspy, hackrf, rtl-sdr)
- ✅ Auto-detects macOS package manager
- ✅ Optional dependencies don't break build

### Professional Packaging
- ✅ Component-based (runtime + systemd)
- ✅ DEB, RPM, TXZ generation
- ✅ Post-install scripts
- ✅ User/group creation
- ✅ Service file installation

### Compatible
- ✅ Coexists with existing Makefiles
- ✅ Follows FHS conventions
- ✅ Works with any init system
- ✅ No forced dependencies

## Build Options

```bash
cmake .. \
  -DCMAKE_BUILD_TYPE=Release \
  -DENABLE_HACKRF=ON \
  -DENABLE_SDRPLAY=OFF \
  -DCMAKE_INSTALL_PREFIX=/usr/local
```

Available options:
- `CMAKE_BUILD_TYPE`: Debug or Release
- `ENABLE_HACKRF`: HackRF support
- `ENABLE_AIRSPY`: Airspy support (auto-detected)
- `ENABLE_RTLSDR`: RTL-SDR support (auto-detected)
- `ENABLE_SDRPLAY`: SDRplay (requires manual SDK)
- `ENABLE_FOBOS`: Fobos SDR (requires manual setup)

## Package Contents

### Runtime Package (ka9q-radio)
- radiod daemon
- Client programs (control, monitor)
- Utilities (pcmrecord, pcmplay, opussend, metadump)
- Configuration files
- Support files
- udev rules

**Dependencies:** Core libraries only (no systemd)

### systemd Package (ka9q-radio-systemd)  
- radiod@.service template
- 98-sockbuf.conf (kernel tuning)

**Dependencies:** ka9q-radio + systemd

## Testing Your Build

```bash
# Check what packages were created
ls -lh *.deb

# Check package contents
dpkg -c ka9q-radio_*.deb
dpkg -c ka9q-radio-systemd_*.deb

# Check dependencies
dpkg -I ka9q-radio_*.deb | grep Depends
dpkg -I ka9q-radio-systemd_*.deb | grep Depends

# Install and test
sudo dpkg -i ka9q-radio_*.deb
radiod --help

# On systemd systems, also install
sudo dpkg -i ka9q-radio-systemd_*.deb
systemctl cat radiod@
```

## Common Questions

**Q: Do I need systemd on the build machine?**  
A: No! The systemd package is always created, even without systemd.

**Q: Can I build on Devuan and deploy on Debian?**  
A: Yes! Build once, deploy everywhere.

**Q: What if I only want one package?**  
A: `cpack -G DEB -D CPACK_COMPONENTS_ALL=runtime` (runtime only)

**Q: Can users upgrade from old monolithic packages?**  
A: Yes, smoothly. See COMPONENTS.md for migration paths.

**Q: What about RPM?**  
A: Same component approach works: `cpack -G RPM`

**Q: Does this work on FreeBSD?**  
A: Yes! Creates .txz packages with rc.d scripts.

**Q: What about macOS?**  
A: Yes! Auto-detects Homebrew or MacPorts.

## Troubleshooting

### "systemd not found during build"
This is fine! The systemd package is still created.

### "Package has unmet dependencies: systemd"
You're trying to install the systemd package on a non-systemd system.  
Install only the runtime package.

### "Library not found: libfftw3f"
Install dependencies first. See CMAKE_README.md for your platform.

### "Source file not found: src/radiod.c"
Adjust paths in CMakeLists.txt. See QUICKSTART.md.

## Next Steps

1. **Integration**: Follow QUICKSTART.md to integrate into ka9q-radio
2. **Understand components**: Read COMPONENT_GUIDE.md  
3. **Test on your platform**: Build and test packages
4. **Read platform docs**: MACOS.md, INIT_SYSTEMS.md as needed
5. **Deploy**: Distribute both packages

## Advantages Over Previous Approaches

| Feature | Old Monolithic | Component-Based |
|---------|---------------|-----------------|
| Works on systemd | ✓ | ✓ |
| Works on non-systemd | ✗ or ⚠ | ✓ |
| Single build | ✓ | ✓ |
| No forced deps | ✗ | ✓ |
| Optional systemd | ✗ | ✓ |
| Professional packaging | ~ | ✓ |

## Support Matrix

| Build Host | Install Target | Works? | Notes |
|------------|---------------|---------|-------|
| Debian+systemd | Debian+systemd | ✓✓ | Install both packages |
| Debian+systemd | Devuan | ✓ | Install runtime only |
| Devuan | Debian+systemd | ✓✓ | Install both packages |
| Devuan | Devuan | ✓ | Install runtime only |
| Any | Any | ✓ | Maximum compatibility |

✓✓ = Perfect, full functionality  
✓ = Works, some features optional

## Philosophy

This build system follows these principles:

1. **Build once, deploy everywhere**
2. **No forced dependencies**
3. **User choice over developer convenience**
4. **Professional packaging standards**
5. **Init system agnostic**
6. **Backward compatible**
7. **Well documented**

## Credits

CMake Build System for ka9q-radio  
Version 1.0  
November 2025

Based on ka9q-radio by Phil Karn KA9Q  
https://github.com/ka9q/ka9q-radio

## License

These CMake files follow the same license as ka9q-radio (GPL-3.0)

## Getting Help

- **Integration issues**: See QUICKSTART.md
- **Build errors**: See CMAKE_README.md troubleshooting
- **Package questions**: See COMPONENTS.md
- **Init system setup**: See INIT_SYSTEMS.md
- **macOS issues**: See MACOS.md
- **ka9q-radio itself**: https://github.com/ka9q/ka9q-radio

---

**Ready to start?** Follow QUICKSTART.md for step-by-step integration!
