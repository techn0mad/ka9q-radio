# Complete Package: ka9q-radio CMake Build System with Directory Structure

## Overview

This package provides a complete, production-ready CMake build system for ka9q-radio with:
- ✅ Component-based packaging (runtime + optional systemd)
- ✅ Support for all major platforms and package formats
- ✅ Comprehensive directory structure
- ✅ Platform-specific packaging templates
- ✅ Extensive documentation

## What's Included

### Core Build System (4 files)
1. **CMakeLists.txt** (15KB) - Main build configuration
2. **debian-postinst** (2.2KB) - Debian runtime installer
3. **debian-systemd-postinst** (1.1KB) - Debian systemd installer
4. **radiod@.service.in** (628B) - systemd service template

### RPM Support (3 files) ⭐ NEW
5. **rpm-postinstall** (1.9KB) - RPM runtime installer
6. **rpm-systemd-postinstall** (1KB) - RPM systemd installer
7. **rpm-README.md** (4.2KB) - RPM packaging guide

### macOS Support (2 files) ⭐ NEW
8. **macos-homebrew-formula.rb** (1.3KB) - Homebrew formula template
9. **macos-macports-portfile** (1.5KB) - MacPorts Portfile template

### FreeBSD Support (1 file)
10. **freebsd-radiod.in** (3KB) - FreeBSD rc.d script

### Documentation (10 files)
11. **START_HERE.md** (8.9KB) - New user introduction
12. **COMPONENT_GUIDE.md** (13KB) - Visual packaging guide
13. **COMPONENTS.md** (7.9KB) - Component packaging details
14. **CMAKE_README.md** (9.2KB) - Complete build reference
15. **INIT_SYSTEMS.md** (8KB) - Init system support guide
16. **QUICKSTART.md** (5.5KB) - Integration steps
17. **MACOS.md** (5.2KB) - macOS specifics
18. **README.md** (8.4KB) - File overview
19. **DIRECTORY_STRUCTURE.md** (9KB) ⭐ NEW - Directory layout guide
20. **FILE_STRUCTURE.txt** (11KB) - Visual file map

**Total: 20 files, ~100KB of build system and documentation**

## Recommended Directory Structure

```
ka9q-radio/
├── CMakeLists.txt              # Main build config
│
├── debian/                     # Debian/Ubuntu packaging
│   ├── postinst
│   ├── systemd-postinst
│   └── README.md               # (create based on rpm-README.md template)
│
├── rpm/                        # RPM packaging ⭐ NEW
│   ├── postinstall
│   ├── systemd-postinstall
│   └── README.md
│
├── systemd/                    # systemd service files
│   ├── radiod@.service.in
│   ├── 98-sockbuf.conf        # (if you have one)
│   └── README.md               # (create)
│
├── freebsd/                    # FreeBSD packaging
│   ├── radiod.in
│   └── README.md               # (create)
│
├── macos/                      # macOS packaging ⭐ NEW
│   ├── homebrew/
│   │   ├── ka9q-radio.rb
│   │   └── README.md           # (create)
│   └── macports/
│       ├── Portfile
│       └── README.md           # (create)
│
├── udev/                       # Hardware rules
│   └── *.rules
│
└── docs/                       # CMake documentation
    ├── START_HERE.md
    ├── CMAKE_README.md
    ├── COMPONENTS.md
    ├── COMPONENT_GUIDE.md
    ├── INIT_SYSTEMS.md
    ├── QUICKSTART.md
    ├── MACOS.md
    ├── DIRECTORY_STRUCTURE.md
    └── FILE_STRUCTURE.txt
```

## Key Features

### 1. Component-Based Packaging
Build once, create multiple packages:
- `ka9q-radio` - Runtime (works everywhere)
- `ka9q-radio-systemd` - Optional systemd support

### 2. Platform Support
- ✅ Debian/Ubuntu (.deb)
- ✅ Fedora/RHEL/CentOS (.rpm) ⭐ Enhanced
- ✅ FreeBSD (.txz)
- ✅ macOS (Homebrew/MacPorts) ⭐ Enhanced

### 3. Init System Agnostic
- systemd (most modern Linux)
- sysvinit (older Debian/Ubuntu)
- runit (Void Linux, Artix)
- OpenRC (Gentoo)
- rc.d (FreeBSD)

### 4. macOS Package Managers
- Homebrew (auto-detection)
- MacPorts (auto-detection)
- Both Intel and Apple Silicon

## Quick Start

### 1. Create Directory Structure
```bash
cd ka9q-radio

# Create packaging directories
mkdir -p debian rpm systemd freebsd macos/homebrew macos/macports docs udev

# Copy files from this package
cp /path/to/CMakeLists.txt .
cp /path/to/debian-* debian/
cp /path/to/rpm-* rpm/
cp /path/to/radiod@.service.in systemd/
cp /path/to/freebsd-radiod.in freebsd/radiod.in
cp /path/to/macos-homebrew-formula.rb macos/homebrew/ka9q-radio.rb
cp /path/to/macos-macports-portfile macos/macports/Portfile
cp /path/to/*.md docs/

# Make scripts executable
chmod +x debian/postinst debian/systemd-postinst
chmod +x rpm/postinstall rpm/systemd-postinstall
```

### 2. Build
```bash
mkdir build && cd build
cmake ..
cmake --build . -j
```

### 3. Create Packages
```bash
# Debian packages
cpack -G DEB

# RPM packages
cpack -G RPM

# FreeBSD packages
cpack -G TXZ
```

## Package Outputs

### Debian/Ubuntu
```
ka9q-radio_1.0.0_amd64.deb       # Runtime
ka9q-radio-systemd_1.0.0_all.deb # systemd support
```

### Fedora/RHEL/CentOS
```
ka9q-radio-1.0.0-1.x86_64.rpm    # Runtime
ka9q-radio-systemd-1.0.0-1.noarch.rpm # systemd support
```

### FreeBSD
```
ka9q-radio-1.0.0-FreeBSD.txz     # Combined package
```

### macOS (via package managers)
```bash
# Homebrew
brew install ka9q-radio

# MacPorts
sudo port install ka9q-radio
```

## Installation Examples

### systemd-based Linux (Debian 11+, Ubuntu 20.04+, Fedora, RHEL 7+)
```bash
# Debian/Ubuntu
sudo apt install ./ka9q-radio_*.deb ./ka9q-radio-systemd_*.deb

# Fedora/RHEL
sudo dnf install ./ka9q-radio-*.rpm ./ka9q-radio-systemd-*.rpm

# Start service
sudo systemctl start radiod@hf
sudo systemctl enable radiod@hf
```

### Non-systemd Linux (Devuan, Void Linux)
```bash
# Install runtime only
sudo dpkg -i ka9q-radio_1.0.0_amd64.deb

# Start manually
/usr/local/sbin/radiod -v /etc/radio/radiod@hf.conf
```

### FreeBSD
```bash
sudo pkg add ka9q-radio-1.0.0-FreeBSD.txz
sudo service radiod start
```

### macOS
```bash
# Via Homebrew
brew install ka9q-radio
/usr/local/sbin/radiod -v /usr/local/etc/radio/radiod@hf.conf

# Via MacPorts
sudo port install ka9q-radio
/opt/local/sbin/radiod -v /opt/local/etc/radio/radiod@hf.conf
```

## What Makes This Special

### Previous Approaches (Problems)
1. **Monolithic with systemd** - Won't install on non-systemd systems
2. **Monolithic without systemd** - systemd users need manual setup
3. **Build-time conditional** - Need separate builds for each target
4. **Manual Makefiles** - Platform-specific, hard to maintain

### This Approach (Solutions)
1. ✅ **Component-based** - Build once, works everywhere
2. ✅ **Optional systemd** - Users choose what to install
3. ✅ **Single build** - One build supports all systems
4. ✅ **CMake** - Cross-platform, modern, maintainable
5. ✅ **Directory structure** - Organized, extensible ⭐ NEW
6. ✅ **RPM support** - First-class RPM packaging ⭐ NEW
7. ✅ **macOS templates** - Ready for distribution ⭐ NEW

## Documentation Roadmap

**New users:**
1. START_HERE.md - Get oriented
2. DIRECTORY_STRUCTURE.md - Understand layout ⭐
3. QUICKSTART.md - Integrate into ka9q-radio

**Package maintainers:**
1. COMPONENTS.md - Understand component packaging
2. COMPONENT_GUIDE.md - Visual reference
3. Platform-specific READMEs in debian/, rpm/, macos/

**Platform-specific:**
- Debian/Ubuntu: debian/README.md (create from template)
- RPM: rpm-README.md ⭐
- macOS: MACOS.md
- FreeBSD: DIRECTORY_STRUCTURE.md
- Init systems: INIT_SYSTEMS.md

**Reference:**
- CMAKE_README.md - Complete build options
- FILE_STRUCTURE.txt - File organization
- README.md - File listing

## New in This Version

### RPM Support ⭐
- rpm-postinstall script
- rpm-systemd-postinstall script
- Component-based RPM packaging
- rpm-README.md documentation
- CMakeLists.txt RPM configuration

### macOS Templates ⭐
- Homebrew formula template
- MacPorts Portfile template
- Ready for submission to package managers
- Hardware variant support

### Directory Structure ⭐
- DIRECTORY_STRUCTURE.md guide
- Recommended layout for all platforms
- README templates for each directory
- .gitignore recommendations

### Enhanced Documentation ⭐
- More comprehensive platform coverage
- Better organization
- Platform-specific guides
- Integration checklists

## Platform Matrix

| Platform | Package Format | Init System | Status |
|----------|---------------|-------------|---------|
| Debian 11+ | .deb | systemd | ✅ Full |
| Ubuntu 20.04+ | .deb | systemd | ✅ Full |
| Devuan | .deb | sysvinit/runit | ✅ Full |
| Fedora | .rpm | systemd | ✅ Full ⭐ |
| RHEL/CentOS 7+ | .rpm | systemd | ✅ Full ⭐ |
| openSUSE | .rpm | systemd | ✅ Full ⭐ |
| FreeBSD | .txz | rc.d | ✅ Full |
| macOS (Homebrew) | formula | manual | ✅ Ready ⭐ |
| macOS (MacPorts) | Portfile | manual | ✅ Ready ⭐ |

## Files Summary

| Category | Count | Purpose |
|----------|-------|---------|
| Build system | 1 | CMakeLists.txt |
| Debian packaging | 2 | postinst scripts |
| RPM packaging ⭐ | 3 | postinst scripts + README |
| systemd | 1 | Service template |
| FreeBSD | 1 | rc.d script |
| macOS ⭐ | 2 | Formula + Portfile templates |
| Documentation | 10 | Guides and references |
| **Total** | **20** | Complete build system |

## Next Steps

1. **Review** DIRECTORY_STRUCTURE.md for layout
2. **Copy** files to ka9q-radio repository
3. **Create** directory structure
4. **Test** build on your platform
5. **Generate** packages with cpack
6. **Verify** package contents
7. **Install** and test
8. **Distribute** via appropriate channels

## Support & Resources

- GitHub: https://github.com/ka9q/ka9q-radio
- Debian Policy: https://www.debian.org/doc/debian-policy/
- RPM Packaging: https://rpm-packaging-guide.github.io/
- FreeBSD Handbook: https://docs.freebsd.org/en/books/porters-handbook/
- Homebrew Docs: https://docs.brew.sh/
- MacPorts Guide: https://guide.macports.org/

## Version

CMake Build System v1.0 with Directory Structure
November 2025
License: GPL-3.0 (same as ka9q-radio)

---

**Ready to integrate?** See START_HERE.md and DIRECTORY_STRUCTURE.md!
