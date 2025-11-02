# Migrating from Static systemd Services to Template Units

## Problem Analysis

### Current Structure (Problematic)
```
service/
├── radiod@hf.service          # Static file for HF
├── radiod@vhf.service         # Static file for VHF  
├── radiod@uhf.service         # Static file for UHF
├── radiod@packet.service      # Static file for packet
└── ...                        # Many more static files
```

**Issues:**
1. Creates hard systemd dependency
2. Files not configured by CMake (hardcoded paths)
3. Duplicates configuration
4. Requires new file for each config
5. Not user-extensible

### Proposed Structure (Better)
```
systemd/
└── radiod@.service.in         # Single template, configured by CMake
```

**Benefits:**
- ✅ Single template file
- ✅ CMake-configured paths
- ✅ Works with ANY config name
- ✅ Part of optional systemd component
- ✅ Users can use their own configs

## How systemd Template Units Work

### Template Unit Syntax
A service file named `radiod@.service` (note the `@` with no instance name) is a **template**.

### Instance Creation
Users create instances by specifying a name after the `@`:
```bash
systemctl start radiod@hf       # Uses radiod@hf.conf
systemctl start radiod@vhf      # Uses radiod@vhf.conf
systemctl start radiod@myconfig # Uses radiod@myconfig.conf
```

### Inside the Service File
The special variable `%i` gets replaced with the instance name:
```ini
[Service]
ExecStart=/usr/local/sbin/radiod -v /etc/radio/radiod@%i.conf
#                                                         ^^
#                                    Replaced with instance name
```

## Migration Steps

### Step 1: Create Single Template File

Replace all static service files with one template:

**systemd/radiod@.service.in**
```ini
[Unit]
Description=KA9Q Radio Daemon (%i)
Documentation=https://github.com/ka9q/ka9q-radio
After=network.target avahi-daemon.service
Wants=network.target

[Service]
Type=simple
User=radio
Group=radio

# %i is replaced with the instance name
ExecStart=@KA9Q_SBIN_DIR@/radiod -v @KA9Q_CONFIG_DIR@/radiod@%i.conf

Restart=on-failure
RestartSec=10

# Resource limits
LimitMEMLOCK=infinity
LimitRTPRIO=99

# Security hardening
ProtectSystem=strict
ProtectHome=yes
ReadWritePaths=@KA9Q_STATE_DIR@
NoNewPrivileges=true
PrivateTmp=true

# Device access
DeviceAllow=/dev/bus/usb

[Install]
WantedBy=multi-user.target
```

**Key points:**
- `%i` = instance name (hf, vhf, uhf, etc.)
- `@KA9Q_SBIN_DIR@` = Configured by CMake
- `.in` extension = Template for CMake configuration

### Step 2: Update CMakeLists.txt

The CMakeLists.txt already handles this correctly:

```cmake
# This is already in your CMakeLists.txt
if(LINUX)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/systemd/radiod@.service.in")
        configure_file(
            systemd/radiod@.service.in
            ${CMAKE_CURRENT_BINARY_DIR}/radiod@.service
            @ONLY
        )
        install(FILES ${CMAKE_CURRENT_BINARY_DIR}/radiod@.service
            DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/systemd/system
            COMPONENT systemd
        )
    endif()
endif()
```

### Step 3: Remove Old Service Directory

```bash
cd ka9q-radio

# Back up old service files (just in case)
mv service service.old

# The systemd/ directory with template already exists
# No action needed - CMake will use it
```

### Step 4: Update Documentation

**Old way (in README):**
```
systemctl start radiod@hf
systemctl start radiod@vhf
systemctl start radiod@uhf
```

**New way (same commands!):**
```
# Works with ANY config name you create
systemctl start radiod@hf
systemctl start radiod@myconfig
systemctl start radiod@whatever
```

## Configuration File Mapping

### Automatic Mapping
systemd automatically maps instance names to config files:

| Command | Config File Used |
|---------|------------------|
| `systemctl start radiod@hf` | `/etc/radio/radiod@hf.conf` |
| `systemctl start radiod@vhf` | `/etc/radio/radiod@vhf.conf` |
| `systemctl start radiod@test` | `/etc/radio/radiod@test.conf` |
| `systemctl start radiod@n` | `/etc/radio/radiod@n.conf` |

### Example Configs
Users just create config files with matching names:
```
/etc/radio/
├── radiod@hf.conf
├── radiod@vhf.conf
├── radiod@uhf.conf
└── radiod@custom.conf
```

Then start them:
```bash
systemctl start radiod@hf
systemctl start radiod@custom
```

## Benefits of Template Units

### 1. User Flexibility
Users can create unlimited instances:
```bash
# Create custom config
sudo nano /etc/radio/radiod@mysdr.conf

# Start it
sudo systemctl start radiod@mysdr
```

### 2. No File Proliferation
One template file vs. dozens of static files.

### 3. CMake Configuration
Paths are configured at build time:
```ini
# Before CMake (in .in file)
ExecStart=@KA9Q_SBIN_DIR@/radiod

# After CMake (in .service file)
ExecStart=/usr/local/sbin/radiod
```

### 4. Component Packaging
Template goes in optional systemd component package.

## Handling Example Configs

### Option 1: Ship Example Configs (Recommended)
```
config/
├── radiod@hf.conf.example
├── radiod@vhf.conf.example
└── radiod@uhf.conf.example
```

Users copy and customize:
```bash
cd /etc/radio
sudo cp radiod@hf.conf.example radiod@hf.conf
sudo nano radiod@hf.conf
systemctl start radiod@hf
```

### Option 2: Install Default Configs
```cmake
# In CMakeLists.txt
install(DIRECTORY config/
    DESTINATION ${KA9Q_CONFIG_DIR}
    FILES_MATCHING PATTERN "*.conf"
    PATTERN "*.conf.example"
    COMPONENT runtime
)
```

Then users just start the instances:
```bash
systemctl start radiod@hf   # Uses pre-installed config
```

## Advanced: Multiple Config Locations

If you want to support configs in different locations:

**radiod@.service.in**
```ini
[Service]
# Try multiple locations
ExecStart=/bin/bash -c '\
    if [ -f @KA9Q_CONFIG_DIR@/radiod@%i.conf ]; then \
        exec @KA9Q_SBIN_DIR@/radiod -v @KA9Q_CONFIG_DIR@/radiod@%i.conf; \
    elif [ -f /usr/local/share/ka9q-radio/configs/radiod@%i.conf ]; then \
        exec @KA9Q_SBIN_DIR@/radiod -v /usr/local/share/ka9q-radio/configs/radiod@%i.conf; \
    else \
        echo "Config not found: radiod@%i.conf"; \
        exit 1; \
    fi'
```

But this is usually overkill - keep it simple.

## Migration Checklist

- [ ] Create `systemd/radiod@.service.in` template
- [ ] Remove or move old `service/` directory
- [ ] Update CMakeLists.txt (already done in yours)
- [ ] Move example configs to `config/` directory
- [ ] Add `.example` extension to example configs
- [ ] Update README with template unit usage
- [ ] Test: Create a config and start an instance
- [ ] Verify CMake generates correct service file
- [ ] Check package includes service file in systemd component

## Testing the Template

### Build and Install
```bash
mkdir build && cd build
cmake ..
cmake --build .
sudo cmake --install .
```

### Verify Service File
```bash
# Check the generated file
cat /etc/systemd/system/radiod@.service

# Should show configured paths like:
# ExecStart=/usr/local/sbin/radiod -v /etc/radio/radiod@%i.conf
```

### Test Instance Creation
```bash
# Create a test config
sudo cp /etc/radio/radiod@hf.conf.example /etc/radio/radiod@test.conf

# Start instance
sudo systemctl start radiod@test

# Check status
systemctl status radiod@test

# View logs
journalctl -u radiod@test -f
```

## Documentation Updates

### In README.md
```markdown
## Starting radiod

ka9q-radio uses systemd template units. Create a config file
and start an instance:

# Create config
sudo cp /etc/radio/radiod@hf.conf.example /etc/radio/radiod@hf.conf
sudo nano /etc/radio/radiod@hf.conf

# Start instance
sudo systemctl start radiod@hf

# Enable on boot
sudo systemctl enable radiod@hf

# Check status
systemctl status radiod@hf

You can create unlimited instances with different config files.
```

### In systemd/README.md
```markdown
# systemd Template Unit

This directory contains a systemd template unit for radiod.

## Template Unit: radiod@.service.in

The `@` in the filename makes this a template. Users create
instances by specifying a name:

    systemctl start radiod@INSTANCE_NAME

The instance name is used to find the config file:

    /etc/radio/radiod@INSTANCE_NAME.conf

## Configuration by CMake

The `.in` extension means CMake processes this file:
- `@KA9Q_SBIN_DIR@` → `/usr/local/sbin`
- `@KA9Q_CONFIG_DIR@` → `/etc/radio`
- `@KA9Q_STATE_DIR@` → `/var/lib/ka9q-radio`

The processed file is installed as `/etc/systemd/system/radiod@.service`

## Multiple Instances

Run multiple radiod instances simultaneously:

    systemctl start radiod@hf
    systemctl start radiod@vhf
    systemctl start radiod@uhf
```

## Comparison: Old vs New

### Old Approach (service/ directory)
```
service/radiod@hf.service:
[Service]
ExecStart=/usr/local/sbin/radiod -v /etc/radio/radiod@hf.conf

service/radiod@vhf.service:
[Service]
ExecStart=/usr/local/sbin/radiod -v /etc/radio/radiod@vhf.conf

... 20+ more files
```

**Problems:**
- 20+ files to maintain
- Hardcoded paths
- Can't create new instances without adding files
- Forces systemd dependency

### New Approach (systemd/radiod@.service.in)
```
systemd/radiod@.service.in:
[Service]
ExecStart=@KA9Q_SBIN_DIR@/radiod -v @KA9Q_CONFIG_DIR@/radiod@%i.conf
```

**Benefits:**
- 1 file to maintain
- CMake-configured paths
- Unlimited instances without new files
- Optional systemd component

## Backward Compatibility

### Do users need to change anything?

**No!** The commands remain the same:
```bash
# Old static files
systemctl start radiod@hf

# New template
systemctl start radiod@hf    # Same command!
```

The only difference is the template is more flexible.

## Summary

### Before (service/ directory)
- ❌ Many static service files
- ❌ Hardcoded paths
- ❌ Forces systemd dependency
- ❌ Limited to predefined configs

### After (systemd/ template)
- ✅ One template file
- ✅ CMake-configured paths
- ✅ Optional systemd component
- ✅ Unlimited user configs

### Migration Steps
1. Create `systemd/radiod@.service.in` (already have it!)
2. Remove `service/` directory
3. Move example configs to `config/*.conf.example`
4. Update documentation
5. Test with: `systemctl start radiod@test`

The CMake build system I provided already implements this correctly.
You just need to remove or ignore the old `service/` directory.
