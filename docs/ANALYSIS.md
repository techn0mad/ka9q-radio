`main`  in  `src/main.c`  is the entry. It parses args, sets signals, calls `loadconfig()`, then
loops to print CPU usage.

`loadconfig` in `src/radio.c` parses a config file or directory. It sets up global parameters,
initializes SDR hardware via a dynamically loaded `.so` file, and then concurrently processes
each receiver channel section. For each channel, it creates a template, applies settings from
global, preset, and channel-specific configs, then creates channels based on `freq` and `raster`
entries. A new thread is spawned for each channel, running a demodulator loop. The core is a
multi-threaded system driven by the config file.

`radio.h` defines `frontend` (SDR hardware with parameters, function pointers, and shared
convolver) and `channel` (receiver state including tuning, filtering, demodulation, signal
levels, output streams, and threads). It also declares functions from various source files.

`src/filter.c` implements the DSP core, using [FFTW3](https://www.fftw.org/) for
[fast convolution via the Overlap-Save method](https://www.iro.umontreal.ca/~mignotte/IFT3205/Documents/TipsAndTricks/MultibandFilterbank.pdf).
It employs a  `filter_in`/`filter_out` architecture where multiple output filters can
share a single, threaded input FFT for efficiency. `create_filter_input` and
`create_filter_output` initialize these stages, while `execute_filter_input` queues FFT jobs and
`execute_filter_output`  applies the filter response after FFT completion. The `set_filter`
function designs filters with a [Kaiser-windowed sinc](https://en.wikipedia.org/wiki/Kaiser_window).
`radio.c`'s  downconvert  function utilizes this framework for down-conversion in what is a
very sophisticated multi-channel receiver design.

Here is a more detailed breakdown:

• Core Radio Logic (`src/`): This is the C source for the main application and
related tools. It can be broken down further:
  • Main Application: `main.c` (entry point), `radio.c` (configuration loading, channel
  management), and `config.c` (INI file parsing helpers).
  • DSP Core: `filter.c` is the heart of the signal processing, implementing the fast convolution
  engine with FFTW3.
  • Hardware Drivers: Files like `airspy.c`, `rx888.c`, `rtlsdr.c`, etc., are the hardware-
  specific backends that are dynamically loaded by `radio.c`.
  • Demodulators: The actual demodulation logic (FM, linear, etc.) is implemented in files like
  `demod_fm.c` and `demod_linear.c`, which are called by the `demod_thread` in `radio.c`.
  • Networking: `multicast.c` and `rtp.c` handle the streaming of processed audio/IQ data over
  the network.
  • Control/Status: `radio_status.c` manages the command and control protocol.
• Configuration Files (`config/`): This contains runtime configurations for the daemons.
  • The structure `radiod@instance.conf` and `radiod@instance.conf.d/` is a key pattern. The
  `loadconfig` function in `radio.c` can either parse a single file or concatenate all `.conf`
  files within a corresponding `.d` directory.
  • `presets.conf` is loaded to define reusable demodulator modes (`am`, `ssb`, `wfm`, etc.).
  • `fragments/`  holds reusable snippets that can be included in the main configurations.
• Hardware Rules (`rules/`): These are [`udev`](https://en.wikipedia.org/wiki/Udev) rules for Linux,
which are necessary to grant the `ka9q-radio` user permission to access the USB-based SDR
hardware without running as root.
• [Systemd](https://en.wikipedia.org/wiki/Systemd) Services (`service/`): These are `systemd`
unit files for running `radiod` and other tools as background daemons. The use of the `@` symbol
in `radiod@.service` indicates a template unit, allowing you to run multiple independent instances
managed by `systemd`  (e.g., `systemctl start radiod@airspy`).
• Auxiliary Scripts (`aux/`): This is a collection of helper scripts and configuration
snippets for system integration, such as `sysctl` settings for network buffers (`98-sockbuf.conf`
) and compilation helpers.
• Documentation (`docs/`): A mix of user guides, deep-dive technical explanations
of the theory behind the code, and API documentation.

## Technical Context

• Architecture decisions made and why
  • Configuration-Driven: The entire radio's behavior is defined in `.ini` files, not hardcoded.
  This allows for extreme flexibility without recompilation.
  • Dynamic Hardware Abstraction: SDR hardware support is provided via dynamically loaded shared
  objects (`.so` files). This decouples the core radio logic from specific hardware, making it
  easy to add support for new SDRs.
  • Massively Concurrent: The application is heavily multi-threaded. A separate thread is created
  for each receiver "channel," allowing a single `radiod` instance to demodulate hundreds or
  thousands of signals simultaneously if the hardware and CPU can support it.
  • Efficient DSP Core: The use of the Overlap-Save (fast convolution) method with FFTW3 is a key
  decision. Instead of performing a separate [FFT](https://en.wikipedia.org/wiki/Fast_Fourier_transform)
  for each channel, a single, large "master" FFT is performed on the wideband input from the SDR.
  Each channel then acts as a "slave," applying its specific filter in the frequency domain before
  performing a smaller inverse FFT (IFFT). This is vastly more CPU-efficient for a large number of
  channels than brute-force approaches.
• Patterns being followed (with examples)
  • Master/Slave Filter (`filter.c`): A single `struct filter_in` (master) handles the initial
  forward FFT on the raw SDR data. Multiple `struct filter_out` (slaves) can subscribe to this
  master. Each slave applies its unique frequency shift and filter response before performing its
  own inverse FFT. This pattern is central to the application's efficiency.
  • Template Unit Files (`service/radiod@.service`): Systemd services are templated, allowing
  multiple, independent instances of `radiod` to run concurrently, managed by systemd (e.g.,
  `systemctl start radiod@airspy` and `systemctl start radiod@rx888`).
  • Hierarchical Configuration (`radio.c:process_section`): Channel settings are built up in
  layers. The final configuration for a channel is a result of applying settings in order of
  precedence: compiled-in defaults ->  [global]  settings ->  preset  settings -> channel-
  specific settings.
• Libraries/frameworks being used
  •  [fftw3](https://github.com/FFTW/fftw3): The core library for performing Fast Fourier Transforms.
  •  [iniparser](https://gitlab.com/iniparser/iniparser): Used to parse the `.ini` -style configuration files.
  •  [pthreads](https://pubs.opengroup.org/onlinepubs/9799919799/idx/threads.html): Used for 
  the multi-threaded architecture.
  •  [dlfcn.h](https://pubs.opengroup.org/onlinepubs/7908799/xsh/dlfcn.h.html) (`dlopen`, `dlsym`): 
  Used for dynamically loading the hardware drivers.

### Portable Code & Components

The core application is highly portable across POSIX-compliant operating systems (like Linux,
macOS, and BSD).

•  `src/`: This directory contains the entire C source code for the radio daemon and its utilities
  • Portability Mechanism: It uses the Autotools build system (configure  script,  `Makefile.am` /
  `Makefile.in`), which is the standard for building C projects on Unix-like systems.
  • Dependencies: It relies on common, cross-platform libraries like `fftw3` (for DSP), `pthreads`
  (for multithreading), and `libusb` (for hardware access), which are available on most platforms
  • Dynamic Loading: Hardware drivers are loaded as shared objects (`.so` files) using the
  standard `dlopen()` function, a POSIX-standard feature.
•  `config/` : These are the `.ini`  configuration files that define the radio's behavior. The
format is plain text and is parsed by the portable C code, making the configurations themselves
portable.
•  `share/` : Contains architecture-independent data files like default presets and help text.
•  `docs/` : Documentation files.

### OS-Specific Support Files

These components are designed for modern Linux distributions and would need to be adapted for
other operating systems.

•  `service/`: Contains `systemd` service files (`.service`).
  • Purpose: These files automate running `radiod` as a background daemon on Linux.
  • OS-Specificity: Systemd is specific to Linux. macOS uses `launchd`, and BSD systems have `rc.d`.
  These files would need to be rewritten for other OSes.
•  `rules/`: Contains `udev` rules files (`.rules`).
  • Purpose: These files automatically set the correct device permissions when an SDR is plugged
  into a Linux machine, allowing the radio software to access it without running as root.
  • OS-Specificity: `udev` is the device manager for the Linux kernel. Other operating systems have
  different mechanisms for device drivers and permissions.
•  `aux/`: A collection of helper scripts and configuration snippets primarily for Linux system
administration.
  • Examples: `98-sockbuf.conf` is a `sysctl` setting for the Linux kernel. `ka9q-radio.tmpfiles`
  is for `systemd-tmpfiles`. These are not applicable to non-Linux systems.
•  `src/Brewfile`: This is a configuration file for Homebrew, a package manager for macOS. It is
a convenience file for developers on macOS to install dependencies and is not part of the
application itself.

In summary, the core radio application in `src/` is designed to be portable. The surrounding
`service`, `rules`, and `aux` directories provide integration and automation for Linux-based
systems using `systemd`. To run the application on another OS like macOS or a BSD variant, you
would compile the code from `src/`  and then manually create the equivalent service/daemon and
device permission files for that specific OS.

Project Structure System

## Files & Changes

• Files that were modified (with brief description of changes)

• Files that were read/analyzed (why they're relevant)
  • For Core Logic Analysis:
    • `src/main.c`: Identified the application entry point (`main`) and the initial call to
    `loadconfig()`.
    • `src/radio.c`: Analyzed as the core application logic. The `loadconfig()` function was
    traced to understand the dynamic loading of hardware drivers (`.so` files via `dlopen`) and
    the spawning of concurrent threads for each receiver channel.
    • `src/radio.h`: Examined to understand the key data structures: `struct frontend` (SDR
    hardware) and `struct channel` (individual receiver).
    • `src/filter.c`: Identified as the core DSP engine, using an efficient Overlap-Save (fast
    convolution) algorithm with FFTW3.
  • For Portability Analysis:
    • The entire repository was listed recursively via `ls -R` to provide a complete view of all
    files. This allowed for the categorization of directories like `src` (portable code),
    `service` (Linux-specific `systemd` files), `rules` (Linux-specific `udev` files), and `aux`
    (Linux-specific helper scripts).

• File paths and line numbers for important code locations
  •  `src/main.c:105` : The `loadconfig()` call that initiates all radio configuration.
  •  `src/radio.c:554` : The call to `dlopen()`  which dynamically loads the hardware-specific
  shared object driver.
  •  `src/radio.c:648` : The `process_section()` function, which is executed in a new thread for
  each configured receiver channel, enabling massive concurrency.
  •  `src/radio.c:981` : The main `demod_thread()` loop for an individual receiver channel.
  •  `src/filter.c:672` : The `execute_filter_output()` function, containing the core DSP operation
  of applying a filter in the frequency domain and performing an inverse FFT.


## Technical Context

• Architecture decisions made and why
  • Configuration-Driven: The radio is defined entirely by `.ini` files, allowing for high
  flexibility without recompiling the C code.
  • Dynamic Hardware Abstraction: Hardware support is provided by `.so` files loaded at runtime.
  This decouples the core logic from hardware drivers, making it easy to extend.
  • Massively Concurrent: Each receiver channel runs in its own thread (`pthreads`), allowing a
  single daemon to handle hundreds or thousands of channels simultaneously.
  • Efficient DSP Core: A "Master/Slave" FFT architecture is used. A single large FFT is
  performed on the raw SDR data (Master). Each channel then applies its filter in the frequency
  domain before performing a smaller, cheaper inverse FFT (Slave). This is vastly more CPU-
  efficient than performing a large FFT for every channel.
• Patterns being followed (with examples)
  • Master/Slave Filter (`filter.c`): A single `struct filter_in` (master) provides the FFT'd
  wideband spectrum to many `struct filter_out` (slaves).
  • Hierarchical Configuration: Settings are applied in layers (defaults -> global -> preset ->
  channel), allowing for fine-grained control and reuse of configuration snippets.
  • Templated Unit Files (`service/radiod@.service`): Systemd service templates are used to allow
  multiple, independent  radiod  instances to be managed by the OS (e.g., one for each physical
  SDR device).
• Libraries/frameworks being used
  •  fftw3 : For Fast Fourier Transforms, the core of the DSP engine.
  •  pthreads : For the multi-threaded, per-channel architecture.
  •  dlfcn.h  (`dlopen`, `dlsym`): For dynamic loading of hardware drivers.
  •  iniparser : For parsing `.ini`  configuration files.
  •  libusb : For direct communication with USB-based SDR hardware.

## Strategy & Approach

• Key insights or gotchas discovered
  • The application's high performance and scalability stem directly from the Master/Slave FFT
  architecture in `filter.c`.
  • The codebase has a very clean and strong separation of concerns: The portable C application
  in `src/`  is distinct from the Linux/systemd integration scripts in `service/`, `rules/`, and
  `aux/`. This makes porting to another POSIX-compliant OS (like macOS or BSD) a straightforward
  task of rewriting the integration scripts, not the core application.


Data Flows

Based on the previous analysis, here are the major data flows within the  ka9q-radio
architecture:

1. SDR Hardware -> Ring Buffer (Input Flow)
  • The hardware-specific driver (e.g., `airspy.so`, loaded via `dlopen` in `src/radio.c:554`)
  continuously captures raw I/Q samples from the SDR device over USB.
  • These samples are written into a large, shared ring buffer (`struct frontend->rb`) to
  decouple the hardware capture rate from the DSP processing rate. This is a classic producer-
  consumer pattern where the hardware driver is the producer.
2. Ring Buffer -> Master FFT (DSP Ingress)
  • A dedicated thread for the "Master Filter" (`master_filter_thread` in `src/radio.c:881`) acts
  as the consumer.
  • It reads blocks of I/Q samples from the ring buffer.
  • It executes the "Master FFT" (`fftwf_execute` in `src/filter.c:641`), converting the wideband
  time-domain samples into the frequency domain. This single, large FFT is the input for all
  receiver channels.
3. Master FFT -> Slave IFFT (Per-Channel Filtering)
  • The core  demod_thread  for each channel (`src/radio.c:981`) pulls the frequency-domain data
  from the Master FFT.
  • It applies its specific channel filter (e.g., a 2.4 kHz SSB filter) by performing a complex
  multiplication in the frequency domain. This is computationally cheap.
  • It then performs a much smaller Inverse FFT (`execute_filter_output` in `src/filter.c:672`)
  to convert the now-filtered, narrow-band signal back into the time domain.
4. Filtered I/Q -> Demodulator -> Audio Buffer (Processing)
  • The filtered I/Q samples are fed into a specific demodulator function (e.g., `demod_ssb`,
  `demod_wfm`) within the channel's `demod_thread`.
  • The demodulator processes the I/Q data into an audio stream (or other data), which is placed
  into another ring buffer, this time specific to the channel (`struct channel->audio_rb`).
5. Audio Buffer -> Network (Egress to Clients)
  • A network output function (e.g., `audio_udp_send` in `src/audio.c`) reads audio samples from
  the channel's audio ring buffer.
  • It packetizes the audio (often using RTP) and sends it over the network via a UDP socket to
  one or more connected clients.
6. Client -> Control Socket (Command & Control)
  • A separate, low-bandwidth control flow exists. Client applications can send commands (e.g.,
  "change frequency to 14.205 MHz") to the daemon.
  • The daemon listens on a control socket, parses these commands, and adjusts the parameters of
  the corresponding channel `struct` in real-time (e.g., changing the frequency offset for the
  filter in the frequency domain).

This architecture minimizes CPU load by performing the most expensive operation (the wideband
FFT) only once, and then using cheap operations for each of the hundreds or thousands of
concurrent channels.

Client programs connect to the `radiod` daemon through a UDP-based control protocol. The
implementation is primarily in `src/control.c`.

Here is a breakdown of the mechanism:

1. Socket Creation: In `src/main.c`, the  main `function` calls `control_thread()`, which is the
entry point for the control logic. Inside `control_thread()`, a UDP socket is created for control
commands.
2. Binding and Listening: The socket binds to a port specified in the configuration, typically
discovered via Avahi/mDNS for service discovery, making it a zero-configuration setup for clients
on the same network. The `control_thread` then enters a `select()` loop, waiting for packets on
the control socket file descriptor.
3. Command Reception: When a client sends a command, it's a UDP packet. The `select()` loop wakes
up, and the packet is read from the socket in `src/control.c`.
4. Command Parsing: The received UDP packet is passed to the `parse_control_packet()` function (a
simplified name for the logic within the `control_thread`'s loop). This function is a large
switch  statement that interprets the command based on a command code in the packet header.
5. Execution: Based on the parsed command, the daemon acts. For example, a "set frequency"
command would cause the daemon to find the specified channel  struct  and update its  frequency
member. This change is then picked up by the channel's  demod_thread  on its next processing loop

This UDP-based approach is stateless and lightweight, suitable for a soft real-time control plane
where occasional packet loss is not catastrophic; a client can simply re-send a command if it
doesn't see the expected change.


mDNS Service Discovery

The rendezvous process uses mDNS (Multicast DNS) service discovery, a core component
of zeroconf networking. On Linux, this is almost always implemented using the Avahi daemon and
library. The goal is to allow a client to discover the IP address and port of the  radiod
control socket without any manual configuration.

The process happens in two distinct phases: the server's announcement and the client's discovery.

### 1. Server-Side: Announcing the  radiod  Service

When the `radiod` daemon starts, it registers itself with the local Avahi daemon, effectively
announcing its presence on the network.

1. Create an Avahi Client: The application first connects to the Avahi daemon running on the
system.
2. Create an Entry Group: It then creates a new "entry group," which is a container for one or
more service records that should be published or removed atomically.
3. Add Service to Group: The daemon adds the specific details of its control service to this
group. This is the critical step and includes:
  • Service Name: A user-friendly instance name, often configurable (e.g., "SDR Receiver on
  Raspberry Pi").
  • Service Type: A standardized string that clients will search for. For this project, it would
  be something like `_ka9q-radio._udp`. The `_udp` suffix is standard, indicating the transport
  protocol.
  • Domain: Usually `.local`, the standard for mDNS.
  • Host and Port: The port number the control socket is listening on. Avahi automatically
  substitutes the machine's current hostname and IP address(es).
4. Commit the Group: The daemon "commits" the entry group. This signals the Avahi daemon to start
multicasting DNS packets onto the local network (to address  224.0.0.251 ), announcing that a
service of type `_ka9q-radio._udp` is available at a specific IP and port.

This announcement is periodically re-broadcast as long as `radiod` is running. When `radiod`
shuts down cleanly, it instructs Avahi to withdraw the announcement.

### 2. Client-Side: Discovering the  radiod  Service

When a client application (e.g., a GUI controller) starts, it needs to find all available `radiod`
servers.

1. Create an Avahi Client: Like the server, the client connects to its local Avahi daemon.
2. Create a Service Browser: The client creates a "service browser" configured to look
specifically for services of type `_ka9q-radio._udp` on the `.local` domain.
3. Implement Browser Callback: The client provides a callback function to the service browser.
Avahi will invoke this function asynchronously whenever a matching service is found or disappears
from the network.
  • On Service "Add": When the callback is triggered for a new service, the client receives the
  service's name, type, and domain. However, it does not yet have the IP address or port.
4. Create a Service Resolver: Inside the "Add" callback, the client tells Avahi, "You found a
service I'm interested in; now please resolve its details." It creates an  AvahiServiceResolver
for that specific service instance.
5. Implement Resolver Callback: The client provides a second callback function to the resolver.
Avahi invokes this function once it has successfully looked up the service's full records (its
SRV, TXT, and A/AAAA records from the mDNS multicast).
  • On Successful Resolution: This callback is the final step of the rendezvous. It provides the
  client with the server's hostname, IP address, and port number.

With the IP address and port in hand, the client can now create its own UDP socket and begin
sending control packets directly to the  radiod  daemon, completing the connection. This entire
process allows a client to populate a list of available servers on the network automatically.


Avahi ==> mDNSResponder Migration

Migrating from native Avahi calls to the mDNSResponder compatible API is the
correct strategy for portability across Linux, macOS, and BSDs.

First, a key clarification: Avahi provides a compatibility library (`avahi-compat-libdns_sd`)
that exposes the exact same C API as Apple's `mDNSResponder`. You don't target the "mDNSResponder
interface provided by Avahi"; rather, you write your code against the standard `dns_sd.h` API,
and the build system links it to the appropriate backend on each OS:

• On Linux: It links to the Avahi `avahi-compat-libdns_sd` compatibility library.
• On macOS: It links to the native mDNSResponder framework.
• On FreeBSD/other BSDs: It links to the installed mDNSResponder port.

This means you can have a single, clean source file for mDNS logic that works everywhere, without
any `#ifdef`s for the OS in the application logic itself. The difference is handled entirely by
the linker.

Here are the required steps to perform this migration:

--------

### Step 1: Abstract the mDNS/DNS-SD Interface

Instead of replacing the Avahi code directly in `main.c`  or `control.c`, create a simple
abstraction. This is a good practice and makes the transition cleaner.

Create a new header, e.g., `discovery.h`:

```
  // discovery.h

  #ifndef DISCOVERY_H
  #define DISCOVERY_H

  // Opaque handle for the discovery service context
  typedef struct DiscoveryService DiscoveryService;

  // Callbacks for the client
  typedef void (*ServiceFoundCallback)(const char *name, const char *ip_address, uint16_t port,
void *context);
  typedef void (*ServiceRemovedCallback)(const char *name, void *context);

  // For the server: Register a service
  DiscoveryService* discovery_register_service(const char *service_name, uint16_t port);

  // For the client: Browse for services
  DiscoveryService* discovery_browse_services(ServiceFoundCallback on_found,
ServiceRemovedCallback on_removed, void *user_context);

  // Get the file descriptor to add to the main select() loop
  int discovery_get_fd(DiscoveryService *service);

  // Process events when the file descriptor is ready
  void discovery_process_events(DiscoveryService *service);

  // Clean up and shut down
  void discovery_free(DiscoveryService *service);

  #endif // DISCOVERY_H
```

### Step 2: Rewrite Service Registration (Server-Side) using DNS-SD API

The current Avahi code for publishing a service involves creating clients, entry groups, adding
services, and committing them. The DNS-SD API simplifies this to a single primary function call.

The new implementation file, `discovery_dnssd.c`, would implement `discovery_register_service()`
like this:

```
  // In discovery_dnssd.c

  #include <dns_sd.h>
  #include <arpa/inet.h> // for htons

  // The actual implementation
  struct DiscoveryService {
      DNSServiceRef sdRef;
  };

  // Callback for the registration event
  static void register_callback(DNSServiceRef sdRef, DNSServiceFlags flags, DNSServiceErrorType
errorCode, const char *name, const char *regtype, const char *domain, void *context) {
      if (errorCode != kDNSServiceErr_NoError) {
          fprintf(stderr, "DNS-SD Registration failed: %d\n", errorCode);
      } else {
          printf("DNS-SD Service registered as '%s' in domain '%s'\n", name, domain);
      }
  }

  DiscoveryService* discovery_register_service(const char *service_name, uint16_t port) {
      DiscoveryService *ds = malloc(sizeof(DiscoveryService));
      if (!ds) return NULL;

      DNSServiceErrorType err = DNSServiceRegister(
          &ds->sdRef,                  // The handle to be initialized
          0,                           // No flags
          kDNSServiceInterfaceIndexAny,// All network interfaces
          service_name,                // User-friendly instance name
          "_ka9q-radio._udp",          // Service type
          "local",                     // Domain (use NULL for default registration domains)
          NULL,                        // Host (use NULL for localhost)
          htons(port),                 // Port number in network byte order
          0,                           // TXT record length (0 for none)
          NULL,                        // TXT record (none)
          register_callback,           // Callback for status
          NULL                         // No user context for this simple callback
      );

      if (err != kDNSServiceErr_NoError) {
          fprintf(stderr, "DNSServiceRegister call failed: %d\n", err);
          free(ds);
          return NULL;
      }
      return ds;
  }
```

### Step 3: Rewrite Service Discovery (Client-Side) using DNS-SD API

Client-side discovery is a two-step process with the DNS-SD API: `DNSServiceBrowse` to find
services and `DNSServiceResolve` to get the address and port for a specific one. This replaces
the Avahi service browser and resolver logic.

```
  // In discovery_dnssd.c - continued

  // Forward declarations for callbacks
  static void resolve_callback(DNSServiceRef, DNSServiceFlags, uint32_t, DNSServiceErrorType,
const char *, const char *, uint16_t, uint16_t, const unsigned char *, void *);
  static void browse_callback(DNSServiceRef, DNSServiceFlags, uint32_t, DNSServiceErrorType,
const char *, const char *, const char *, void *);

  // ... implementation of discovery_browse_services() would call DNSServiceBrowse()
  // with browse_callback as its callback.

  // Browse callback: Called when a service appears or disappears
  static void browse_callback(DNSServiceRef sdRef, DNSServiceFlags flags, uint32_t interfaceIndex
DNSServiceErrorType errorCode, const char *serviceName, const char *regtype, const char
*replyDomain, void *context) {

      if (errorCode != kDNSServiceErr_NoError) return;

      if (flags & kDNSServiceFlagsAdd) {
          // A service was found, now resolve its address and port
          DNSServiceRef resolveRef;
          DNSServiceResolve(&resolveRef, 0, interfaceIndex, serviceName, regtype, replyDomain,
resolve_callback, context);
          // NOTE: In a real implementation, you'd need to manage the lifecycle of this
resolveRef
      } else {
          // A service was removed
          ServiceRemovedCallback on_removed = (ServiceRemovedCallback)context; // Simplified
          if (on_removed) on_removed(serviceName, context);
      }
  }

  // Resolve callback: Called with the final IP address and port
  static void resolve_callback(DNSServiceRef sdRef, DNSServiceFlags flags, uint32_t
interfaceIndex, DNSServiceErrorType errorCode, const char *fullname, const char *hosttarget,
uint16_t port, uint16_t txtLen, const unsigned char *txtRecord, void *context) {

      if (errorCode != kDNSServiceErr_NoError) return;

      // NOTE: This only gives the hostname. A final step is needed to get the IP.
      // A proper implementation would use DNSServiceGetAddrInfo to resolve hosttarget -> IP.
      // For simplicity, we'll assume the callback context has what it needs.
      ServiceFoundCallback on_found = (ServiceFoundCallback)context; // Simplified
      if (on_found) on_found(fullname, "127.0.0.1", ntohs(port), context); // IP is placeholder
  }
```

### Step 4: Integrate with the Main Event Loop

The DNS-SD API is asynchronous. The functions above return immediately. You must integrate them
into the application's `select()` loop.

1. Get File Descriptor: `DNSServiceRefSockFD()` will return the file descriptor associated with
your `DNSServiceRef` handle. This is what `discovery_get_fd()` will do.
2. Add to `select()`: Add this file descriptor to the `fd_set` in your main loop.
3. Process Results: When `select()` indicates that the descriptor is readable, you must call
`DNSServiceProcessResult()` with the `DNSServiceRef`. This is what `discovery_process_events()`
will do. This function will, in turn, trigger the callbacks you defined (`register_callback`,
`browse_callback`, etc.).

### Step 5: Update the Build System (e.g.,  Makefile  or  CMake )

This is the final and most critical step for portability.

• Include Header: The code should now `#include <dns_sd.h>`.
• Linker Flags:
  • For Linux: Use `pkg-config` to check for `avahi-compat-libdns_sd`. If it exists, add 
  `-ldns_sd`  to the linker flags. If not, the user needs to install it (e.g., `sudo apt-get install
  libavahi-compat-libdnssd-dev`).
  • For macOS: No special flags are needed. The header and library are part of the base system
  and the compiler/linker will find them automatically.
  • For FreeBSD: The build system should check for the presence of the `mDNSResponder_nossl`
  package. If installed, the headers and library will be in the standard search paths 
  (`/usr/local/include`, `/usr/local/lib`), so linking with `-ldns_sd` should work.

By following these steps, the core application logic becomes completely agnostic to the
underlying mDNS implementation, achieving true portability.


Build/Packaging System

Excellent question. Moving from a rudimentary build system to a modern, cross-platform one is
critical for maintainability and collaboration. The best approach involves two main components:

1. A Meta-Build System to handle compilation and linking across different platforms.
2. A Packaging Strategy tailored to the conventions of each target OS.

For this project, my strong recommendation is to use CMake. It is the de-facto industry standard
for cross-platform C/C++ projects and is perfectly suited to handle the challenges of this
codebase.

--------

### Recommended Approach: CMake + OS-Native Packaging

This approach provides a single, unified way to build the source code (`CMakeLists.txt`) while
producing packages that feel native on each target system (`.deb`, Homebrew formula, FreeBSD
port).

#### Phase 1: Migrating the Build Logic to CMake

The goal is to create a `CMakeLists.txt` file in the root of the repository that describes how to
build, link, and install everything.

1. Create the Root `CMakeLists.txt`

This file will define the project, find dependencies, and build the main `radiod` executable.

```
  # Minimum CMake version required
  cmake_minimum_required(VERSION 3.15)

  # Define the project
  project(ka9q-radio VERSION 1.0 LANGUAGES C)

  # --- Find Dependencies ---
  # Use pkg-config to find most libraries, which is standard on Linux/BSD
  find_package(PkgConfig REQUIRED)

  # Find necessary libraries
  pkg_check_modules(FFTW3f REQUIRED fftw3f)
  pkg_check_modules(LIBUSB REQUIRED libusb-1.0)
  pkg_check_modules(INIPARSER REQUIRED iniparser)

  # Pthreads is handled specially by CMake
  find_package(Threads REQUIRED)

  # --- Find the mDNS/DNS-SD Library (Platform-Specific) ---
  # On Linux, we want the Avahi compatibility layer. On macOS/BSD, it's native.
  if(APPLE)
      # On macOS, dns_sd is part of the system framework.
      # This is often found automatically, but we can be explicit if needed.
      # For now, we assume the compiler finds it.
      set(DNS_SD_LIBRARIES "") # Handled by the linker implicitly
  else()
      # On Linux/BSD, find it via pkg-config
      pkg_check_modules(DNS_SD REQUIRED avahi-compat-libdns_sd)
  endif()


  # --- Build the Main Executable ---
  add_executable(radiod
      src/main.c
      src/radio.c
      src/filter.c
      src/control.c
      # ... add all other .c files for the main binary
  )

  # Link the executable against the found libraries
  target_link_libraries(radiod PRIVATE
      ${FFTW3f_LIBRARIES}
      ${LIBUSB_LIBRARIES}
      ${INIPARSER_LIBRARIES}
      Threads::Threads
      ${DNS_SD_LIBRARIES}
      m # Math library
      dl # Dynamic loading library
  )

  # Add include directories
  target_include_directories(radiod PRIVATE
      ${FFTW3f_INCLUDE_DIRS}
      ${LIBUSB_INCLUDE_DIRS}
      ${INIPARSER_INCLUDE_DIRS}
      ${DNS_SD_INCLUDE_DIRS}
  )

  # --- Build Hardware Drivers (Shared Libraries) ---
  # Create a function or loop to build each .so driver
  file(GLOB DRIVER_SOURCES "src/drivers/*.c")
  foreach(driver_source ${DRIVER_SOURCES})
      get_filename_component(driver_name ${driver_source} NAME_WE)
      add_library(${driver_name} SHARED ${driver_source})
      # Drivers may need to link against the same libs
      target_link_libraries(${driver_name} PRIVATE ${LIBUSB_LIBRARIES})
  endforeach()

  # --- Installation Rules ---
  # This tells 'make install' where to put the files
  install(TARGETS radiod DESTINATION bin)
  install(TARGETS airspy # Add all other driver names here
          DESTINATION lib/ka9q-radio/drivers)

  # Install configuration files
  install(FILES config.ini.dist DESTINATION etc/ka9q-radio)

  # Install systemd/udev rules ONLY on Linux
  if(UNIX AND NOT APPLE)
      install(FILES service/radiod.service DESTINATION lib/systemd/system)
      install(FILES rules/88-airspy.rules DESTINATION lib/udev/rules.d)
  endif()
```

2. The Build Process for a Developer would now be:

```
  mkdir build
  cd build
  cmake ..
  make
  sudo make install
```

This is a standard, clean, out-of-source build process that works identically on all three
platforms.

#### Phase 2: Platform-Specific Packaging

With a robust `install` stage in CMake, creating native packages becomes much simpler.

1. Debian/Ubuntu (`.deb` package)

You can use CPack, which is CMake's built-in packaging tool. Add this to the end of your
CMakeLists.txt :

  # --- CPack Configuration for Debian Packages ---
  set(CPACK_GENERATOR "DEB")
  set(CPACK_PACKAGE_NAME "ka9q-radio")
  set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
  set(CPACK_PACKAGE_VENDOR "KA9Q")
  set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Your Name <you@example.com>")
  # Crucially, list the package dependencies
  set(CPACK_DEBIAN_PACKAGE_DEPENDS "libfftw3-dev, libusb-1.0-0-dev, libiniparser-dev, libavahi-
compat-libdnssd-dev")
  include(CPack)

Now, after running `cmake` and `make`, you can simply run:

  `cpack`

This will generate a perfect `.deb` file, ready for distribution.

2. macOS (Homebrew)

Homebrew is the standard. You don't build a binary package; you write a "Formula" (a Ruby script)
that tells Homebrew how to build the project from source using your new CMake system.

Create a file `ka9q-radio.rb`:

```
  class Ka9qRadio < Formula
    desc "A massively concurrent SDR receiver daemon"
    homepage "https://github.com/ka9q/ka9q-radio"
    url "https://github.com/ka9q/ka9q-radio/archive/refs/tags/v1.0.tar.gz" # URL to a source
tarball
    sha256 "..." # SHA256 of the tarball

    depends_on "cmake" => :build
    depends_on "pkg-config" => :build
    depends_on "fftw"
    depends_on "libusb"
    depends_on "iniparser"

    def install
      system "cmake", "-S", ".", "-B", "build", *std_cmake_args
      system "cmake", "--build", "build"
      system "cmake", "--install", "build"
    end

    test do
      # A simple test to see if the binary runs
      system "#{bin}/radiod", "--version"
    end
  end
```

Users can then install it with `brew install ka9q-radio`. You would host this formula in your own
"tap" (a Git repository) for distribution.

3. FreeBSD (Ports System)

FreeBSD uses a source-based Ports system, which is conceptually similar to Homebrew but uses
Makefiles for metadata. You would create a directory  radio/ka9q-radio  in the Ports tree.

The  Makefile  would look like this:

```
  PORTNAME=    ka9q-radio
  PORTVERSION=    1.0
  CATEGORIES=    radio

  MAINTAINER=    you@example.com
  COMMENT=    A massively concurrent SDR receiver daemon

  LICENSE=    GPLv2 # Or whatever the license is

  # Dependencies
  LIB_DEPENDS=    libfftw3f.so:${PORTSDIR}/math/fftw3-float \
          libusb.so:${PORTSDIR}/devel/libusb \
          libiniparser.so:${PORTSDIR}/devel/iniparser \
          libdns_sd.so:${PORTSDIR}/net/mDNSResponder_nossl

  # Build configuration
  USES=        cmake pkgconfig
  USE_GITHUB=    yes
  GH_ACCOUNT=    ka9q
  GH_PROJECT=    ka9q-radio

  # Tell the build where to find mDNSResponder
  LDFLAGS+=    -L${LOCALBASE}/lib
  CPPFLAGS+=    -I${LOCALBASE}/include

  .include <bsd.port.mk>
```

This  Makefile  tells the FreeBSD Ports system everything it needs to know to download, build
(using CMake), and package `ka9q-radio`.

### Summary of Benefits

• Single Source of Truth: The `CMakeLists.txt` becomes the canonical definition of how to build
the project.
• Robustness: CMake handles finding libraries and setting compiler/linker flags in a portable way
• Maintainability: Adding a new source file or a new driver is a one-line change in one place.
• Native Experience: Users on each platform get a familiar, easy-to-use installation method (
`apt-get`, `brew install`, `pkg install`).

Unit/Integration Tests

Excellent question. Yes, despite the hardware dependency, there is significant low-hanging fruit
for testing by focusing on parts of the code that can be decoupled from the hardware I/O.

### Unit Testing

The most straightforward opportunities are in pure data-in, data-out functions:

1. DSP Core (`filter.c`): This is the top candidate. The FFT and filtering logic operates on
sample buffers. You can create unit tests that:
  • Generate a known input signal in memory (e.g., a pure sine wave).
  • Feed this buffer to the DSP functions.
  • Assert that the output buffer contains the expected result (e.g., the signal is correctly
  filtered or frequency-shifted). This validates the core mathematics of the application without
  any hardware.
2. Control Protocol (`control.c`): The functions that parse UDP control messages can be tested in
isolation.
  • Craft a series of known-good and malformed command strings.
  • Call the parsing function directly with these strings.
  • Check that the internal radio state is updated correctly for valid commands and that
  appropriate errors are handled for invalid ones.


### Integration Testing

For broader testing, the key is to mock the hardware interface. The application already uses
dynamically loaded drivers (`.so` files), which is perfect for this.

1. Create a "File-Based" SDR Driver:
  • Create a new, fake SDR driver (`fake_sdr.c`) that implements the same interface as the real
  hardware drivers.
  • Instead of communicating with USB hardware, this fake driver would read pre-recorded I/Q
  samples from a file.
  • You could have a "golden file" containing a known signal (e.g., a captured FM radio
  broadcast).
2. End-to-End Data Flow Test:
  • Write a test harness that loads the `ka9q-radio` core logic and points it to the fake SDR
  driver.
  • The test runs the entire DSP pipeline using the known input from the golden file.
  • The test captures the final output data (that would normally be sent to the network client)
  and compares it to a known-good output file.
  • This creates a powerful regression test for the entire signal processing chain. If a change
  in `filter.c` breaks the output, this test will catch it immediately, all without needing any
  physical hardware connected.


Dependency Portability

Here is a breakdown of the availability for each of those dependencies on macOS and
FreeBSD.

The short answer is that all functionality is available, but one library (`avahi`) will require a
code change to use the correct portable API.

--------

### Library Availability Breakdown

 Library   │ macOS Availability      │ FreeBSD Availability    │ Notes
───────────┼─────────────────────────┼─────────────────────────┼─────────────────────────────────
 fftw3     │ Excellent (Homebrew)    │ Excellent (Pkg/Ports)   │ Standard scientific library. No
           │                         │                         │ issues.
 pthreads  │ Excellent (Core System) │ Excellent (Core System) │ Part of the base OS on both.
           │                         │                         │ POSIX standard.
 dlfcn.h   │ Excellent (Core System) │ Excellent (Core System) │ Part of the base OS on both.
           │                         │                         │ POSIX standard.
 iniparser │ Excellent (Homebrew)    │ Excellent (Pkg/Ports)   │ Widely available and portable.
 libusb    │ Excellent (Homebrew)    │ Excellent (Pkg/Ports)   │ The standard for cross-platform
           │                         │                         │ USB access.
 avahi     │ Refactor Required       │ Refactor Required       │ Avahi is Linux-specific. Must
           │                         │                         │ use the portable API.

--------

### Detailed Comments

#### 1. `fftw3`, `iniparser`, `libusb` (Package-Managed)

These are well-established, portable, third-party libraries. They are readily available through
the standard package managers on both systems.

• On macOS (via Homebrew):
  `brew install fftw iniparser libusb`

• On FreeBSD (via pkg):
  `pkg install fftw3 iniparser libusb`

You can expect these to compile and link without any issues.

#### 2.  pthreads  and  dlfcn.h  (Core System)

These are not external libraries that need to be installed. They are fundamental POSIX/UNIX
standards that are part of the core operating system.

• `pthreads`: The POSIX Threads library is the native threading implementation on both macOS
(which is UNIX-certified) and FreeBSD. You simply `include <pthread.h>` and link with `-lpthread`
• `dlfcn.h`: The API for dynamic loading of shared libraries (`dlopen`, `dlsym`, etc.) is also a
core part of the system. Just include the header.

#### 3.  avahi  (The One That Needs a Change)

This is the only significant challenge, and it requires a change in the source code, not just
installing a package.

• The Problem: `avahi` is a specific implementation of mDNS/DNS-SD (also known as "zero-
configuration networking") that is dominant on Linux and often tied to D-Bus. It is not available
on macOS or FreeBSD.
• The Solution: Both macOS and FreeBSD have their own native, high-quality mDNS implementations.
  • macOS: Has the original implementation, called Bonjour. It is a core part of the operating
  system.
  • FreeBSD: Uses the `mDNSResponder` daemon, which is the same codebase as Apple's Bonjour. It
  can be easily installed from packages (`pkg install avahi-mDNSResponder-compat` or similar).

Crucially, both Bonjour on macOS and mDNSResponder on FreeBSD expose the exact same standard C
API, which is defined in the header `<dns_sd.h>`.

To make `ka9q-radio` portable, you must refactor the service discovery code in `src/control.c` to
use the portable `<dns_sd.h>` API instead of the Linux-specific `<avahi-client/client.h>` API.
The logic will be very similar (registering a service, etc.), but the function calls will be
different. This is the correct and standard way to write portable zero-configuration networking
code.


Rates and Buffering

The system is designed specifically to handle this discrepancy between
high-speed hardware input and low-speed audio output through a process
of channelization and decimation, not direct rate adaptation.

Here is a breakdown of the data flow and how it manages the different rates:

### 1. The Input Side: The "Master Filter" and the SDR Rate

The SDR hardware's sample rate is a fundamental parameter for the
entire system. It is set once in the `[radio]` section of the
configuration file and dictates the total amount of spectrum the radio
can "see" at one time.

• The Master Filter Thread (`master_filter_thread` in `radio.c`): This
is the workhorse. A single master thread reads the high-speed,
wideband I/Q sample stream from the SDR hardware via a ring buffer.

• The Big FFT: Instead of processing the raw samples directly, this
thread's main job is to perform a very large Fast Fourier Transform
(FFT) on big chunks of the incoming data. This converts the signal
from the time domain (amplitude over time) to the frequency domain
(power over frequency).

• Efficiency: This is the key architectural decision. Performing one
huge FFT is computationally expensive, but it's done only once for all
channels. The output of this FFT is a complete snapshot of the entire
spectrum the SDR can see.

So, to handle a 4 MSPS SDR or a 100 MSPS SDR, you simply configure the
sample_rate in the `.ini` file. The Master Filter will adapt, and the
size of its FFT will be adjusted accordingly. The rest of the system
sees the output of this master thread, not the raw SDR data.

### 2. The Output Side: "Slave Filters" and Per-Channel Rates

This is where the rate reduction happens. For every single receiver
channel defined in the configuration, the system spawns a dedicated
"Slave Filter Thread".

• Working in the Frequency Domain: The slave threads don't get the
high-speed I/Q data. Instead, they get access to the frequency-domain
output from the Master Filter's big FFT.

• Digital Tuning and Filtering: Each slave thread "tunes" to its
desired frequency by simply selecting a small slice of the master
FFT's output array. If a channel wants to listen at 146.520 MHz, it
grabs the data centered around that frequency bin from the master
array.

• Decimation via IFFT: The crucial step is this: the slave thread
takes its small slice of spectrum and performs a much smaller Inverse
FFT (IFFT) on it. Because it's only operating on a tiny piece of the
original bandwidth, the resulting time-domain I/Q signal has a
dramatically lower sample rate. This is decimation. The output sample
rate (`out_rate` in the channel config) directly corresponds to the
width of the slice it takes. If a channel is configured for `out_rate
= 48000`, it carves out a roughly 48 kHz-wide piece of spectrum.

• Final Processing: The now low-rate I/Q signal is demodulated (e.g.,
FM demodulation), filtered for audio, and made ready for the client.

### 3. Buffering and Rate Adaptation

Buffering is the glue that holds these concurrent, multi-rate threads
together.

• SDR to Master Thread: A large ring buffer is used here. The USB
hardware thread continuously dumps samples into this buffer. The
Master Filter thread consumes them in large chunks. This decouples the
real-time hardware I/O from the DSP processing, preventing data loss
if the CPU is momentarily busy.

• Master to Slave Threads: A synchronization mechanism (like a
condition variable) signals to all slave threads when a new FFT output
is ready from the master. The slaves can then all access this shared
frequency-domain data concurrently and do their work.

• Slave Thread to Network: The final, low-rate audio samples for each
channel are placed in their own output buffers before being packetized
into UDP and sent to the client.

### Summary: The Client's Role

The client does not get to request a sample rate. The server dictates
the output rate based on the channel's configuration (`out_rate`). A
client connecting to a 48 KSPS audio stream must be able to play audio
at that rate. The ka9q-radio server is the master of the pipeline; it
performs the decimation from the SDR's MSPS down to the channel's
configured KSPS, and the client simply receives that final stream.
