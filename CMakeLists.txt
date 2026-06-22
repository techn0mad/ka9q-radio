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

    # Add package manager lib dir to the linker search path so that
    # libraries found via pkg-config (which may return just -lfoo without -L)
    # can be located by the linker at link time.
    if(DEFINED HOMEBREW_PREFIX)
        link_directories(${HOMEBREW_PREFIX}/lib)
    elseif(DEFINED MACPORTS_PREFIX)
        link_directories(${MACPORTS_PREFIX}/lib)
    endif()
endif()

# Set C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_C_EXTENSIONS ON)  # For GNU extensions like _GNU_SOURCE

# Compiler flags
include(CheckCCompilerFlag)

# Common warnings first
foreach(flag IN ITEMS -Wall -Wextra)
  check_c_compiler_flag("${flag}" HAVE_${flag})
  if(HAVE_${flag})
    add_compile_options(${flag})
  endif()
endforeach()

# Math/FP & threading flags — test each one before adding
foreach(flag IN ITEMS
  -pthread
  -funsafe-math-optimizations
  -fno-math-errno
  -fcx-limited-range        # GCC only; Clang/AppleClang typically lacks it
  -freciprocal-math
  -fno-trapping-math
)
  # Turn "-" and "=" into "_" to form a valid CMake var name
  string(REPLACE "-" "_" _var "${flag}")
  string(REPLACE "=" "_" _var "${_var}")
  check_c_compiler_flag("${flag}" HAVE${_var})
  if(HAVE${_var})
    add_compile_options(${flag})
  endif()
endforeach()

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

# Find FFTW (single-precision) and its threads component as imported targets
pkg_check_modules(FFTW3F     IMPORTED_TARGET fftw3f)
pkg_check_modules(FFTW3F_THR IMPORTED_TARGET fftw3f_threads)

pkg_check_modules(OPUS REQUIRED opus)
if(LINUX)
    pkg_check_modules(BSD REQUIRED libbsd)
endif()

# Ask pkg-config to create imported targets
pkg_check_modules(AIRSPY    IMPORTED_TARGET libairspy)
pkg_check_modules(AIRSPYHF  IMPORTED_TARGET libairspyhf)
pkg_check_modules(RTLSDR    IMPORTED_TARGET librtlsdr)
pkg_check_modules(HACKRF    IMPORTED_TARGET libhackrf)

# Helper: remove bad include dirs from a PkgConfig imported target, and add stable fallbacks
function(sanitize_pkg_import tgt)
  if(NOT TARGET ${tgt})
    return()
  endif()
  get_target_property(_incs ${tgt} INTERFACE_INCLUDE_DIRECTORIES)
  set(_clean_incs "")
  foreach(d IN LISTS _incs)
    if(EXISTS "${d}")
      list(APPEND _clean_incs "${d}")
    endif()
  endforeach()

  # Homebrew stable includes (won't change across upgrades):
  if(APPLE)
    # Generic include
    list(APPEND _clean_incs "/opt/homebrew/include")
    # Common subdirs used by dependencies
    if(EXISTS "/opt/homebrew/opt/libusb/include/libusb-1.0")
      list(APPEND _clean_incs "/opt/homebrew/opt/libusb/include/libusb-1.0")
    endif()
  endif()

  # De-dup
  list(REMOVE_DUPLICATES _clean_incs)
  set_property(TARGET ${tgt} PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${_clean_incs}")
endfunction()

# Sanitize the SDR backends created via pkg-config
sanitize_pkg_import(PkgConfig::HACKRF)
sanitize_pkg_import(PkgConfig::AIRSPY)
sanitize_pkg_import(PkgConfig::AIRSPYHF)
sanitize_pkg_import(PkgConfig::RTLSDR)

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

# dlopen — in libc on macOS/FreeBSD; separate libdl on Linux
find_library(DL_LIBRARY dl)

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

# Generate config_paths.h (paths + git version info used by misc.h, radio.c, filter.c)
find_package(Git QUIET)
if(Git_FOUND)
    execute_process(COMMAND ${GIT_EXECUTABLE} rev-parse HEAD
        OUTPUT_VARIABLE GIT_HASH OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    execute_process(COMMAND ${GIT_EXECUTABLE} show -s --format=%ci
        OUTPUT_VARIABLE GIT_TIME OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    execute_process(COMMAND ${GIT_EXECUTABLE} log --pretty=format:%d -n 1
        OUTPUT_VARIABLE GIT_BRANCH OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    execute_process(COMMAND ${GIT_EXECUTABLE} log -1 --format=%s
        OUTPUT_VARIABLE GIT_SUMMARY OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    execute_process(COMMAND ${GIT_EXECUTABLE} describe --always --dirty --tags
        OUTPUT_VARIABLE GIT_VERSION OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    execute_process(COMMAND ${GIT_EXECUTABLE} remote get-url origin
        OUTPUT_VARIABLE GIT_REMOTE_URL OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
endif()
configure_file(cmake/config_paths.h.in ${CMAKE_CURRENT_BINARY_DIR}/config_paths.h @ONLY)

# Common source files for libradio
set(LIBRADIO_SOURCES
    src/ax25.c
    src/morse.c
    src/bandplan.c
    src/dump.c
    src/modes.c
    src/attr.c
    src/filter.c
    src/iir.c
    src/window.c
    src/sincospi.c
    src/sincospif.c
    src/decode_status.c
    src/status.c
    src/misc.c
    src/multicast.c
    src/rtp.c
    src/osc.c
    src/config.c
)

# avahi.c / avahi_browse.c require avahi-client headers; use stubs elsewhere
if(AVAHI_CLIENT_FOUND)
    list(APPEND LIBRADIO_SOURCES src/avahi.c src/avahi_browse.c)
else()
    list(APPEND LIBRADIO_SOURCES src/compat/avahi_stub.c)
endif()


# Create libradio (static for now, but could be shared)
add_library(radio STATIC ${LIBRADIO_SOURCES})

target_include_directories(radio PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}
    ${FFTW3F_INCLUDE_DIRS}
    ${BSD_INCLUDE_DIRS}
)
if(TARGET PkgConfig::FFTW3F)
  target_link_libraries(radio PUBLIC PkgConfig::FFTW3F)
else()
  target_link_libraries(radio PUBLIC ${FFTW3F_LIBRARIES})
  if(FFTW3F_LIBRARY_DIRS)
    target_link_directories(radio PUBLIC ${FFTW3F_LIBRARY_DIRS})
  endif()
endif()

if(TARGET PkgConfig::FFTW3F_THR)
  target_link_libraries(radio PUBLIC PkgConfig::FFTW3F_THR)
else()
  find_library(FFTW3F_THREADS_LIBRARY NAMES fftw3f_threads
               HINTS ${FFTW3F_LIBRARY_DIRS})
  if(FFTW3F_THREADS_LIBRARY)
    target_link_libraries(radio PUBLIC ${FFTW3F_THREADS_LIBRARY})
    target_compile_definitions(radio PUBLIC HAVE_FFTW_THREADS=1)
  endif()
endif()

target_link_libraries(radio PUBLIC
    ${BSD_LIBRARIES}
    Threads::Threads
    ${MATH_LIBRARY}
)

# iniparser is included by headers compiled into the 'radio' library,
# so the library itself must carry the include dirs and link lib.
if(INIPARSER_FOUND)
  # Homebrew gives -I.../include/iniparser, while our code includes <iniparser/iniparser.h>.
  # Make both layouts work by adding the parent dir too.
  # Example:
  #   Brew:  .../include/iniparser/iniparser.h    (CFLAGS points to the subdir)
  #   Linux: .../include/iniparser/iniparser.h    (CFLAGS often points to .../include)
  #
  # Always add both: the reported include dir and its parent.
  target_include_directories(radio PUBLIC
    ${INIPARSER_INCLUDE_DIRS}
  )
  foreach(_p IN LISTS INIPARSER_INCLUDE_DIRS)
    get_filename_component(_parent "${_p}" DIRECTORY)
    target_include_directories(radio PUBLIC "${_parent}")
  endforeach()

  target_link_libraries(radio PUBLIC ${INIPARSER_LIBRARIES})
  target_compile_definitions(radio PUBLIC HAVE_INIPARSER=1)
endif()

if(OPUS_FOUND)
  # Always add what pkg-config reported
  target_include_directories(radio PUBLIC ${OPUS_INCLUDE_DIRS})
  target_link_libraries(radio  PUBLIC ${OPUS_LIBRARIES})

  # If pkg-config pointed at .../include/opus (contains opus.h),
  # also add its parent .../include so that <opus/opus.h> resolves.
  foreach(_p IN LISTS OPUS_INCLUDE_DIRS)
    if(EXISTS "${_p}/opus.h" AND NOT EXISTS "${_p}/opus/opus.h")
      get_filename_component(_parent "${_p}" DIRECTORY)
      target_include_directories(radio PUBLIC "${_parent}")
    endif()
  endforeach()
endif()

if(OPUS_FOUND)
  # Always add what pkg-config reported
  target_include_directories(radio PUBLIC ${OPUS_INCLUDE_DIRS})
  target_link_libraries(radio  PUBLIC ${OPUS_LIBRARIES})

  # If pkg-config pointed at .../include/opus (contains opus.h),
  # also add its parent .../include so that <opus/opus.h> resolves.
  foreach(_p IN LISTS OPUS_INCLUDE_DIRS)
    if(EXISTS "${_p}/opus.h" AND NOT EXISTS "${_p}/opus/opus.h")
      get_filename_component(_parent "${_p}" DIRECTORY)
      target_include_directories(radio PUBLIC "${_parent}")
    endif()
  endforeach()
endif()

# --- Avahi (client) include dir normalization + link ---
if(PKG_CONFIG_FOUND)
  pkg_check_modules(AVAHI_CLIENT QUIET avahi-client)
endif()

if(AVAHI_CLIENT_FOUND)
  # Always add what pkg-config reported
  target_include_directories(radio PUBLIC ${AVAHI_CLIENT_INCLUDE_DIRS})
  target_link_libraries(radio  PUBLIC ${AVAHI_CLIENT_LIBRARIES})

  # If a reported dir is .../include/avahi-client (contains client.h),
  # also add its parent .../include so that <avahi-client/client.h> works.
  foreach(_p IN LISTS AVAHI_CLIENT_INCLUDE_DIRS)
    if(EXISTS "${_p}/client.h" AND NOT EXISTS "${_p}/avahi-client/client.h")
      get_filename_component(_parent "${_p}" DIRECTORY)
      target_include_directories(radio PUBLIC "${_parent}")
    endif()
  endforeach()

  target_compile_definitions(radio PUBLIC HAVE_AVAHI=1 HAVE_AVAHI_CLIENT=1)
endif()

# --- Avahi via pkg-config ---
if(PKG_CONFIG_FOUND)
  # Help CMake find a from-source install on macOS
  if(APPLE)
    foreach(_p IN LISTS ENV{PKG_CONFIG_PATH})
      # no-op, just to avoid overriding if user already set it
    endforeach()
    # add common locations if not already present
    foreach(_hint IN ITEMS "/usr/local/lib/pkgconfig" "/opt/local/lib/pkgconfig" "/opt/homebrew/lib/pkgconfig")
      if(EXISTS "${_hint}/avahi-client.pc")
        if(NOT ";$ENV{PKG_CONFIG_PATH};" MATCHES ";${_hint};")
          set(ENV{PKG_CONFIG_PATH} "$ENV{PKG_CONFIG_PATH}:${_hint}")
        endif()
      endif()
    endforeach()
  endif()

  pkg_search_module(AVAHI_CLIENT QUIET avahi-client)
endif()

# Fallback without pkg-config (or if .pc not found)
if(NOT AVAHI_CLIENT_FOUND)
  find_path(AVAHI_CLIENT_INCLUDE_DIR avahi-client/client.h
            HINTS /usr/local/include /opt/local/include /opt/homebrew/include)
  find_library(AVAHI_CLIENT_LIBRARY avahi-client
               HINTS /usr/local/lib /opt/local/lib /opt/homebrew/lib)
  if(AVAHI_CLIENT_INCLUDE_DIR AND AVAHI_CLIENT_LIBRARY)
    set(AVAHI_CLIENT_FOUND TRUE)
    set(AVAHI_CLIENT_INCLUDE_DIRS ${AVAHI_CLIENT_INCLUDE_DIR})
    set(AVAHI_CLIENT_LIBRARIES    ${AVAHI_CLIENT_LIBRARY})
  endif()
endif()

# Wire it into 'radio' (library that compiles avahi.c / avahi.h)
if(AVAHI_CLIENT_FOUND)
  target_include_directories(radio PUBLIC ${AVAHI_CLIENT_INCLUDE_DIRS})
  target_link_libraries(radio  PUBLIC ${AVAHI_CLIENT_LIBRARIES})
  target_compile_definitions(radio PUBLIC HAVE_AVAHI=1 HAVE_AVAHI_CLIENT=1)

  # macOS CI used this flag; keep parity
  if(APPLE)
    target_compile_definitions(radio PUBLIC __APPLE_USE_RFC_2292=1)
  endif()

  # Normalize include dir:
  # If CFLAGS gives .../include/avahi-client (contains client.h),
  # also add its parent .../include so <avahi-client/client.h> resolves.
  foreach(_p IN LISTS AVAHI_CLIENT_INCLUDE_DIRS)
    if(EXISTS "${_p}/client.h" AND NOT EXISTS "${_p}/avahi-client/client.h")
      get_filename_component(_parent "${_p}" DIRECTORY)
      target_include_directories(radio PUBLIC "${_parent}")
    endif()
  endforeach()
else()
  message(STATUS "Avahi not found; proceeding without Avahi (set -DENABLE_AVAHI=OFF to silence this).")
endif()

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
  # On macOS, MODULE bundles must allow symbols (Verbose, Blocktime, etc.) that
  # are defined in the host radiod binary and resolved at runtime via dlopen.
  if(APPLE)
    target_link_options(${_t} PRIVATE -undefined dynamic_lookup)
  endif()
  install(TARGETS ${_t}
          LIBRARY DESTINATION ${KA9Q_LIB_DIR}
          COMPONENT runtime)
endfunction()

if(AIRSPY_FOUND)
  add_driver(airspy SOURCES src/airspy.c INCLUDES ${AIRSPY_INCLUDE_DIRS})
  if(TARGET PkgConfig::AIRSPY)
    target_link_libraries(airspy_drv PRIVATE PkgConfig::AIRSPY)
  else()
    target_link_libraries(airspy_drv PRIVATE ${AIRSPY_LIBRARIES})
    target_link_directories(airspy_drv PRIVATE ${AIRSPY_LIBRARY_DIRS})
  endif()
  # normalize include dir if pkg-config pointed at .../include/libairspy
  foreach(_p IN LISTS AIRSPY_INCLUDE_DIRS)
    if(EXISTS "${_p}/airspy.h" AND NOT EXISTS "${_p}/libairspy/airspy.h")
      get_filename_component(_parent "${_p}" DIRECTORY)
      target_include_directories(airspy_drv PRIVATE "${_parent}")
    endif()
  endforeach()
endif()

if(AIRSPYHF_FOUND)
  add_driver(airspyhf SOURCES src/airspyhf.c INCLUDES ${AIRSPYHF_INCLUDE_DIRS})
  if(TARGET PkgConfig::AIRSPYHF)
    target_link_libraries(airspyhf_drv PRIVATE PkgConfig::AIRSPYHF)
  else()
    target_link_libraries(airspyhf_drv PRIVATE ${AIRSPYHF_LIBRARIES})
    target_link_directories(airspyhf_drv PRIVATE ${AIRSPYHF_LIBRARY_DIRS})
  endif()
  foreach(_p IN LISTS AIRSPYHF_INCLUDE_DIRS)
    if(EXISTS "${_p}/airspyhf.h" AND NOT EXISTS "${_p}/libairspyhf/airspyhf.h")
      get_filename_component(_parent "${_p}" DIRECTORY)
      target_include_directories(airspyhf_drv PRIVATE "${_parent}")
    endif()
  endforeach()
endif()

# Do the same pattern for RTLSDR/HACKRF if you like:
if(RTLSDR_FOUND)
  add_driver(rtlsdr SOURCES src/rtlsdr.c)
  if(TARGET PkgConfig::RTLSDR)
    target_link_libraries(rtlsdr_drv PRIVATE PkgConfig::RTLSDR)
  else()
    target_link_libraries(rtlsdr_drv PRIVATE ${RTLSDR_LIBRARIES})
    target_link_directories(rtlsdr_drv PRIVATE ${RTLSDR_LIBRARY_DIRS})
  endif()
endif()

if(HACKRF_FOUND)
  add_driver(hackrf SOURCES src/hackrf.c)
  if(TARGET PkgConfig::HACKRF)
    target_link_libraries(hackrf_drv PRIVATE PkgConfig::HACKRF)
  else()
    target_link_libraries(hackrf_drv PRIVATE ${HACKRF_LIBRARIES})
    target_link_directories(hackrf_drv PRIVATE ${HACKRF_LIBRARY_DIRS})
  endif()
endif()

if(PORTAUDIO_FOUND AND USB_LIBRARY AND USB_INCLUDE_DIR)
  add_driver(funcube SOURCES src/funcube.c;src/fcd.c;src/hid-libusb.c LIBS ${PORTAUDIO_LIBRARIES} ${USB_LIBRARY} INCLUDES ${PORTAUDIO_INCLUDE_DIRS} ${USB_INCLUDE_DIR})
endif()
if(USB_LIBRARY AND USB_INCLUDE_DIR)
  add_driver(rx888   SOURCES src/rx888.c;src/si5351.c;src/ezusb.c;src/hid-libusb.c LIBS ${USB_LIBRARY} INCLUDES ${USB_INCLUDE_DIR})
endif()

# sig_gen: software signal generator — always built; no hardware required
add_driver(sig_gen SOURCES src/sig_gen.c src/gauss.c
           LIBS ${SAMPLERATE_LIBRARIES}
           INCLUDES ${SAMPLERATE_INCLUDE_DIRS})

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

# ---- Daemons (install to sbin) ----

# radiod: main SDR daemon — loads hardware drivers via dlopen
add_executable(radiod
    src/main.c
    src/audio.c
    src/fm.c
    src/wfm.c
    src/linear.c
    src/spectrum.c
    src/radio.c
    src/radio_status.c
    src/rtcp.c
)
set_target_properties(radiod PROPERTIES ENABLE_EXPORTS TRUE)  # -rdynamic for dlopen
# radio.c returns -1 from a void* function (error path); Clang rejects this as an error
set_source_files_properties(src/radio.c PROPERTIES COMPILE_OPTIONS "-Wno-int-conversion")
target_link_libraries(radiod PRIVATE radio ${USB_LIBRARY} ${DL_LIBRARY})

add_executable(aprs src/aprs.c)
target_link_libraries(aprs PRIVATE radio)

add_executable(aprsfeed src/aprsfeed.c)
target_link_libraries(aprsfeed PRIVATE radio)

add_executable(cwd src/cwd.c)
target_link_libraries(cwd PRIVATE radio)

add_executable(packetd src/packetd.c)
target_link_libraries(packetd PRIVATE radio)

# rx888_boot: firmware uploader; only needed when RX888 is enabled
if(USB_LIBRARY AND USB_INCLUDE_DIR)
  add_executable(rx888_boot src/rx888_boot.c src/ezusb.c)
  target_link_libraries(rx888_boot PRIVATE ${USB_LIBRARY} ${MATH_LIBRARY} Threads::Threads)
  target_include_directories(rx888_boot PRIVATE ${USB_INCLUDE_DIR} ${CMAKE_CURRENT_BINARY_DIR})
endif()

# ---- User tools (install to bin) ----

if(INIPARSER_FOUND AND NCURSES_FOUND)
  add_executable(control src/control.c)
  target_link_libraries(control PRIVATE radio Iniparser::iniparser ${NCURSES_LIBRARIES})
endif()

add_executable(fft-gen src/fft-gen.c)
target_link_libraries(fft-gen PRIVATE radio)

add_executable(jt-decoded src/jt-decoded.c)
target_link_libraries(jt-decoded PRIVATE radio)

add_executable(metadump src/metadump.c)
target_link_libraries(metadump PRIVATE radio)

if(PORTAUDIO_FOUND AND NCURSES_FOUND AND INIPARSER_FOUND)
  add_executable(monitor
      src/monitor.c src/monitor-data.c src/monitor-display.c src/monitor-repeater.c)
  target_include_directories(monitor PRIVATE ${PORTAUDIO_INCLUDE_DIRS} ${SAMPLERATE_INCLUDE_DIRS})
  target_link_libraries(monitor PRIVATE radio
      ${PORTAUDIO_LIBRARIES} ${NCURSES_LIBRARIES} Iniparser::iniparser)
  if(SAMPLERATE_FOUND)
    target_link_libraries(monitor PRIVATE ${SAMPLERATE_LIBRARIES})
  endif()
  if(ALSA_FOUND)
    target_link_libraries(monitor PRIVATE ${ALSA_LIBRARIES})
  endif()
endif()

if(PORTAUDIO_FOUND)
  add_executable(opussend src/opussend.c)
  target_include_directories(opussend PRIVATE ${PORTAUDIO_INCLUDE_DIRS})
  target_link_libraries(opussend PRIVATE radio ${PORTAUDIO_LIBRARIES})
endif()

add_executable(pcmrecord src/pcmrecord.c)
target_link_libraries(pcmrecord PRIVATE radio)
if(OGG_FOUND)
  target_include_directories(pcmrecord PRIVATE ${OGG_INCLUDE_DIRS})
  target_link_libraries(pcmrecord PRIVATE ${OGG_LIBRARIES})
endif()

add_executable(powers src/powers.c)
target_link_libraries(powers PRIVATE radio)

add_executable(tune src/tune.c)
target_link_libraries(tune PRIVATE radio)

add_executable(wd-record src/wd-record.c)
target_link_libraries(wd-record PRIVATE radio)
if(OGG_FOUND)
  target_include_directories(wd-record PRIVATE ${OGG_INCLUDE_DIRS})
  target_link_libraries(wd-record PRIVATE ${OGG_LIBRARIES})
endif()

add_executable(ctcss src/ctcss.c)
target_link_libraries(ctcss PRIVATE radio)

# ---- Secondary tools (built but not installed by default) ----

add_executable(opusd src/opusd.c)
target_link_libraries(opusd PRIVATE radio)
if(MACOS)
  target_compile_options(opusd PRIVATE -include poll.h)
endif()

add_executable(pcmcat src/pcmcat.c)
target_link_libraries(pcmcat PRIVATE radio)

add_executable(pcmspawn src/pcmspawn.c)
target_link_libraries(pcmspawn PRIVATE radio)

if(PORTAUDIO_FOUND)
  add_executable(pcmsend src/pcmsend.c)
  target_include_directories(pcmsend PRIVATE ${PORTAUDIO_INCLUDE_DIRS})
  target_link_libraries(pcmsend PRIVATE radio ${PORTAUDIO_LIBRARIES})
endif()

add_executable(rdsd src/rdsd.c)
target_link_libraries(rdsd PRIVATE radio)

add_executable(setfilt src/setfilt.c)
target_link_libraries(setfilt PRIVATE radio)

add_executable(show-pkt src/show-pkt.c)
target_link_libraries(show-pkt PRIVATE radio ${NCURSES_LIBRARIES})

add_executable(show-sig src/show-sig.c)
target_link_libraries(show-sig PRIVATE radio ${NCURSES_LIBRARIES})

add_executable(stereod src/stereod.c)
target_link_libraries(stereod PRIVATE radio)

# ---- Install rules ----

# Daemons → sbin
foreach(_t IN ITEMS radiod aprs aprsfeed cwd packetd)
  if(TARGET ${_t})
    install(TARGETS ${_t} DESTINATION ${KA9Q_SBIN_DIR} COMPONENT runtime)
  endif()
endforeach()
if(TARGET rx888_boot)
  install(TARGETS rx888_boot DESTINATION ${KA9Q_SBIN_DIR} COMPONENT runtime)
endif()

# User tools → bin
foreach(_t IN ITEMS
    control ctcss fft-gen jt-decoded metadump monitor opussend
    pcmrecord pl powers tune wd-record)
  if(TARGET ${_t})
    install(TARGETS ${_t} DESTINATION ${KA9Q_BIN_DIR} COMPONENT runtime)
  endif()
endforeach()

