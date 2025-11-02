# Static Service Files vs. Template Units - Visual Comparison

## The Problem You Identified

```
ka9q-radio/
└── service/                      ← This directory is problematic
    ├── radiod@hf.service         ← Static, hardcoded paths
    ├── radiod@vhf.service        ← Duplicates configuration
    ├── radiod@uhf.service        ← Forces systemd dependency
    ├── radiod@packet.service     ← Not CMake-configured
    ├── radiod@10m.service
    ├── radiod@6m.service
    └── ... many more             ← File proliferation
```

## Static Files Approach (Current Problem)

### File Content Example
```ini
# service/radiod@hf.service
[Unit]
Description=KA9Q Radio Daemon (hf)
After=network.target

[Service]
Type=simple
User=radio
ExecStart=/usr/local/sbin/radiod -v /etc/radio/radiod@hf.conf
           ^^^^^^^^^^^^^^^^^^^^     ^^^^^^^^^^^
           Hardcoded paths - not configured by CMake

[Install]
WantedBy=multi-user.target
```

### Problems
```
❌ Issue 1: Path Hardcoding
   /usr/local/sbin/radiod  ← What if installed to /usr/sbin?
   /etc/radio/             ← What if installed to /opt/etc?

❌ Issue 2: File Duplication
   20+ files, each nearly identical
   Only difference: "hf" vs "vhf" vs "uhf" etc.

❌ Issue 3: Maintenance Nightmare
   Change security policy? Update 20+ files
   New resource limit? Update 20+ files

❌ Issue 4: User Inflexibility
   User wants config "mysdr"? Need to add new file to repo

❌ Issue 5: Forces systemd
   Can't create DEB package without systemd dependency
   Files installed even on non-systemd systems

❌ Issue 6: Not CMake-aware
   Doesn't respect CMAKE_INSTALL_PREFIX
   Doesn't work with component packaging
```

## Template Unit Approach (Solution)

### Single Template File
```
ka9q-radio/
└── systemd/
    └── radiod@.service.in        ← One template, CMake-configured
```

### File Content
```ini
# systemd/radiod@.service.in
[Unit]
Description=KA9Q Radio Daemon (%i)
                              ^^^^
                              Shows instance name
After=network.target

[Service]
Type=simple
User=radio
ExecStart=@KA9Q_SBIN_DIR@/radiod -v @KA9Q_CONFIG_DIR@/radiod@%i.conf
          ^^^^^^^^^^^^^^^^^           ^^^^^^^^^^^^^^^^^^        ^^
          CMake variable              CMake variable           Instance name

[Install]
WantedBy=multi-user.target
```

### After CMake Configuration
```ini
# build/radiod@.service (generated)
[Unit]
Description=KA9Q Radio Daemon (%i)
After=network.target

[Service]
Type=simple
User=radio
ExecStart=/usr/local/sbin/radiod -v /etc/radio/radiod@%i.conf
          ^^^^^^^^^^^^^^^^^^^^       ^^^^^^^^^^^
          Configured by CMake        Configured by CMake

[Install]
WantedBy=multi-user.target
```

## Side-by-Side Comparison

### Static Files (service/)
```
Files:    20+ static files
Paths:    Hardcoded
Configs:  Fixed set only
CMake:    Not integrated
Install:  Always (forced)
Package:  Forces systemd dep
Users:    Limited to predefined configs
```

### Template Unit (systemd/)
```
Files:    1 template
Paths:    CMake-configured
Configs:  Unlimited
CMake:    Fully integrated
Install:  Optional component
Package:  Optional systemd component
Users:    Create any config
```

## User Experience Comparison

### Static Approach
```bash
# Predefined configs only
$ systemctl start radiod@hf      ✓ Works (if file exists)
$ systemctl start radiod@vhf     ✓ Works (if file exists)
$ systemctl start radiod@mysdr   ✗ Fails (no service file)

# To add new config, need to:
1. Create service file
2. Add to repository
3. Rebuild package
4. Reinstall
```

### Template Approach
```bash
# Any config name works
$ systemctl start radiod@hf      ✓ Works
$ systemctl start radiod@vhf     ✓ Works
$ systemctl start radiod@mysdr   ✓ Works (if config file exists)

# To add new config:
1. Create config file: /etc/radio/radiod@mysdr.conf
2. systemctl start radiod@mysdr
   Done!
```

## Technical Details

### How Templates Work

**Template File Name:**
```
radiod@.service
       ^^
       Empty - makes it a template
```

**Instance Creation:**
```bash
systemctl start radiod@INSTANCE_NAME
                       ^^^^^^^^^^^^^
                       Fills in %i variable
```

**Variable Substitution:**
```ini
ExecStart=/usr/local/sbin/radiod -v /etc/radio/radiod@%i.conf
                                                        ^^
When started as:                 %i becomes:            Value:
systemctl start radiod@hf        %i = hf                radiod@hf.conf
systemctl start radiod@vhf       %i = vhf               radiod@vhf.conf
systemctl start radiod@test      %i = test              radiod@test.conf
```

### Other Template Variables
```
%i    Instance name (hf, vhf, etc.)
%I    Instance name with escaping
%n    Full unit name (radiod@hf.service)
%N    Unescaped full unit name
%p    Prefix (radiod)
%f    Path to unit file
```

## Package Installation Comparison

### Static Files (Problematic)

**Debian Package Contents:**
```
ka9q-radio_1.0.0_amd64.deb:
  /usr/local/sbin/radiod
  /usr/local/bin/control
  /etc/systemd/system/radiod@hf.service       ← Forces systemd
  /etc/systemd/system/radiod@vhf.service      ← on all installs
  /etc/systemd/system/radiod@uhf.service
  ... 20+ more service files

Dependencies: ... systemd                     ← Hard dependency
```

**Install on Devuan (non-systemd):**
```
$ sudo dpkg -i ka9q-radio_1.0.0_amd64.deb
dpkg: dependency problems prevent configuration:
 ka9q-radio depends on systemd; however:
  Package systemd is not installed.
  
✗ FAILS
```

### Template Approach (Flexible)

**Runtime Package:**
```
ka9q-radio_1.0.0_amd64.deb:
  /usr/local/sbin/radiod
  /usr/local/bin/control
  /etc/radio/radiod@hf.conf.example
  
Dependencies: libfftw3-single3, libopus0, ...   ← No systemd!
```

**systemd Package (Optional):**
```
ka9q-radio-systemd_1.0.0_all.deb:
  /etc/systemd/system/radiod@.service         ← One template
  
Dependencies: ka9q-radio, systemd             ← Only this package needs systemd
```

**Install on Devuan:**
```
$ sudo dpkg -i ka9q-radio_1.0.0_amd64.deb
✓ SUCCESS - no systemd required

$ sudo dpkg -i ka9q-radio-systemd_1.0.0_all.deb
✗ Dependency error (expected - don't install this one)
```

**Install on Debian with systemd:**
```
$ sudo dpkg -i ka9q-radio_1.0.0_amd64.deb
✓ SUCCESS

$ sudo dpkg -i ka9q-radio-systemd_1.0.0_all.deb
✓ SUCCESS - adds systemd support

$ systemctl start radiod@hf
✓ Works!
```

## Migration Path

### Step 1: Ignore service/ directory
```cmake
# CMakeLists.txt already does this
# It only looks for systemd/radiod@.service.in
# Completely ignores service/ directory
```

### Step 2: Document the change
```markdown
# In CHANGELOG or migration notes:

## Breaking Change: systemd Service Files

The static service files in `service/` have been replaced with
a single template unit in `systemd/radiod@.service.in`.

**If you were using:**
- systemctl start radiod@hf

**You still use the same command!** No changes needed.

**If you had custom service files:**
- Create config file: /etc/radio/radiod@custom.conf
- Use: systemctl start radiod@custom

The template approach is more flexible and doesn't force systemd.
```

### Step 3: Optional - Remove service/ directory
```bash
# From repository
git rm -r service/
git commit -m "Remove static service files, use template unit"

# Or just add to .gitignore
echo "service/" >> .gitignore
```

## Real-World Example

### Before (service/ directory)
```
$ ls service/
radiod@10m.service    radiod@hf.service     radiod@uhf.service
radiod@15m.service    radiod@packet.service radiod@vhf.service
radiod@20m.service    radiod@satellite.service
radiod@40m.service    radiod@test.service
radiod@6m.service     radiod@transponder.service
radiod@80m.service    radiod@2m.service

$ wc -l service/*.service
  15 service/radiod@10m.service
  15 service/radiod@15m.service
  15 service/radiod@20m.service
  ... (all nearly identical)
  Total: 225 lines across 15 files
```

### After (systemd/ template)
```
$ ls systemd/
radiod@.service.in

$ wc -l systemd/radiod@.service.in
  20 systemd/radiod@.service.in
  Total: 20 lines in 1 file
```

**Reduction: 225 lines → 20 lines**

## Q&A

**Q: Will my existing configs still work?**
A: Yes! If you have `/etc/radio/radiod@hf.conf`, just use `systemctl start radiod@hf`

**Q: Do I need to change user documentation?**
A: No, the commands are identical. Users won't notice the difference.

**Q: What about custom service files users created?**
A: They can still use them, or switch to the template (more flexible)

**Q: Can I keep both approaches?**
A: Yes, but don't. It's confusing and defeats the purpose.

**Q: What if I want different ExecStart for different configs?**
A: You shouldn't. That's what config files are for. If you really need it, use systemd drop-in units.

## Summary

### The Problem
```
service/ directory with 20+ static, hardcoded service files
└─> Forces systemd dependency
└─> Not CMake-configured  
└─> Maintenance nightmare
└─> User-inflexible
```

### The Solution
```
systemd/radiod@.service.in - Single CMake-configured template
└─> Optional systemd component
└─> CMake path configuration
└─> Easy maintenance (one file)
└─> User-flexible (unlimited instances)
```

### Migration
```
1. CMakeLists.txt already uses template (nothing to do)
2. Ignore or remove service/ directory
3. Document that commands don't change
4. Enjoy simpler, more flexible packaging
```

The CMake build system I provided completely solves this problem.
The service/ directory can be safely ignored or removed.
