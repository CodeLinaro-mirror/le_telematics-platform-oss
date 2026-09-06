ACLOCAL_AMFLAGS = -I m4

if USE_GLIB
GLIB_CFLAGS = -DUSE_GLIB @GLIB_CFLAGS@
GLIB_LDFLAGS = @GLIB_LIBS@
endif

EXTRA_DIST = \
	convert_to_cfg.sh \
	log-default.txt

BUILT_SOURCES = commonlog.conf

CLEANFILES = commonlog.conf

.PHONY: commonlog.conf

lib_LTLIBRARIES = libcfg.la

libcfg_la_SOURCES = \
    src/cfg.c

libcfg_la_CFLAGS = \
    -I$(top_srcdir)/src \
    -Wall \
    -Wextra \
    -Wpedantic \
    -Wstrict-prototypes \
    -Wwrite-strings \
    -Wconversion \
    -g -O2 \
     $(GLIB_CFLAGS)

libcfg_la_LIBADD = $(GLIB_LDFLAGS)

libcfg_la_LDFLAGS = -shared -version-info 1:0:0

include_HEADERS = \
    src/cfg.h \
    src/cfg_types.h

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libcfg.pc

all-local: commonlog.conf

commonlog.conf: $(srcdir)/convert_to_cfg.sh $(srcdir)/log-default.txt
	$(SHELL) $(srcdir)/convert_to_cfg.sh "$(srcdir)/log-default.txt" "$@"

install-data-local: commonlog.conf
	$(MKDIR_P) $(DESTDIR)$(sysconfdir)
	$(INSTALL_DATA) commonlog.conf $(DESTDIR)$(sysconfdir)/commonlog.conf

uninstall-local:
	rm -f $(DESTDIR)$(sysconfdir)/commonlog.conf

clean-local:
	rm -f *~
	rm -f *.o
	rm -f *.a
	rm -f *.so*

distclean-local:
	rm -f config.h
	rm -f config.log
	rm -f config.status
