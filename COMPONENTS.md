# Component-Based Packaging for ka9q-radio

The CMake build system now supports **component-based packaging**, allowing you to build separate packages for the main runtime and optional systemd support.

## Overview

The build creates two components:
1. **runtime** - Main ka9q-radio programs (required)
2. **systemd** - systemd service files (optional)

This allows:
- Building on non-systemd systems while still providing systemd support
- Creating separate packages that users can install independently
- Maximum compatibility across different init systems

## How It Works

### Single Build, Multiple Packages

```bash
cd ka9q-radio
mkdir build && cd build
cmake ..
cmake --build . -j

# Generate component packages
cpack -G DEB
```

This creates **two** .deb files:
- `ka9q-radio-1.0.0-Linux.deb` - Main runtime package (no systemd dependency)
- `ka9q-radio-systemd-1.0.0-Linux.deb` - systemd support package (depends on runtime + systemd)

## Package Details

### Runtime Package (ka9q-radio)
**Contents:**
- radiod daemon
- Client programs (control, monitor)
- Utilities (pcmrecord, pcmplay, opussend, etc.)
- Configuration files
- Support files
- udev rules

**Dependencies:**
- libfftw3-single3
- libopus0
- libbsd0
- libiniparser1
- libavahi-client3
- Hardware libraries (if enabled)

**Does NOT include:**
- systemd service files
- systemd dependency

### systemd Package (ka9q-radio-systemd)
**Contents:**
- /etc/systemd/system/radiod@.service
- /etc/sysctl.d/98-sockbuf.conf

**Dependencies:**
- ka9q-radio (= 1.0.0)
- systemd

**Purpose:**
Provides systemd integration for systems that use systemd.

## Usage Scenarios

### Scenario 1: Non-systemd Debian System (e.g., Devuan)

```bash
# Install only the runtime package
sudo dpkg -i ka9q-radio-1.0.0-Linux.deb

# Start manually
/usr/local/sbin/radiod -v /etc/radio/radiod@hf.conf

# Or create your own init scripts
```

### Scenario 2: systemd-based Debian System

```bash
# Install runtime package
sudo dpkg -i ka9q-radio-1.0.0-Linux.deb

# Install systemd support
sudo dpkg -i ka9q-radio-systemd-1.0.0-Linux.deb

# Use systemd
sudo systemctl start radiod@hf
sudo systemctl enable radiod@hf
```

### Scenario 3: Building for Both Types of Systems

Build once on any system (even non-systemd):

```bash
# Build (on Devuan, Debian, Ubuntu, anything)
mkdir build && cd build
cmake ..
cmake --build . -j
cpack -G DEB

# Distribute both packages
# Users with systemd install both
# Users without systemd install only runtime
```

## Building Individual Components

### Build Only Runtime Package
```bash
cpack -G DEB -D CPACK_COMPONENTS_ALL=runtime
```
Creates: `ka9q-radio-1.0.0-Linux.deb`

### Build Only systemd Package
```bash
cpack -G DEB -D CPACK_COMPONENTS_ALL=systemd
```
Creates: `ka9q-radio-systemd-1.0.0-Linux.deb`

### Build Both (default)
```bash
cpack -G DEB
```
Creates both packages.

## Installation Options

### Option 1: Install Everything (on systemd systems)
```bash
sudo apt install ./ka9q-radio-1.0.0-Linux.deb ./ka9q-radio-systemd-1.0.0-Linux.deb
```

### Option 2: Install Only Runtime (on any system)
```bash
sudo apt install ./ka9q-radio-1.0.0-Linux.deb
```

### Option 3: Add systemd Support Later
```bash
# Initially installed without systemd
sudo apt install ./ka9q-radio-1.0.0-Linux.deb

# Later, add systemd support
sudo apt install ./ka9q-radio-systemd-1.0.0-Linux.deb
```

### Option 4: Remove systemd Support
```bash
# Remove just the systemd package
sudo apt remove ka9q-radio-systemd

# Runtime continues to work
```

## Advantages

### For Users
- ✅ **Flexibility**: Install only what you need
- ✅ **Compatibility**: Runtime package works everywhere
- ✅ **Clean dependencies**: No forced systemd on non-systemd systems
- ✅ **Easy migration**: Add/remove systemd support without reinstalling main package

### For Package Maintainers
- ✅ **Single build**: Build once, support all systems
- ✅ **No special configurations**: Works on any build system
- ✅ **Simpler testing**: Test each component independently
- ✅ **Distribution-friendly**: Follows Debian policy for optional components

### For Distributions
- ✅ **Repository-friendly**: Can package both or just runtime
- ✅ **Init-agnostic**: Supports systemd, sysvinit, runit, OpenRC
- ✅ **User choice**: Users decide which components to install

## Comparison with Other Approaches

| Approach | Pros | Cons |
|----------|------|------|
| **Single package with systemd** | Simple | Won't install on non-systemd systems |
| **Single package without systemd** | Compatible | systemd users need manual setup |
| **Build-time conditional** | Smaller packages | Need separate builds for each target |
| **Component-based (this)** | Best of all worlds | Slightly more complex packaging |

## Distribution Repository Layout

```
pool/main/k/ka9q-radio/
├── ka9q-radio_1.0.0-1_amd64.deb          # Runtime (required)
└── ka9q-radio-systemd_1.0.0-1_all.deb    # systemd support (optional)
```

Users can:
```bash
# On systemd systems
apt install ka9q-radio ka9q-radio-systemd

# On non-systemd systems  
apt install ka9q-radio

# Or with Recommends
apt install ka9q-radio  # Pulls in ka9q-radio-systemd if systemd present
```

## Testing

### Test Runtime Package Alone
```bash
# On non-systemd system (or VM)
sudo dpkg -i ka9q-radio-1.0.0-Linux.deb
dpkg -L ka9q-radio  # Should show no systemd files
radiod --help  # Should work
```

### Test systemd Package
```bash
# On systemd system
sudo dpkg -i ka9q-radio-1.0.0-Linux.deb
sudo dpkg -i ka9q-radio-systemd-1.0.0-Linux.deb
systemctl cat radiod@  # Should show service file
```

### Test Dependency
```bash
# Try to install systemd package without runtime
sudo dpkg -i ka9q-radio-systemd-1.0.0-Linux.deb
# Should fail with dependency error
```

## Package Metadata

### Runtime Package Control File
```
Package: ka9q-radio
Version: 1.0.0
Architecture: amd64
Depends: libfftw3-single3, libopus0, libbsd0, ...
Description: Multichannel SDR based on fast convolution
 Main runtime package without init system dependencies.
```

### systemd Package Control File
```
Package: ka9q-radio-systemd
Version: 1.0.0
Architecture: all
Depends: ka9q-radio (= 1.0.0), systemd
Description: systemd integration for ka9q-radio
 Provides systemd service files for ka9q-radio daemon.
```

## For Package Maintainers: Debian Repository

### Option 1: Both packages available
```bash
# Users on systemd
apt install ka9q-radio ka9q-radio-systemd

# Users on other init systems
apt install ka9q-radio
```

### Option 2: Use Recommends
Modify CMakeLists.txt:
```cmake
set(CPACK_DEBIAN_RUNTIME_PACKAGE_RECOMMENDS "ka9q-radio-systemd")
```

Then systemd users get both automatically:
```bash
apt install ka9q-radio  # Pulls in ka9q-radio-systemd by default
```

Non-systemd users can prevent it:
```bash
apt install --no-install-recommends ka9q-radio
```

## Migration from Old Packages

### From monolithic systemd package
```bash
# Users on systemd
apt install ka9q-radio ka9q-radio-systemd  # Same functionality

# Users on non-systemd
apt install ka9q-radio  # Now works!
```

### From monolithic non-systemd package
```bash
# All users
apt install ka9q-radio  # Same functionality

# systemd users can add
apt install ka9q-radio-systemd  # Adds systemd support
```

## RPM Support

Component-based packaging also works with RPM:

```bash
cpack -G RPM
```

Creates:
- `ka9q-radio-1.0.0-1.x86_64.rpm` - Runtime
- `ka9q-radio-systemd-1.0.0-1.noarch.rpm` - systemd support

## Future Enhancements

Potential additional components:
- `ka9q-radio-sysvinit` - sysvinit scripts
- `ka9q-radio-runit` - runit services  
- `ka9q-radio-openrc` - OpenRC scripts
- `ka9q-radio-doc` - Documentation
- `ka9q-radio-dev` - Development headers

## Summary

Component-based packaging gives you:
- ✅ Build once on any system
- ✅ Support all init systems
- ✅ Let users choose what to install
- ✅ Clean dependencies
- ✅ Professional packaging

This is the **recommended approach** for maximum compatibility and flexibility.
