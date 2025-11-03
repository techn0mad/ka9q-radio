cmake_minimum_required(VERSION 3.15)
project(ka9q-radio 
    VERSION 1.0.0
    DESCRIPTION "Multichannel SDR based on fast convolution and IP multicasting"
    LANGUAGES C)

# Build options
option(BUILD_SHARED_LIBS "Build shared libraries for hardware drivers" OFF)
option(ENABLE_FOBOS "Enable Fobos SDR support (requires manual installation)" OFF)
option(ENABLE_SDRPLAY "Enable SDRplay support (requires API installation)" OFF)
option(ENABLE_BLADERF "Enable BladeRF support" OFF)
option(ENABLE_HACKRF "Enable HackRF support" ON)
option(ENABLE_HYDRASDR "Enable HydraSDR support" OFF)

add_compile_definitions(_GNU_SOURCE=1)

# Platform detection
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(LINUX TRUE)
elseif(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
    set(FREEBSD TRUE)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(MACOS TRUE)
    
    # Detect macOS package manager and set up paths
    # Check for Homebrew first (more common on Apple Silicon)
    if(EXISTS "/opt/homebrew/bin/brew")
        # Apple Silicon Homebrew
        set(HOMEBREW_PREFIX "/opt/homebrew")
        message(STATUS "Detected Homebrew (Apple Silicon) at ${HOMEBREW_PREFIX}")
        list(APPEND CMAKE_PREFIX_PATH "${HOMEBREW_PREFIX}")
        set(PKG_CONFIG_PATH "${HOMEBREW_PREFIX}/lib/pkgconfig:${HOMEBREW_PREFIX}/share/pkgconfig")
    elseif(EXISTS "/usr/local/bin/brew")
        # Intel Homebrew
        set(HOMEBREW_PREFIX "/usr/local")
        message(STATUS "Detected Homebrew (Intel) at ${HOMEBREW_PREFIX}")
        list(APPEND CMAKE_PREFIX_PATH "${HOMEBREW_PREFIX}")
        set(PKG_CONFIG_PATH "${HOMEBREW_PREFIX}/lib/pkgconfig:${HOMEBREW_PREFIX}/share/pkgconfig")
    elseif(EXISTS "/opt/local/bin/port")
        # MacPorts
        set(MACPORTS_PREFIX "/opt/local")
        message(STATUS "Detected MacPorts at ${MACPORTS_PREFIX}")
        list(APPEND CMAKE_PREFIX_PATH "${MACPORTS_PREFIX}")
        set(PKG_CONFIG_PATH "${MACPORTS_PREFIX}/lib/pkgconfig:${MACPORTS_PREFIX}/share/pkgconfig")
    else()
        message(WARNING "No package manager (Homebrew or MacPorts) detected on macOS. You may need to set CMAKE_PREFIX_PATH manually.")
    endif()
    
    # Set PKG_CONFIG_PATH environment variable for this build
    if(DEFINED PKG_CONFIG_PATH)
        set(ENV{PKG_CONFIG_PATH} "${PKG_CONFIG_PATH}")
        message(STATUS "Set PKG_CONFIG_PATH to ${PKG_CONFIG_PATH}")
    endif()
endif()

# Set C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_C_EXTENSIONS ON)  # For GNU extensions like _GNU_SOURCE

# Compiler flags
add_compile_options(
    -Wall
    -Wextra
    -pthread
    -funsafe-math-optimizations
    -fno-math-errno
    -fcx-limited-range
    -freciprocal-math
    -fno-trapping-math
)

# Platform-specific compiler flags
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_compile_options(-O3 -DNDEBUG=1)
    if(NOT MACOS)
        add_compile_options(-march=native)
    endif()
else()
    add_compile_options(-g)
endif()

# Find required dependencies using pkg-config
find_package(PkgConfig REQUIRED)

# Core required libraries
pkg_check_modules(FFTW3F REQUIRED fftw3f)
pkg_check_modules(OPUS REQUIRED opus)
pkg_check_modules(BSD REQUIRED libbsd)

# --- iniparser: try pkg-config names first, then fallback ---
if(PKG_CONFIG_FOUND)
  pkg_search_module(INIPARSER QUIET iniparser iniparser4)
endif()

if(NOT INIPARSER_FOUND)
  # Ubuntu Jammy's libiniparser-dev has no .pc; find headers/libs directly
  find_path(INIPARSER_INCLUDE_DIR NAMES iniparser.h
            PATHS /usr/include /usr/local/include)
  # Library is usually libiniparser.so
  find_library(INIPARSER_LIBRARY  NAMES iniparser
               PATHS /usr/lib /usr/lib/x86_64-linux-gnu /usr/local/lib)
  if(INIPARSER_INCLUDE_DIR AND INIPARSER_LIBRARY)
    set(INIPARSER_FOUND TRUE)
    set(INIPARSER_INCLUDE_DIRS ${INIPARSER_INCLUDE_DIR})
    set(INIPARSER_LIBRARIES    ${INIPARSER_LIBRARY})
  endif()
endif()

if(INIPARSER_FOUND)
  add_library(Iniparser::iniparser INTERFACE IMPORTED)
  set_target_properties(Iniparser::iniparser PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${INIPARSER_INCLUDE_DIRS}"
    INTERFACE_LINK_LIBRARIES      "${INIPARSER_LIBRARIES}"
  )
else()
  message(STATUS "iniparser not found via pkg-config; looked for headers/libs directly.
  On Ubuntu/Debian install: sudo apt-get install libiniparser-dev")
endif()

# Find other required libraries
find_package(Threads REQUIRED)
find_library(MATH_LIBRARY m)

# Platform-specific required libraries
if(LINUX)
    pkg_check_modules(AVAHI REQUIRED avahi-client)
    # Check for systemd (optional, for service file installation)
    find_package(PkgConfig)
    pkg_check_modules(SYSTEMD libsystemd)
    if(SYSTEMD_FOUND)
        set(HAVE_SYSTEMD TRUE)
        pkg_get_variable(SYSTEMD_UNIT_DIR systemd systemdsystemunitdir)
    endif()
endif()

# Optional hardware support libraries
if(ENABLE_HACKRF OR NOT DEFINED ENABLE_HACKRF)
    pkg_check_modules(HACKRF libhackrf)
    if(HACKRF_FOUND)
        set(HAVE_HACKRF TRUE)
    endif()
endif()

pkg_check_modules(AIRSPY libairspy)
if(AIRSPY_FOUND)
    set(HAVE_AIRSPY TRUE)
endif()

pkg_check_modules(AIRSPYHF libairspyhf)
if(AIRSPYHF_FOUND)
    set(HAVE_AIRSPYHF TRUE)
endif()

pkg_check_modules(RTLSDR librtlsdr)
if(RTLSDR_FOUND)
    set(HAVE_RTLSDR TRUE)
endif()

# USB libraries (for hardware support)
find_library(USB_LIBRARY usb-1.0)
find_path(USB_INCLUDE_DIR libusb-1.0/libusb.h)

# Audio libraries (for monitor program)
pkg_check_modules(PORTAUDIO portaudio-2.0)
pkg_check_modules(ALSA alsa)
pkg_check_modules(NCURSES ncursesw)
if(NOT NCURSES_FOUND)
    pkg_check_modules(NCURSES ncurses)
endif()

# Additional optional libraries
pkg_check_modules(OGG ogg)
pkg_check_modules(SAMPLERATE samplerate)

# Installation directories
include(GNUInstallDirs)
set(KA9Q_SBIN_DIR "${CMAKE_INSTALL_SBINDIR}")
set(KA9Q_BIN_DIR "${CMAKE_INSTALL_BINDIR}")
set(KA9Q_SHARE_DIR "${CMAKE_INSTALL_DATADIR}/ka9q-radio")
set(KA9Q_LIB_DIR "${CMAKE_INSTALL_LIBDIR}/ka9q-radio")
set(KA9Q_STATE_DIR "${CMAKE_INSTALL_LOCALSTATEDIR}/lib/ka9q-radio")
set(KA9Q_CONFIG_DIR "${CMAKE_INSTALL_SYSCONFDIR}/radio")

# Platform-specific directories
if(LINUX AND HAVE_SYSTEMD)
    set(KA9Q_SYSTEMD_DIR "${SYSTEMD_UNIT_DIR}")
    set(KA9Q_SYSCTL_DIR "${CMAKE_INSTALL_SYSCONFDIR}/sysctl.d")
    set(KA9Q_UDEV_DIR "${CMAKE_INSTALL_SYSCONFDIR}/udev/rules.d")
elseif(FREEBSD)
    set(KA9Q_RC_DIR "${CMAKE_INSTALL_SYSCONFDIR}/rc.d")
endif()

# Common source files for libradio
set(LIBRADIO_SOURCES
    src/ax25.c
    src/morse.c
    src/bandplan.c
    src/dump.c
    src/modes.c
    src/avahi.c
    src/avahi_browse.c
    src/attr.c
    src/filter.c
    src/iir.c
    src/decode_status.c
    src/status.c
    src/misc.c
    src/multicast.c
    src/rtp.c
    src/osc.c
    src/config.c
)


# Create libradio (static for now, but could be shared)
add_library(radio STATIC ${LIBRADIO_SOURCES})

target_include_directories(radio PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${FFTW3F_INCLUDE_DIRS}
    ${BSD_INCLUDE_DIRS}
)
target_link_libraries(radio PUBLIC
    ${FFTW3F_LIBRARIES}
    ${BSD_LIBRARIES}
    Threads::Threads
    ${MATH_LIBRARY}
)

# FFTW threads library (needed for fftwf_*_with_nthreads, fftwf_init_threads)
find_library(FFTW3F_THREADS_LIBRARY NAMES fftw3f_threads)

if(FFTW3F_THREADS_LIBRARY)
  target_link_libraries(radio PUBLIC ${FFTW3F_THREADS_LIBRARY})
  target_compile_definitions(radio PUBLIC HAVE_FFTW_THREADS=1)
else()
  message(WARNING "fftw3f_threads not found; builds using FFTW threading will fail to link on some targets.")
endif()

# ---- Driver plugins (built as MODULE and dlopened at runtime) ----
function(add_driver name)
  cmake_parse_arguments(DRV "" "" "SOURCES;LIBS;INCLUDES" ${ARGN})
  # Use an internal target name to avoid clashes with system libs
  set(_t "${name}_drv")
  add_library(${_t} MODULE ${DRV_SOURCES})
  # Keep the .so/.dylib filename as "airspy", "rtlsdr", etc.
  set_target_properties(${_t} PROPERTIES OUTPUT_NAME "${name}")
  if(DRV_INCLUDES)
    target_include_directories(${_t} PRIVATE ${DRV_INCLUDES})
  endif()
  if(DRV_LIBS)
    target_link_libraries(${_t} PRIVATE radio ${DRV_LIBS})
  else()
    target_link_libraries(${_t} PRIVATE radio)
  endif()
  install(TARGETS ${_t}
          LIBRARY DESTINATION ${KA9Q_LIB_DIR}
          COMPONENT runtime)
endfunction()

if(HAVE_AIRSPY)
  add_driver(airspy  SOURCES src/airspy.c   LIBS ${AIRSPY_LIBRARIES}   INCLUDES ${AIRSPY_INCLUDE_DIRS})
endif()
if(HAVE_AIRSPYHF)
  add_driver(airspyhf SOURCES src/airspyhf.c LIBS ${AIRSPYHF_LIBRARIES} INCLUDES ${AIRSPYHF_INCLUDE_DIRS})
endif()
if(HAVE_RTLSDR)
  add_driver(rtlsdr  SOURCES src/rtlsdr.c   LIBS ${RTLSDR_LIBRARIES}   INCLUDES ${RTLSDR_INCLUDE_DIRS})
endif()
if(HAVE_HACKRF)
  add_driver(hackrf  SOURCES src/hackrf.c   LIBS ${HACKRF_LIBRARIES}   INCLUDES ${HACKRF_INCLUDE_DIRS})
endif()
if(PORTAUDIO_FOUND AND USB_LIBRARY AND USB_INCLUDE_DIR)
  add_driver(funcube SOURCES src/funcube.c;src/fcd.c;src/hid-libusb.c LIBS ${PORTAUDIO_LIBRARIES} ${USB_LIBRARY} INCLUDES ${PORTAUDIO_INCLUDE_DIRS} ${USB_INCLUDE_DIR})
endif()
if(USB_LIBRARY AND USB_INCLUDE_DIR)
  add_driver(rx888   SOURCES src/rx888.c;src/ezusb.c;src/hid-libusb.c LIBS ${USB_LIBRARY} INCLUDES ${USB_INCLUDE_DIR})
endif()

if(TARGET oggrecord)
    install(TARGETS oggrecord DESTINATION ${KA9Q_BIN_DIR} COMPONENT runtime)
endif()

# Install support files
install(FILES
    share/presets.txt
    share/modes.txt
    DESTINATION ${KA9Q_SHARE_DIR}
    COMPONENT runtime
    OPTIONAL
)

# Install example configuration files
install(DIRECTORY config/
    DESTINATION ${KA9Q_CONFIG_DIR}
    COMPONENT runtime
    OPTIONAL
    FILES_MATCHING
        PATTERN "*.conf"
        PATTERN "*.conf.example" EXCLUDE
)

# Create state directory
install(DIRECTORY DESTINATION ${KA9Q_STATE_DIR} COMPONENT runtime)

# Platform-specific installations
if(LINUX)
    # Note: If the repository has a "service/" directory with static systemd files,
    # we intentionally ignore it in favor of the template unit approach.
    # Static files in service/ create a hard systemd dependency and don't benefit
    # from CMake path configuration. The template unit (radiod@.service.in) is
    # more flexible and allows unlimited user-defined instances.
    
    # Always try to install systemd service files if the template exists
    # even if systemd isn't detected at build time
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
        
        # Install sysctl configuration with systemd component
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/systemd/98-sockbuf.conf")
            install(FILES systemd/98-sockbuf.conf
                DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/sysctl.d
                COMPONENT systemd
            )
        endif()
    endif()
    
    # Install udev rules (always, not systemd-specific)
    file(GLOB UDEV_RULES "udev/*.rules")
    if(UDEV_RULES)
        install(FILES ${UDEV_RULES}
            DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/udev/rules.d
            COMPONENT runtime
        )
    endif()
elseif(FREEBSD)
    # Install FreeBSD rc.d script
    configure_file(
        freebsd/radiod.in
        ${CMAKE_CURRENT_BINARY_DIR}/radiod
        @ONLY
    )
    install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/radiod
        DESTINATION ${KA9Q_RC_DIR}
        COMPONENT runtime
        OPTIONAL
    )
endif()

# Create user and group (post-install script needed)
# This would typically be done by the package manager

# Print configuration summary
message(STATUS "")
message(STATUS "ka9q-radio configuration summary:")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  System: ${CMAKE_SYSTEM_NAME}")
if(MACOS)
    if(DEFINED HOMEBREW_PREFIX)
        message(STATUS "  Package Manager: Homebrew (${HOMEBREW_PREFIX})")
    elseif(DEFINED MACPORTS_PREFIX)
        message(STATUS "  Package Manager: MacPorts (${MACPORTS_PREFIX})")
    endif()
endif()
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C Compiler: ${CMAKE_C_COMPILER}")
message(STATUS "")
message(STATUS "Features:")
message(STATUS "  Airspy support: ${HAVE_AIRSPY}")
message(STATUS "  Airspy HF+ support: ${HAVE_AIRSPYHF}")
message(STATUS "  RTL-SDR support: ${HAVE_RTLSDR}")
message(STATUS "  HackRF support: ${HAVE_HACKRF}")
if(LINUX)
    message(STATUS "  systemd support: ${HAVE_SYSTEMD}")
    message(STATUS "  Avahi support: ${AVAHI_FOUND}")
endif()
message(STATUS "  PortAudio support: ${PORTAUDIO_FOUND}")
message(STATUS "  ALSA support: ${ALSA_FOUND}")
message(STATUS "  ncurses support: ${NCURSES_FOUND}")
message(STATUS "")
message(STATUS "Installation directories:")
message(STATUS "  Daemons: ${CMAKE_INSTALL_PREFIX}/${KA9Q_SBIN_DIR}")
message(STATUS "  Programs: ${CMAKE_INSTALL_PREFIX}/${KA9Q_BIN_DIR}")
message(STATUS "  Support files: ${CMAKE_INSTALL_PREFIX}/${KA9Q_SHARE_DIR}")
message(STATUS "  Config files: ${KA9Q_CONFIG_DIR}")
message(STATUS "  State files: ${KA9Q_STATE_DIR}")
if(LINUX AND HAVE_SYSTEMD)
    message(STATUS "  systemd units: ${KA9Q_SYSTEMD_DIR}")
endif()
message(STATUS "")

# CPack configuration for packaging
set(CPACK_PACKAGE_NAME "ka9q-radio")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_DESCRIPTION}")
set(CPACK_PACKAGE_VENDOR "Phil Karn KA9Q")
set(CPACK_PACKAGE_CONTACT "karn@ka9q.net")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")

# Component-based packaging
# This allows building separate packages for main program and systemd support
set(CPACK_DEB_COMPONENT_INSTALL ON)
set(CPACK_RPM_COMPONENT_INSTALL ON)

# Define components
set(CPACK_COMPONENTS_ALL runtime systemd)
set(CPACK_COMPONENT_RUNTIME_DISPLAY_NAME "KA9Q Radio Runtime")
set(CPACK_COMPONENT_RUNTIME_DESCRIPTION "Main ka9q-radio programs and libraries")
set(CPACK_COMPONENT_RUNTIME_REQUIRED ON)

set(CPACK_COMPONENT_SYSTEMD_DISPLAY_NAME "KA9Q Radio systemd support")
set(CPACK_COMPONENT_SYSTEMD_DESCRIPTION "systemd service files for ka9q-radio")
set(CPACK_COMPONENT_SYSTEMD_DEPENDS runtime)

# Debian package configuration
set(CPACK_DEBIAN_PACKAGE_SECTION "hamradio")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")

# Base dependencies (always required for runtime component)
set(CPACK_DEBIAN_RUNTIME_PACKAGE_NAME "ka9q-radio")
set(CPACK_DEBIAN_RUNTIME_PACKAGE_DEPENDS 
    "libfftw3-single3, libopus0, libbsd0, libiniparser1, libavahi-client3, libc6"
)

# systemd component creates a separate package
set(CPACK_DEBIAN_SYSTEMD_PACKAGE_NAME "ka9q-radio-systemd")
set(CPACK_DEBIAN_SYSTEMD_PACKAGE_DEPENDS "ka9q-radio (= ${PROJECT_VERSION}), systemd")
set(CPACK_DEBIAN_SYSTEMD_PACKAGE_ARCHITECTURE "all")

# Add hardware-specific dependencies to runtime package
if(HAVE_AIRSPY)
    set(CPACK_DEBIAN_RUNTIME_PACKAGE_DEPENDS "${CPACK_DEBIAN_RUNTIME_PACKAGE_DEPENDS}, libairspy0")
endif()
if(HAVE_RTLSDR)
    set(CPACK_DEBIAN_RUNTIME_PACKAGE_DEPENDS "${CPACK_DEBIAN_RUNTIME_PACKAGE_DEPENDS}, librtlsdr0")
endif()
if(HAVE_HACKRF)
    set(CPACK_DEBIAN_RUNTIME_PACKAGE_DEPENDS "${CPACK_DEBIAN_RUNTIME_PACKAGE_DEPENDS}, libhackrf0")
endif()

# Control scripts
set(CPACK_DEBIAN_RUNTIME_PACKAGE_CONTROL_EXTRA 
    "${CMAKE_CURRENT_SOURCE_DIR}/debian/postinst;${CMAKE_CURRENT_SOURCE_DIR}/debian/prerm"
)
# systemd package gets its own postinst
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/debian/systemd-postinst")
    set(CPACK_DEBIAN_SYSTEMD_PACKAGE_CONTROL_EXTRA 
        "${CMAKE_CURRENT_SOURCE_DIR}/debian/systemd-postinst"
    )
endif()

# RPM package configuration
set(CPACK_RPM_PACKAGE_GROUP "Applications/Communications")
set(CPACK_RPM_PACKAGE_LICENSE "GPL-3.0")

# Base dependencies (always required for runtime component)
set(CPACK_RPM_RUNTIME_PACKAGE_NAME "ka9q-radio")
set(CPACK_RPM_RUNTIME_PACKAGE_REQUIRES 
    "fftw-libs-single, opus, libbsd, iniparser, avahi-libs"
)

# systemd component creates a separate package
set(CPACK_RPM_SYSTEMD_PACKAGE_NAME "ka9q-radio-systemd")
set(CPACK_RPM_SYSTEMD_PACKAGE_REQUIRES "ka9q-radio = ${PROJECT_VERSION}, systemd")
set(CPACK_RPM_SYSTEMD_PACKAGE_ARCHITECTURE "noarch")

# Post-install scripts for RPM
set(CPACK_RPM_RUNTIME_POST_INSTALL_SCRIPT_FILE "${CMAKE_CURRENT_SOURCE_DIR}/rpm/postinstall")
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/rpm/systemd-postinstall")
    set(CPACK_RPM_SYSTEMD_POST_INSTALL_SCRIPT_FILE "${CMAKE_CURRENT_SOURCE_DIR}/rpm/systemd-postinstall")
endif()

# FreeBSD package configuration
if(FREEBSD)
    set(CPACK_GENERATOR "TXZ")
    set(CPACK_FREEBSD_PACKAGE_DEPS 
        "fftw3-float, opus, libbsd, iniparser"
    )
endif()

# General package generators
if(LINUX)
    set(CPACK_GENERATOR "DEB;RPM;TGZ")
elseif(MACOS)
    set(CPACK_GENERATOR "TGZ")
endif()

include(CPack)

# tools that require iniparser (e.g., control)
if(INIPARSER_FOUND AND NCURSES_FOUND)
  add_executable(control src/control.c)
  target_link_libraries(control PRIVATE radio Iniparser::iniparser ${NCURSES_LIBRARIES})
endif()

add_executable(packetd src/packetd.c)
target_link_libraries(packetd PRIVATE radio)

add_executable(fft-gen src/fft-gen.c)
target_link_libraries(fft-gen PRIVATE radio ${FFTW3F_LIBRARIES})

add_executable(pl src/pl.c)
target_link_libraries(pl PRIVATE radio ${FFTW3F_LIBRARIES})

add_executable(powers src/powers.c)
target_link_libraries(powers PRIVATE radio)

add_executable(tune src/tune.c)
target_link_libraries(tune PRIVATE radio)

add_executable(show-sig src/show-sig.c)
target_link_libraries(show-sig PRIVATE radio ${NCURSES_LIBRARIES})

add_executable(show-pkt src/show-pkt.c)
target_link_libraries(show-pkt PRIVATE radio ${NCURSES_LIBRARIES})

add_executable(rdsd src/rdsd.c)
target_link_libraries(rdsd PRIVATE radio ${FFTW3F_LIBRARIES})

add_executable(stereod src/stereod.c)
target_link_libraries(stereod PRIVATE radio)

add_executable(jt-decoded src/jt-decoded.c)
target_link_libraries(jt-decoded PRIVATE radio)

add_executable(wd-record src/wd-record.c)
target_link_libraries(wd-record PRIVATE radio)

add_executable(cwd src/cwd.c)
target_link_libraries(cwd PRIVATE radio)

add_executable(pcmcat src/pcmcat.c)
target_link_libraries(pcmcat PRIVATE radio)

add_executable(pcmspawn src/pcmspawn.c)
target_link_libraries(pcmspawn PRIVATE radio)

add_executable(sig-gen src/sig_gen.c)
target_link_libraries(sig-gen PRIVATE radio)

# Always install the daemon if present
if(TARGET radiod)
  install(TARGETS radiod DESTINATION ${KA9Q_SBIN_DIR} COMPONENT runtime)
endif()

# Binaries: install only those that exist
foreach(_t IN ITEMS
  control monitor pcmrecord pcmplay opussend metadump
  fft-gen pl powers tune show-sig show-pkt rdsd stereod
  jt-decoded wd-record cwd pcmcat pcmspawn sig-gen packetd)
  if(TARGET ${_t})
    install(TARGETS ${_t} DESTINATION ${KA9Q_BIN_DIR} COMPONENT runtime)
  endif()
endforeach()

