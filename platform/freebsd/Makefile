# -*- makefile -*-
# ka9q-radio FreeBSD rc.d service integration

sbindir    ?= /usr/local/sbin
bindir     ?= /usr/local/bin
sysconfdir ?= /etc
# rc.d scripts for third-party software live under /usr/local/etc on FreeBSD,
# independent of sysconfdir; override rcdir to change.
rcdir      ?= /usr/local/etc/rc.d

SERVICES = radiod

.PHONY: all clean install uninstall purge

all: $(SERVICES)

# expand @placeholders@ from the .in template
%: rc.d/%.in
	sed -e 's|@sbindir@|$(sbindir)|g' \
	    -e 's|@bindir@|$(bindir)|g' \
	    -e 's|@sysconfdir@|$(sysconfdir)|g' \
	    $< > $@

install: $(SERVICES)
	install -d -m 0755 $(DESTDIR)$(rcdir)
	for s in $(SERVICES); do \
		install -m 0555 $$s $(DESTDIR)$(rcdir)/$$s; \
	done

uninstall:
	for s in $(SERVICES); do rm -f $(DESTDIR)$(rcdir)/$$s; done

clean:
	rm -f $(SERVICES)

purge:
	@test "$(CONFIRMED)" = "1" || { echo "Use top-level make purge."; exit 1; }
	for s in $(SERVICES); do rm -f $(DESTDIR)$(rcdir)/$$s; done
