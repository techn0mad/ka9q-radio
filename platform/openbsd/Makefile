# -*- makefile -*-
# ka9q-radio OpenBSD rc.d service integration

sbindir    ?= /usr/local/sbin
bindir     ?= /usr/local/bin
sysconfdir ?= /etc
rcdir      ?= /etc/rc.d

SERVICES = radiod

.PHONY: all clean install uninstall purge

all: $(SERVICES)

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
