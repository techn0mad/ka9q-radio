#pragma once
//
// libusb header, whose location is not consistent across platforms.
//
// Linux (libusb-1.0-0-dev) and MacPorts install it as
// <libusb-1.0/libusb.h>. FreeBSD ships libusb in the base system with the
// header directly at <libusb.h>, no subdirectory. Include this instead of
// either spelling.
//
// __has_include is supported by clang and by GCC 5 and later, which covers
// every compiler this tree is built with.
//
#if __has_include(<libusb.h>)
#  include <libusb.h>                 // FreeBSD base, OpenBSD
#elif __has_include(<libusb-1.0/libusb.h>)
#  include <libusb-1.0/libusb.h>      // Linux, MacPorts, Homebrew
#else
#  error "No usable libusb header found; install the libusb 1.0 development headers"
#endif
