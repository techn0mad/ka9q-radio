# macOS Support in ka9q-radio CMake Build

The CMake build system automatically detects and configures for both Homebrew and MacPorts on macOS.

## Automatic Detection

When you run `cmake ..`, the build system checks for package managers in this order:

1. **Homebrew on Apple Silicon** (`/opt/homebrew`)
2. **Homebrew on Intel** (`/usr/local`)
3. **MacPorts** (`/opt/local`)

The first one found is used. You'll see a message like:
```
-- Detected Homebrew (Apple Silicon) at /opt/homebrew
-- Set PKG_CONFIG_PATH to /opt/homebrew/lib/pkgconfig:/opt/homebrew/share/pkgconfig
```

## Installing Dependencies

### With Homebrew

```bash
# Install Homebrew if needed
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# Install dependencies
brew install cmake fftw opus libbsd iniparser libusb portaudio ncurses pkgconfig

# Optional hardware support (if available)
brew install hackrf airspy rtl-sdr
```

### With MacPorts

```bash
# Install MacPorts from https://www.macports.org/install.php

# Install dependencies
sudo port install cmake fftw-3-single opus libbsd iniparser libusb portaudio ncurses pkgconfig

# Optional hardware support (if available)
sudo port install hackrf airspy rtl-sdr
```

## Building

```bash
cd ka9q-radio
mkdir build && cd build

# Usually just this is enough
cmake ..
cmake --build . -j$(sysctl -n hw.ncpu)

# If auto-detection doesn't work, specify manually
cmake .. -DCMAKE_PREFIX_PATH=/opt/homebrew  # or /opt/local
```

## Mixed Installations

If you have both Homebrew and MacPorts installed:

### Use Homebrew (default)
```bash
cmake ..
```

### Force MacPorts
```bash
cmake .. -DCMAKE_PREFIX_PATH=/opt/local
```

### Force Homebrew
```bash
# Apple Silicon
cmake .. -DCMAKE_PREFIX_PATH=/opt/homebrew

# Intel
cmake .. -DCMAKE_PREFIX_PATH=/usr/local
```

## Limitations on macOS

Some Linux-specific features are unavailable on macOS:

- **No Avahi/mDNS**: The Linux multicast DNS isn't available
- **No systemd**: macOS doesn't use systemd
- **Limited hardware**: Some SDR hardware may have limited macOS support

The build automatically disables these features and will inform you:
```
Features:
  Avahi support: NO  (expected on macOS)
  systemd support: NO  (expected on macOS)
```

## What Works on macOS

You can build and run:
- Client programs: `control`, `monitor`
- Utilities: `pcmrecord`, `pcmplay`, `opussend`
- Some hardware drivers (if libraries available)

The main `radiod` daemon may have limited functionality due to Linux-specific networking features.

## Troubleshooting

### Libraries Not Found

**Problem:**
```
Could NOT find FFTW3F
```

**Solution:**
1. Make sure you installed the dependencies
2. Check which package manager you're using:
   ```bash
   which brew   # Homebrew
   which port   # MacPorts
   ```
3. Manually set the path:
   ```bash
   # For Homebrew
   cmake .. -DCMAKE_PREFIX_PATH=$(brew --prefix)
   
   # For MacPorts
   cmake .. -DCMAKE_PREFIX_PATH=/opt/local
   ```

### Wrong Package Manager Detected

**Problem:** CMake detects Homebrew but you want to use MacPorts

**Solution:**
```bash
# Force MacPorts
cmake .. -DCMAKE_PREFIX_PATH=/opt/local

# Or set environment variable
export CMAKE_PREFIX_PATH=/opt/local
cmake ..
```

### pkgconfig Not Found

**Problem:**
```
Could NOT find PkgConfig (missing: PKG_CONFIG_EXECUTABLE)
```

**Solution:**
```bash
# Homebrew
brew install pkgconfig

# MacPorts
sudo port install pkgconfig
```

### Conflicting Libraries

If you have libraries from both package managers, you might get conflicts.

**Best Practice:** Stick to one package manager for all dependencies.

**Clean Slate:**
```bash
# Remove build directory and start fresh
rm -rf build
mkdir build && cd build

# Explicitly set everything
export CMAKE_PREFIX_PATH=/opt/local  # or /opt/homebrew
export PKG_CONFIG_PATH=/opt/local/lib/pkgconfig
cmake ..
```

## Verifying Your Setup

Check what CMake detected:
```bash
cmake .. 2>&1 | grep -E "(Detected|Package Manager|support:)"
```

Example output:
```
-- Detected Homebrew (Apple Silicon) at /opt/homebrew
-- Package Manager: Homebrew (/opt/homebrew)
-- Airspy support: YES
-- RTL-SDR support: YES
-- HackRF support: NO
-- Avahi support: NO
```

## Notes for Package Maintainers

If you're creating a Homebrew formula or MacPorts Portfile:

### Homebrew Formula Snippet
```ruby
depends_on "cmake" => :build
depends_on "pkg-config" => :build
depends_on "fftw"
depends_on "opus"
depends_on "libbsd"
depends_on "iniparser"
depends_on "libusb"
depends_on "portaudio"
depends_on "ncurses"

def install
  mkdir "build" do
    system "cmake", "..", *std_cmake_args
    system "cmake", "--build", "."
    system "cmake", "--install", "."
  end
end
```

### MacPorts Portfile Snippet
```tcl
depends_build   port:cmake \
                port:pkgconfig

depends_lib     port:fftw-3-single \
                port:opus \
                port:libbsd \
                port:iniparser \
                port:libusb \
                port:portaudio \
                port:ncurses

configure.args  -DCMAKE_PREFIX_PATH=${prefix}
```

## Additional Resources

- [Homebrew Documentation](https://docs.brew.sh/)
- [MacPorts Guide](https://guide.macports.org/)
- [CMake on macOS](https://cmake.org/cmake/help/latest/manual/cmake.1.html)
