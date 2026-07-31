#pragma once
//
// Socket API headers, gathered so every consumer gets a consistent set.
//
// Glibc's <sys/socket.h> pulls in <netinet/in.h> and <arpa/inet.h>
// transitively, so on Linux including it alone happens to be enough. FreeBSD's
// does not, and code that names IPPROTO_UDP, struct sockaddr_in or inet_ntop
// fails to compile there with nothing more than <sys/socket.h> in scope.
//
// All four headers below are POSIX and present on Linux, macOS, FreeBSD and
// OpenBSD, so no feature detection is needed. Include this instead of
// <sys/socket.h>.
//
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
