# ka9q-radio CMake Build System - File Summary

This package contains a complete CMake build system for ka9q-radio with cross-platform support.

## Files Included

### Core Build Files

**CMakeLists.txt** (11KB)
- Main CMake configuration file
- Handles dependency detection for all platforms
- Automatically detects macOS package managers (Homebrew/MacPorts)
- Configures optional hardware drivers
- Sets up package generation (DEB/RPM/TXZ)
- Creates proper installation layout

**CMAKE_README.md** (6.5KB)
- Complete documentation for the CMake build system
- Prerequisites for each platform
- Build instructions and options
- Package generation guide
- Troubleshooting section

**QUICKSTART.md** (5.2KB)
- Step-by-step integration guide
- How to adapt to actual source layout
- Testing and deployment procedures
- Common adjustments needed

**MACOS.md** (5KB)
- Comprehensive macOS-specific guide
- Automatic Homebrew/MacPorts detection
- Dependency installation for both package managers
- Troubleshooting macOS-specific issues
- Notes for package maintainers

**COMPONENTS.md** (9KB)
- Complete guide to component-based packaging
- How to build separate runtime and systemd packages
- Installation scenarios for different systems
- Advantages over monolithic packages
- Repository layout recommendations

**INIT_SYSTEMS.md** (11KB)
- Comprehensive init system support documentation
- systemd vs non-systemd scenarios
- Manual service setup for various init systems
- Build strategy recommendations
- Compatibility matrix

### Platform Support Files

**systemd/radiod@.service.in** (628 bytes)
- systemd service template for Linux
- Configured by CMake at build time
- Supports multiple instances via @ syntax
- Part of systemd component package

**debian/postinst** (2KB)
- Debian runtime package post-installation script
- Creates radio user and group
- Sets up permissions
- Adapts to systemd presence
- Reloads systemd and udev

**debian/systemd-postinst** (1KB)
- Post-installation script for systemd component
- Reloads systemd daemon
- Provides usage instructions

**freebsd/radiod.in** (3KB)
- FreeBSD rc.d script template
- Supports multiple daemon instances
- Handles user/group creation
- Start/stop/status commands

## Quick Start

1. Copy CMakeLists.txt to ka9q-radio root directory
2. Adjust source file paths if needed (see QUICKSTART.md)
3. Build:
   ```bash
   mkdir build && cd build
   cmake ..
   cmake --build . -j
   sudo cmake --install .
   ```
4. Create packages (optional):
   ```bash
   cpack -G DEB  # Creates both runtime and systemd packages
   ```

## Component-Based Packaging

The build creates **two packages** from a single build:

1. **ka9q-radio** (runtime) - Works on any system
2. **ka9q-radio-systemd** - Optional systemd support

This means:
- ✅ Build once on any system (even without systemd)
- ✅ Runtime package works everywhere (systemd, sysvinit, runit, etc.)
- ✅ systemd users can optionally install systemd package
- ✅ No dependency conflicts

See **COMPONENTS.md** for complete details.

## Platform Support Summary

### Linux (Debian/Ubuntu)
✅ Full support
- All features enabled
- Component-based packaging (runtime + optional systemd)
- udev rules
- DEB package generation
- Avahi/mDNS support

**All init systems supported:**
- systemd: Install both packages
- sysvinit/runit/OpenRC: Install runtime package only
- Build once, works everywhere

See **COMPONENTS.md** for packaging details.

### Linux (RPM-based)
✅ Full support
- All features enabled
- systemd integration
- RPM package generation
- Avahi/mDNS support

### FreeBSD
✅ Full support
- rc.d script support
- TXZ package generation
- BSD-specific library paths
- Port-friendly structure

### macOS
⚠️  Partial support
- Client programs (control, monitor)
- Utilities (pcmrecord, pcmplay)
- **Automatic Homebrew/MacPorts detection**
- Limited radiod functionality (no Avahi)
- Hardware drivers if libraries available

## Key Features

### Automatic Detection
- ✅ Available libraries via pkg-config
- ✅ Hardware driver support (airspy, rtl-sdr, hackrf, etc.)
- ✅ systemd availability (Linux)
- ✅ macOS package managers (Homebrew/MacPorts)
- ✅ Platform-specific features

### Package Management
- ✅ Debian (.deb) packages with proper dependencies
- ✅ RPM packages with proper dependencies
- ✅ FreeBSD (.txz) packages
- ✅ Post-install scripts for user creation
- ✅ systemd/rc.d service integration

### Build Options
```bash
-DCMAKE_BUILD_TYPE=Release      # or Debug
-DENABLE_HACKRF=ON              # Hardware drivers
-DENABLE_SDRPLAY=OFF
-DENABLE_FOBOS=OFF
-DCMAKE_INSTALL_PREFIX=/usr/local
```

### macOS Package Manager Support
The build **automatically detects**:
1. Homebrew on Apple Silicon (`/opt/homebrew`)
2. Homebrew on Intel (`/usr/local`)
3. MacPorts (`/opt/local`)

Override if needed:
```bash
cmake .. -DCMAKE_PREFIX_PATH=/opt/local
```

## Integration Steps

### Minimal Integration
1. Copy `CMakeLists.txt` to repository root
2. Adjust source file paths
3. Build and test

### Full Integration
1. Copy all files to appropriate locations:
   ```
   CMakeLists.txt           → Root directory
   systemd/radiod@.service.in → systemd/
   debian/postinst          → debian/
   freebsd/radiod.in        → freebsd/
   ```
2. Adjust source file paths in CMakeLists.txt
3. Test build on target platforms
4. Generate packages with `cpack`

### Documentation Integration
Copy documentation to docs/:
```
CMAKE_README.md    → docs/
QUICKSTART.md      → docs/
MACOS.md          → docs/
```

## Installation Locations

Following standard Filesystem Hierarchy Standard (FHS):

| Content | Location |
|---------|----------|
| Daemons | `/usr/local/sbin/` |
| Programs | `/usr/local/bin/` |
| Libraries | `/usr/local/lib/ka9q-radio/` |
| Support files | `/usr/local/share/ka9q-radio/` |
| Config files | `/etc/radio/` |
| State files | `/var/lib/ka9q-radio/` |
| systemd units | `/etc/systemd/system/` (Linux) |
| rc.d scripts | `/etc/rc.d/` or `/usr/local/etc/rc.d/` (FreeBSD) |

## Dependencies

### Required (all platforms)
- FFTW3 (float version)
- Opus
- libbsd
- iniparser
- libusb
- pthreads
- math library

### Linux-specific
- avahi-client (mDNS)
- systemd (optional, for service files)

### Optional (hardware)
- libairspy
- libairspyhf
- librtlsdr
- libhackrf
- libfobos (manual installation)
- SDRplay API (manual installation)

### Optional (features)
- portaudio (monitor program audio)
- alsa (monitor program audio)
- ncurses (control program UI)
- libogg (oggrecord utility)
- libsamplerate (sample rate conversion)

## Advantages Over Manual Makefiles

1. **Cross-platform**: Single source for Linux/FreeBSD/macOS
2. **Dependency management**: Automatic detection and configuration
3. **Package generation**: Built-in support for multiple formats
4. **Modern tooling**: Better IDE integration
5. **Parallel builds**: Efficient multi-core compilation
6. **Out-of-tree builds**: Keeps source directory clean
7. **Flexible configuration**: Easy enable/disable of features
8. **macOS friendly**: Automatic Homebrew/MacPorts detection

## Maintaining Compatibility

This CMake build system can coexist with existing Makefiles:
- Both install to same locations
- Can be maintained in parallel
- Easy migration path for users
- Fallback if CMake unavailable

## Testing Checklist

Before deployment, test on:
- [ ] Debian/Ubuntu (with and without optional deps)
- [ ] Fedora/RHEL (RPM-based)
- [ ] FreeBSD (latest release)
- [ ] macOS with Homebrew (Intel and Apple Silicon if possible)
- [ ] macOS with MacPorts
- [ ] Package generation on each platform
- [ ] Service startup (systemd/rc.d)
- [ ] Hardware detection (if devices available)

## Contributing Back

Consider submitting this to upstream ka9q-radio:
1. Test thoroughly on multiple platforms
2. Document any platform-specific quirks
3. Create pull request with CMake files
4. Include this summary as documentation

## Support

For issues specific to this CMake build system:
1. Check QUICKSTART.md for integration steps
2. Check CMAKE_README.md for build options
3. Check MACOS.md for macOS-specific issues
4. Review CMakeLists.txt comments

For ka9q-radio itself:
- GitHub: https://github.com/ka9q/ka9q-radio
- Documentation: https://github.com/ka9q/ka9q-radio/tree/main/docs

## Version

CMake Build System v1.0
Created: November 2025
Compatible with: ka9q-radio (current git main)

## License

These CMake files should follow the same license as ka9q-radio itself (GPL-3.0).
