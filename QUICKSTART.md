# Quick Start Guide: CMake Build System for ka9q-radio

This guide shows you how to integrate the CMake build system into ka9q-radio.

## Step 1: Prepare the Repository

```bash
# Clone ka9q-radio if you haven't already
git clone https://github.com/ka9q/ka9q-radio.git
cd ka9q-radio

# Copy the CMake files into the repository
# (Assuming you have these files in /path/to/cmake-files/)
cp /path/to/cmake-files/CMakeLists.txt .
cp /path/to/cmake-files/CMAKE_README.md .
mkdir -p systemd debian freebsd
cp /path/to/cmake-files/radiod@.service.in systemd/
cp /path/to/cmake-files/debian-postinst debian/postinst
cp /path/to/cmake-files/freebsd-radiod.in freebsd/radiod.in
```

## Step 2: Adjust for Actual Source Layout

The CMakeLists.txt assumes this structure:
```
ka9q-radio/
├── src/           # Source files (.c)
├── include/       # Header files (.h)
├── config/        # Example config files
├── share/         # Support files (presets.txt, modes.txt)
├── systemd/       # systemd service files
├── udev/          # udev rules
└── debian/        # Debian packaging files
```

**Important**: Check the actual ka9q-radio layout and adjust CMakeLists.txt accordingly.

Looking at the repository, sources appear to be in the root directory, not `src/`.
You'll need to modify the paths:

```cmake
# Change this:
set(LIBRADIO_SOURCES
    src/attr.c
    src/ax25.c
    ...
)

# To this (if sources are in root):
set(LIBRADIO_SOURCES
    attr.c
    ax25.c
    ...
)
```

## Step 3: Verify Source Files

List the actual source files in the repository:

```bash
# List all .c files
ls -1 *.c

# List all .h files  
ls -1 *.h
```

Update CMakeLists.txt with the correct file names.

## Step 4: Build and Test

```bash
# Create build directory
mkdir build
cd build

# Configure - this will show you what's found
cmake ..

# Review the output - it shows:
# - Which libraries were found
# - Which hardware drivers will be built
# - Where files will be installed

# If everything looks good, build
cmake --build . -j$(nproc)

# Test locally (don't install yet)
./radiod --help
```

## Step 5: Installation

```bash
# Install to default location (/usr/local)
sudo cmake --install .

# Or install to a custom prefix
cmake .. -DCMAKE_INSTALL_PREFIX=/opt/ka9q
sudo cmake --install .
```

## Step 6: Create Packages (Optional)

### Debian Package
```bash
cd build
cpack -G DEB
sudo dpkg -i ka9q-radio_*.deb
```

### RPM Package
```bash
cd build
cpack -G RPM
sudo rpm -i ka9q-radio-*.rpm
```

## Common Adjustments You'll Need to Make

### 1. Finding Actual Executables
Look at the existing Makefile to see what executables are built:

```bash
# In ka9q-radio directory
grep "^DAEMONS" Makefile.linux
grep "^APPS" Makefile.linux
```

Update the CMakeLists.txt executable list accordingly.

### 2. Library Dependencies
The build will tell you what's missing:

```
-- Could NOT find AIRSPY (missing: AIRSPY_LIBRARIES AIRSPY_INCLUDE_DIRS)
```

Either install the library or disable the feature:
```bash
cmake .. -DENABLE_AIRSPY=OFF
```

### 3. Platform-Specific Adjustments

**For FreeBSD**, you might need to set:
```bash
cmake .. \
    -DCMAKE_PREFIX_PATH=/usr/local \
    -DPKG_CONFIG_PATH=/usr/local/libdata/pkgconfig
```

**For macOS**, libraries might be in different locations:
```bash
# The build auto-detects your package manager, but you can override:

# Force Homebrew (Apple Silicon)
cmake .. -DCMAKE_PREFIX_PATH=/opt/homebrew

# Force Homebrew (Intel)
cmake .. -DCMAKE_PREFIX_PATH=/usr/local

# Force MacPorts
cmake .. -DCMAKE_PREFIX_PATH=/opt/local
```

The build will display which package manager it detected:
```
-- Detected Homebrew (Apple Silicon) at /opt/homebrew
-- Set PKG_CONFIG_PATH to /opt/homebrew/lib/pkgconfig:/opt/homebrew/share/pkgconfig
```

## Testing the Package

After creating a .deb package:

```bash
# Install in a test environment
dpkg -i ka9q-radio_*.deb

# Check installed files
dpkg -L ka9q-radio

# Check if user was created
id radio

# Check if systemd service is present
systemctl list-unit-files | grep radiod

# Test running
sudo systemctl start radiod@test
systemctl status radiod@test
```

## Troubleshooting

### Source Files Not Found
```
CMake Error: Cannot find source file: src/radiod.c
```
**Fix**: Adjust paths in CMakeLists.txt to match actual layout.

### Library Not Found
```
Could NOT find FFTW3F
```
**Fix**: Install the library or check PKG_CONFIG_PATH.

### Build Errors
```
undefined reference to `avahi_client_new'
```
**Fix**: Add missing library to target_link_libraries in CMakeLists.txt.

## Next Steps

1. **Test on your target platform**: Build and test on the actual system you'll deploy to
2. **Customize the package**: Adjust debian/postinst for your needs
3. **Create a port**: For FreeBSD, create a proper ports Makefile
4. **Contribute back**: Consider submitting CMake support to the upstream project

## Files You've Created

- `CMakeLists.txt` - Main build configuration
- `CMAKE_README.md` - Comprehensive documentation
- `systemd/radiod@.service.in` - systemd service template
- `debian/postinst` - Debian post-install script
- `freebsd/radiod.in` - FreeBSD rc.d script template
- `QUICKSTART.md` - This file

## Resources

- [CMake Tutorial](https://cmake.org/cmake/help/latest/guide/tutorial/)
- [CPack Documentation](https://cmake.org/cmake/help/book/mastering-cmake/chapter/Packaging%20With%20CPack.html)
- [Debian Packaging with CMake](https://www.debian.org/doc/manuals/maint-guide/dreq.en.html)
- [FreeBSD Porter's Handbook](https://docs.freebsd.org/en/books/porters-handbook/)
