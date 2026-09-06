ACLOCAL_AMFLAGS = -I m4

EXTRA_DIST = \
    API.md

# ============================================================================
# Library Definition
# ============================================================================

lib_LTLIBRARIES = libclog.la

libclog_la_SOURCES = \
    src/clog.c \
    src/clog_util.c \
    src/sinkImpl/clog_sinkImpl.c

if USE_GLIB
GLIB_CFLAGS = -DUSE_GLIB @GLIB_CFLAGS@
GLIB_LDFLAGS = @GLIB_LIBS@
endif

libclog_la_CFLAGS = \
    -I$(top_srcdir)/src \
    -I$(top_srcdir)/src/sinkImpl \
    -Wall \
    -Wextra \
    -Wpedantic \
    -Wstrict-prototypes \
    -Wwrite-strings \
    -Wconversion \
    -g -O2 \
    $(PTHREAD_LIBS) \
    $(CFG_CFLAGS) \
    $(DLT_CFLAGS) \
    $(DIAG_CFLAGS) \
    $(LIBLOG_CFLAGS) \
    $(GLIB_CFLAGS)

libclog_la_LDFLAGS = -shared -version-info 1:0:0 $(GLIB_LDFLAGS)

libclog_la_LIBADD = $(PTHREAD_LIBS) $(CFG_LIBS) $(GLIB_LIBS)

# ============================================================================
# Public Headers
# ============================================================================

include_HEADERS = \
    src/clog.h \
    src/clog_ids.h \
    src/clog_qcom_ids.h \
    src/clog_customer_ids.h \
    src/clog_actx_ipcl.h \
    src/clog_util.h \
    src/sinkImpl/clog_sinkImpl.h


# ============================================================================
# Compiler Flags and Sources based on Backend Configuration
# ============================================================================

if CLOG_WITH_DLT
libclog_la_SOURCES += src/sinkImpl/clog_sink_dlt.c
libclog_la_CFLAGS += -DCLOG_WITH_DLT
libclog_la_LIBADD += $(DLT_LIBS)
endif

if CLOG_WITH_DIAG
libclog_la_SOURCES += src/sinkImpl/clog_sink_diag.c
libclog_la_CFLAGS += -DCLOG_WITH_DIAG
libclog_la_LIBADD += $(DIAG_LIBS)
endif

if CLOG_WITH_SYSLOG
libclog_la_SOURCES += src/sinkImpl/clog_sink_syslog.c
libclog_la_CFLAGS += -DCLOG_WITH_SYSLOG
endif

if CLOG_WITH_LOGCAT
libclog_la_SOURCES += src/sinkImpl/clog_sink_logcat.c
libclog_la_CFLAGS += -DCLOG_WITH_LOGCAT
libclog_la_LIBADD += $(LIBLOG_LIBS)
endif

if CLOG_WITH_FILE
libclog_la_SOURCES += src/sinkImpl/clog_sink_file.c
libclog_la_CFLAGS += -DCLOG_WITH_FILE
endif

if CLOG_WITH_CONSOLE
libclog_la_SOURCES += src/sinkImpl/clog_sink_console.c
libclog_la_CFLAGS += -DCLOG_WITH_CONSOLE
endif

if CLOG_WITH_DEBUG
libclog_la_CFLAGS += -DCLOG_WITH_DEBUG
endif

# ============================================================================
# pkg-config file installation
# ============================================================================

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libclog.pc

# ============================================================================
# Cleanup
# ============================================================================

clean-local:
	rm -f *~
	rm -f *.o
	rm -f *.a
	rm -f *.so*

distclean-local:
	rm -f config.h
	rm -f config.log
	rm -f config.status
