# Init System Support in ka9q-radio CMake Build

This document explains how the CMake build handles different init systems, particularly on Debian-based systems.

## Overview

The CMake build system automatically detects the init system at **build time** and adapts accordingly:

- **systemd present**: Installs service files, adds systemd dependency to package
- **systemd absent**: Skips service files, no systemd dependency, provides manual start instructions

## Detection Logic

### At Build Time (cmake ..)

```cmake
# In CMakeLists.txt
if(LINUX)
    pkg_check_modules(SYSTEMD libsystemd)
    if(SYSTEMD_FOUND)
        set(HAVE_SYSTEMD TRUE)
        # Will install systemd service files
        # Will add systemd to package dependencies
    else()
        # Will skip systemd service files
        # Will NOT add systemd dependency
    endif()
endif()
```

The build reports what it found:
```
Features:
  systemd support: YES
```
or
```
Features:
  systemd support: NO
```

### At Install Time (postinst script)

The postinst script also checks at package installation time:

```bash
if command -v systemctl > /dev/null 2>&1 && systemctl is-system-running > /dev/null 2>&1; then
    # systemd is present and running
    systemctl daemon-reload
    echo "Start with: sudo systemctl start radiod@<config>"
else
    # systemd not available
    echo "Start manually: /usr/local/sbin/radiod -v /etc/radio/radiod@<config>.conf"
fi
```

## Use Cases

### Case 1: Building on Debian with systemd for systemd systems

**Most common scenario**

```bash
# On Debian 11+ with systemd
cd ka9q-radio
mkdir build && cd build
cmake ..  # Detects systemd
cmake --build . -j
cpack -G DEB
```

**Result:**
- ✅ systemd service files included
- ✅ Package depends on systemd
- ✅ postinst runs `systemctl daemon-reload`
- ✅ User can `systemctl start radiod@config`

**Package can be installed on:** Debian/Ubuntu systems with systemd only

### Case 2: Building on Debian without systemd (e.g., Devuan)

**For non-systemd distributions**

```bash
# On Devuan, Debian with sysvinit, or similar
cd ka9q-radio
mkdir build && cd build
cmake ..  # Does NOT detect systemd
cmake --build . -j
cpack -G DEB
```

**Result:**
- ❌ No systemd service files
- ❌ No systemd dependency
- ✅ postinst provides manual start instructions
- ✅ User starts with: `/usr/local/sbin/radiod -v /etc/radio/radiod@config.conf`

**Package can be installed on:** Any Debian system (systemd or not)

### Case 3: Building on systemd, installing on non-systemd

**Problem scenario - package built for systemd, installed where systemd isn't available**

If you build a package on a systemd system and try to install it on a non-systemd system:

```bash
# Built on Debian 12 (systemd)
cpack -G DEB
# ka9q-radio_1.0.0_amd64.deb created

# Try to install on Devuan (no systemd)
sudo dpkg -i ka9q-radio_1.0.0_amd64.deb
```

**What happens:**
- ❌ dpkg will complain about missing systemd dependency
- ❌ Package won't install unless you force it (`dpkg -i --force-depends`)

**Solution:** Build separate packages for systemd and non-systemd targets.

### Case 4: Building on non-systemd, installing on systemd

**This works fine!**

```bash
# Built on Devuan (no systemd)
cpack -G DEB
# ka9q-radio_1.0.0_amd64.deb created

# Install on Debian 12 (systemd)
sudo dpkg -i ka9q-radio_1.0.0_amd64.deb
```

**What happens:**
- ✅ Package installs successfully (no systemd dependency)
- ❌ No service files available
- ⚠️  User must start radiod manually or create their own service file

## Best Practices

### For Package Maintainers

**Option 1: Build separate packages**
```bash
# On systemd builder
cmake .. && cpack -G DEB
mv ka9q-radio_1.0.0_amd64.deb ka9q-radio_1.0.0_systemd_amd64.deb

# On non-systemd builder  
cmake .. && cpack -G DEB
mv ka9q-radio_1.0.0_amd64.deb ka9q-radio_1.0.0_nosystemd_amd64.deb
```

**Option 2: Build on non-systemd, document systemd setup**
Build the minimal package (no systemd), then provide separate systemd service files for users to install manually.

**Option 3: Make systemd a Recommends instead of Depends**
Edit CMakeLists.txt:
```cmake
set(CPACK_DEBIAN_PACKAGE_RECOMMENDS "systemd")
# instead of
set(CPACK_DEBIAN_PACKAGE_DEPENDS "${CPACK_DEBIAN_PACKAGE_DEPENDS}, systemd")
```

### For End Users

**If you need systemd support:**
Build on a system that has systemd installed, or install libsystemd-dev before building:
```bash
sudo apt install libsystemd-dev
```

**If you don't want systemd:**
Build on a system without systemd, or explicitly disable it (future enhancement):
```bash
cmake .. -DENABLE_SYSTEMD=OFF  # Not yet implemented
```

## Manual Service Setup

### On systemd systems (if package built without systemd)

Create `/etc/systemd/system/radiod@.service`:
```ini
[Unit]
Description=KA9Q Radio Daemon (%i)
After=network.target avahi-daemon.service

[Service]
Type=simple
User=radio
Group=radio
ExecStart=/usr/local/sbin/radiod -v /etc/radio/radiod@%i.conf
Restart=on-failure

[Install]
WantedBy=multi-user.target
```

Then:
```bash
sudo systemctl daemon-reload
sudo systemctl start radiod@myconfig
```

### On sysvinit systems

Create `/etc/init.d/radiod`:
```bash
#!/bin/sh
### BEGIN INIT INFO
# Provides:          radiod
# Required-Start:    $network $local_fs
# Required-Stop:     $network $local_fs
# Default-Start:     2 3 4 5
# Default-Stop:      0 1 6
# Short-Description: KA9Q Radio Daemon
### END INIT INFO

DAEMON=/usr/local/sbin/radiod
USER=radio
CONFIG=/etc/radio/radiod@hf.conf

case "$1" in
  start)
    start-stop-daemon --start --background --make-pidfile \
      --pidfile /var/run/radiod.pid --chuid $USER \
      --exec $DAEMON -- -v $CONFIG
    ;;
  stop)
    start-stop-daemon --stop --pidfile /var/run/radiod.pid
    ;;
  restart)
    $0 stop
    sleep 1
    $0 start
    ;;
  *)
    echo "Usage: $0 {start|stop|restart}"
    exit 1
    ;;
esac
```

Make executable and enable:
```bash
sudo chmod +x /etc/init.d/radiod
sudo update-rc.d radiod defaults
```

### On runit systems (common in Void Linux, Artix)

Create `/etc/sv/radiod/run`:
```bash
#!/bin/sh
exec chpst -u radio /usr/local/sbin/radiod -v /etc/radio/radiod@hf.conf
```

Enable:
```bash
chmod +x /etc/sv/radiod/run
ln -s /etc/sv/radiod /var/service/
```

## Checking What Your Package Contains

```bash
# Check if systemd files are in the package
dpkg -c ka9q-radio_*.deb | grep systemd

# Check package dependencies
dpkg -I ka9q-radio_*.deb | grep Depends
```

If you see:
- `./etc/systemd/system/radiod@.service` → Package includes systemd
- `Depends: ... systemd` → Package requires systemd

## Future Enhancements

Possible improvements to the build system:

1. **Build option to force systemd on/off:**
   ```bash
   cmake .. -DENABLE_SYSTEMD=OFF
   ```

2. **Multiple init system support:**
   Include service files for systemd, sysvinit, and runit, let the user choose at runtime.

3. **Separate packages:**
   Generate `ka9q-radio-systemd` and `ka9q-radio-nosystemd` automatically.

4. **Recommends instead of Depends:**
   Make systemd a recommendation rather than hard requirement.

## Summary Table

| Build System | Install System | Works? | Notes |
|--------------|----------------|--------|-------|
| systemd | systemd | ✅ Yes | Perfect match |
| systemd | non-systemd | ❌ No | Dependency error |
| non-systemd | systemd | ⚠️  Partial | Works but no service files |
| non-systemd | non-systemd | ✅ Yes | Manual start required |

**Recommendation:** For maximum compatibility, build on a non-systemd system or disable systemd support during build.

## Questions?

- **Q: How do I check if my build has systemd?**
  - A: Look for `systemd support: YES` in the cmake output

- **Q: Can I remove systemd from an already-built package?**
  - A: No, you need to rebuild without systemd present

- **Q: Will this work on Devuan?**
  - A: Yes, if built on Devuan or without systemd. The package will work fine.

- **Q: What about OpenRC (Gentoo)?**
  - A: Currently not supported. Use manual start or create your own OpenRC service.

- **Q: What about Shepherd (Guix)?**
  - A: Currently not supported. Use manual start or create your own Shepherd service.
