# RPM Packaging Files

This directory contains files for creating RPM packages (Fedora, RHEL, CentOS, openSUSE, etc.).

## Files

### postinstall
Post-installation script for the runtime package (`ka9q-radio`).

**Actions performed:**
- Creates `radio` system group
- Creates `radio` system user
- Sets ownership of `/var/lib/ka9q-radio`
- Sets ownership of `/etc/radio` config directory
- Reloads systemd (if present)
- Applies sysctl settings

**Differences from Debian:**
- Uses `groupadd -r` instead of `addgroup --system`
- Uses `useradd -r` instead of `adduser --system`
- Uses `/sbin/nologin` instead of `/usr/sbin/nologin`

### systemd-postinstall
Post-installation script for the systemd component package (`ka9q-radio-systemd`).

**Actions performed:**
- Reloads systemd daemon
- Displays usage instructions

## Package Structure

The CMake build creates TWO RPM packages:

### ka9q-radio (runtime)
```
Name:         ka9q-radio
Architecture: x86_64 (or appropriate)
Depends:      fftw-libs-single, opus, libbsd, iniparser, avahi-libs
Contents:     radiod, control, monitor, utilities, configs
```

### ka9q-radio-systemd
```
Name:         ka9q-radio-systemd
Architecture: noarch
Depends:      ka9q-radio (exact version), systemd
Contents:     radiod@.service, sysctl configs
```

## Building RPM Packages

```bash
# In build directory
cpack -G RPM

# Results in:
# - ka9q-radio-1.0.0-1.x86_64.rpm (runtime)
# - ka9q-radio-systemd-1.0.0-1.noarch.rpm (systemd)
```

## Installation

### On systemd-based systems (Fedora 15+, RHEL 7+, openSUSE 12.1+)
```bash
sudo rpm -i ka9q-radio-1.0.0-1.x86_64.rpm
sudo rpm -i ka9q-radio-systemd-1.0.0-1.noarch.rpm

# Start service
sudo systemctl start radiod@hf
sudo systemctl enable radiod@hf
```

### On non-systemd systems (older versions)
```bash
# Install runtime only
sudo rpm -i ka9q-radio-1.0.0-1.x86_64.rpm

# Start manually
/usr/local/sbin/radiod -v /etc/radio/radiod@hf.conf
```

## Verification

```bash
# Check installed files
rpm -ql ka9q-radio
rpm -ql ka9q-radio-systemd

# Check dependencies
rpm -qR ka9q-radio
rpm -qR ka9q-radio-systemd

# Verify installation
rpm -V ka9q-radio
```

## Distribution Notes

### Fedora
- systemd standard since Fedora 15
- Use `dnf install` instead of `rpm -i`
- Packages can be submitted to Fedora repos

### RHEL/CentOS
- systemd standard since version 7
- RHEL 6 and earlier use upstart/sysvinit
- May need EPEL repo for some dependencies

### openSUSE
- systemd standard since 12.1
- Use `zypper install` instead of `rpm -i`
- Different dependency names (e.g., libfftw3f0)

## Troubleshooting

### "Failed dependencies" error
Some dependencies may have different names on different distros:
```bash
# Fedora
sudo dnf install fftw-libs-single opus libbsd iniparser avahi-libs

# openSUSE (names may differ)
sudo zypper install libfftw3f0 libopus0 libbsd0 iniparser avahi
```

### User/group creation fails
The scripts check if the user/group exists before creating.
If you see errors, check `/etc/passwd` and `/etc/group`.

### systemd not reloading
Ensure systemd is running and you have proper permissions:
```bash
systemctl is-system-running
sudo systemctl daemon-reload
```

## Advanced: Building from SPEC

If you prefer traditional RPM building:

```bash
# Create spec file (not included, but can be generated)
rpmbuild -ba ka9q-radio.spec
```

The CMake-based approach is simpler and cross-platform.

## References

- RPM Packaging Guide: https://rpm-packaging-guide.github.io/
- Fedora Packaging Guidelines: https://docs.fedoraproject.org/en-US/packaging-guidelines/
- openSUSE Build Service: https://build.opensuse.org/
