ACLOCAL_AMFLAGS = -I m4

bin_PROGRAMS = clogctl

clogctl_SOURCES = \
    src/clogctl.c

if USE_GLIB
GLIB_CFLAGS = -DUSE_GLIB @GLIB_CFLAGS@
GLIB_LDFLAGS = @GLIB_LIBS@
endif

clogctl_CFLAGS = \
    -Wall \
    -Wextra \
    -Wpedantic \
    -Wstrict-prototypes \
    -Wwrite-strings \
    -Wconversion \
    -g -O2 \
    $(CFG_CFLAGS)\
    $(GLIB_CFLAGS)

clogctl_LDADD = $(CFG_LIBS) $(GLIB_LDFLAGS)

clean-local:
	rm -f *~
	rm -f *.o

distclean-local:
	rm -f config.h
	rm -f config.log
	rm -f config.status
