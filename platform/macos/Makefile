# -*- makefile -*-
# ka9q-radio macOS launchd service integration

sbindir    ?= /usr/local/sbin
bindir     ?= /usr/local/bin
sysconfdir ?= /etc
datadir    ?= /usr/local/share
pkgdatadir ?= $(datadir)/ka9q-radio
# launchd templates are installed here, not into /Library/LaunchDaemons, because
# each receiver needs its own instantiated copy (see docs/PORTABLE-INIT.md).
launchddir ?= $(pkgdatadir)/launchd

PLISTS = net.ka9q.radiod.plist

.PHONY: all clean install uninstall purge

all: $(PLISTS)

%.plist: launchd/%.plist.in
	sed -e 's|@sbindir@|$(sbindir)|g' \
	    -e 's|@bindir@|$(bindir)|g' \
	    -e 's|@sysconfdir@|$(sysconfdir)|g' \
	    $< > $@

install: $(PLISTS)
	install -d -m 0755 $(DESTDIR)$(launchddir)
	install -m 0644 $(PLISTS) $(DESTDIR)$(launchddir)/
	@echo "Installed launchd template(s) to $(DESTDIR)$(launchddir)."
	@echo "Instantiate one per receiver -- see docs/PORTABLE-INIT.md."

uninstall:
	for p in $(PLISTS); do rm -f $(DESTDIR)$(launchddir)/$$p; done

clean:
	rm -f $(PLISTS)

purge:
	@test "$(CONFIRMED)" = "1" || { echo "Use top-level make purge."; exit 1; }
	for p in $(PLISTS); do rm -f $(DESTDIR)$(launchddir)/$$p; done
