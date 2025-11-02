# Repository Directory Structure for ka9q-radio CMake Build

This document describes the recommended directory structure for integrating the CMake build system into ka9q-radio.

## Complete Directory Structure

```
ka9q-radio/
├── CMakeLists.txt                  # Main CMake configuration
├── LICENSE                          # GPL-3.0 license
├── README.md                        # Project README
│
├── src/                             # Source files (adjust paths in CMakeLists.txt)
│   ├── radiod.c
│   ├── control.c
│   ├── monitor.c
│   └── ...
│
├── include/                         # Header files (adjust paths in CMakeLists.txt)
│   ├── radio.h
│   ├── filter.h
│   └── ...
│
├── config/                          # Example configuration files
│   ├── radiod@hf.conf.example
│   ├── radiod@vhf.conf.example
│   └── ...
│
├── share/                           # Support files
│   ├── presets.txt
│   ├── modes.txt
│   └── bandplan.txt
│
├── docs/                            # CMake build documentation
│   ├── CMAKE_README.md
│   ├── COMPONENTS.md
│   ├── COMPONENT_GUIDE.md
│   ├── INIT_SYSTEMS.md
│   ├── MACOS.md
│   ├── QUICKSTART.md
│   └── FILE_STRUCTURE.txt
│
├── debian/                          # Debian packaging files
│   ├── postinst                     # Runtime package post-install
│   ├── prerm                        # Pre-removal script
│   ├── systemd-postinst             # systemd component post-install
│   └── README.md                    # Debian packaging notes
│
├── rpm/                             # RPM packaging files
│   ├── postinstall                  # Runtime package post-install
│   ├── systemd-postinstall          # systemd component post-install
│   └── README.md                    # RPM packaging notes
│
├── systemd/                         # systemd service files
│   ├── radiod@.service.in           # Service template (configured by CMake)
│   ├── 98-sockbuf.conf              # Kernel tuning parameters
│   └── README.md                    # systemd service notes
│
├── freebsd/                         # FreeBSD packaging files
│   ├── radiod.in                    # rc.d script template
│   ├── pkg-plist                    # Package file list (optional)
│   └── README.md                    # FreeBSD packaging notes
│
├── macos/                           # macOS packaging files
│   ├── homebrew/
│   │   ├── ka9q-radio.rb            # Homebrew formula
│   │   └── README.md                # Homebrew packaging notes
│   │
│   └── macports/
│       ├── Portfile                 # MacPorts Portfile
│       └── README.md                # MacPorts packaging notes
│
├── udev/                            # udev rules for hardware
│   ├── 52-airspy.rules
│   ├── 52-hackrf.rules
│   ├── 52-rtlsdr.rules
│   └── README.md
│
└── build/                           # Build directory (created by user, not in repo)
    ├── ...                          # CMake build artifacts
    └── *.deb, *.rpm                 # Generated packages
```

## Directory Purposes

### Core Directories

**debian/**
- Purpose: Debian/Ubuntu package control files
- Required: Yes (for DEB packages)
- Contents: Post-install scripts, pre-removal scripts
- See: `debian/README.md` for details

**rpm/**
- Purpose: RPM package control files (Fedora, RHEL, openSUSE)
- Required: Yes (for RPM packages)
- Contents: Post-install scripts for RPM-based systems
- See: `rpm/README.md` for details

**systemd/**
- Purpose: systemd service files and configuration
- Required: Yes (for Linux with systemd)
- Contents: Service templates, sysctl configs
- Note: Part of systemd component package

**freebsd/**
- Purpose: FreeBSD-specific files
- Required: Yes (for FreeBSD packages)
- Contents: rc.d scripts, pkg files
- See: `freebsd/README.md` for details

**macos/**
- Purpose: macOS package manager integration
- Required: Optional (for Homebrew/MacPorts distribution)
- Contents: Formula/Portfile templates
- Subdirectories: `homebrew/`, `macports/`

**udev/**
- Purpose: Hardware access rules for USB devices
- Required: Yes (for hardware support on Linux)
- Contents: udev rules files

**docs/**
- Purpose: CMake build system documentation
- Required: Highly recommended
- Contents: All CMake-related documentation

## Minimal Setup

If you want to start with the absolute minimum:

```
ka9q-radio/
├── CMakeLists.txt
├── systemd/
│   └── radiod@.service.in
├── debian/
│   ├── postinst
│   └── systemd-postinst
└── freebsd/
    └── radiod.in
```

This is enough to build and create basic packages. Add others as needed.

## Creating the Directories

```bash
cd ka9q-radio

# Create core packaging directories
mkdir -p debian rpm systemd freebsd udev

# Create macOS directories
mkdir -p macos/homebrew macos/macports

# Create documentation directory (optional)
mkdir -p docs

# Copy files from CMake build system package
cp /path/to/debian-postinst debian/postinst
cp /path/to/debian-systemd-postinst debian/systemd-postinst
cp /path/to/rpm-postinstall rpm/postinstall
cp /path/to/rpm-systemd-postinstall rpm/systemd-postinstall
cp /path/to/radiod@.service.in systemd/
cp /path/to/freebsd-radiod.in freebsd/radiod.in
cp /path/to/macos-homebrew-formula.rb macos/homebrew/ka9q-radio.rb
cp /path/to/macos-macports-portfile macos/macports/Portfile

# Copy documentation (optional but recommended)
cp /path/to/*.md docs/

# Make scripts executable
chmod +x debian/postinst debian/systemd-postinst
chmod +x rpm/postinstall rpm/systemd-postinstall
```

## README Files for Each Directory

Each packaging directory should have a README explaining its purpose:

### debian/README.md
```markdown
# Debian Packaging Files

- `postinst` - Runtime package post-installation
- `systemd-postinst` - systemd component post-installation
- `prerm` - Pre-removal script (optional)

These scripts handle:
- User/group creation
- Permission setup
- systemd integration
- Configuration directory setup
```

### rpm/README.md
```markdown
# RPM Packaging Files

- `postinstall` - Runtime package post-installation
- `systemd-postinstall` - systemd component post-installation

Similar to Debian scripts but using RPM conventions:
- `useradd` instead of `adduser`
- Different group creation syntax
```

### systemd/README.md
```markdown
# systemd Service Files

- `radiod@.service.in` - Service template (@ enables instances)
- `98-sockbuf.conf` - Kernel socket buffer tuning

The .in file is processed by CMake to substitute paths.
Part of the ka9q-radio-systemd component package.
```

### freebsd/README.md
```markdown
# FreeBSD Packaging Files

- `radiod.in` - rc.d service script template
- `pkg-plist` - Package file list (optional)

FreeBSD uses rc.d for service management.
The script supports multiple instances like systemd.
```

### macos/homebrew/README.md
```markdown
# Homebrew Formula

Formula for installing ka9q-radio via Homebrew.

To use:
1. Update version and checksums
2. Submit to homebrew-core or create a tap
3. Users install with: `brew install ka9q-radio`
```

### macos/macports/README.md
```markdown
# MacPorts Portfile

Portfile for installing ka9q-radio via MacPorts.

To use:
1. Update version and checksums
2. Submit to MacPorts or add to local ports tree
3. Users install with: `sudo port install ka9q-radio`
```

## .gitignore Recommendations

Add to `.gitignore`:
```
# CMake build artifacts
build/
CMakeCache.txt
CMakeFiles/
cmake_install.cmake
Makefile
*.cmake
!CMakeLists.txt

# Generated files
*.deb
*.rpm
*.txz
*.tar.gz

# Compiled binaries
radiod
control
monitor
*.o
*.a
*.so

# Package build directories
debian/ka9q-radio/
debian/.debhelper/
debian/files
debian/*.log
debian/*.substvars
```

## Integration Checklist

- [ ] Create directory structure
- [ ] Copy CMakeLists.txt to root
- [ ] Copy packaging files to appropriate directories
- [ ] Adjust source paths in CMakeLists.txt if needed
- [ ] Create README.md in each packaging directory
- [ ] Update .gitignore
- [ ] Test build: `mkdir build && cd build && cmake ..`
- [ ] Test package: `cpack -G DEB`
- [ ] Verify package contents: `dpkg -c *.deb`
- [ ] Test installation in VM/container

## Version Control

### What to commit:
✅ CMakeLists.txt
✅ All packaging scripts (debian/, rpm/, systemd/, freebsd/, macos/)
✅ Documentation (docs/)
✅ README files in each directory
✅ Templates (.in files)

### What NOT to commit:
❌ build/ directory
❌ Generated packages (.deb, .rpm, .txz)
❌ CMake cache and artifacts
❌ Compiled binaries

## Platform-Specific Notes

### Debian/Ubuntu
- Requires: `debian/` directory
- Optional: `debian/prerm`, `debian/postrm`
- Package names follow Debian naming conventions

### Fedora/RHEL
- Requires: `rpm/` directory
- Uses different user management commands
- systemd is standard on modern versions

### FreeBSD
- Requires: `freebsd/` directory
- Uses rc.d for services
- Package format is .txz (tar + xz)

### macOS
- Optional: `macos/` directory for distribution
- Homebrew is more common than MacPorts
- Limited hardware support (no native USB SDR libs)

## Maintenance

When updating ka9q-radio:
1. Update version in CMakeLists.txt
2. Update changelogs if present
3. Test build on each platform
4. Regenerate packages
5. Update formulas/portfiles with new checksums

## References

- Debian Packaging: https://www.debian.org/doc/manuals/maint-guide/
- RPM Packaging: https://rpm-packaging-guide.github.io/
- FreeBSD Porter's Handbook: https://docs.freebsd.org/en/books/porters-handbook/
- Homebrew Formula Cookbook: https://docs.brew.sh/Formula-Cookbook
- MacPorts Portfile Guide: https://guide.macports.org/

## Questions?

See the main documentation:
- START_HERE.md - Overview
- QUICKSTART.md - Integration steps
- COMPONENTS.md - Component packaging details
