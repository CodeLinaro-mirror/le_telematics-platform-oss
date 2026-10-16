AC_PREREQ(2.61)
AC_INIT([libclog], [1.0.0])
AM_INIT_AUTOMAKE([foreign subdir-objects])
AM_MAINTAINER_MODE
AC_CONFIG_SRCDIR([src/clog.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])

# ============================================================================
# Compiler and Tools
# ============================================================================

AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_RANLIB
AM_PROG_AR
LT_INIT

PKG_PROG_PKG_CONFIG

AC_ARG_WITH([glib],
      AC_HELP_STRING([--with-glib],
         [enable glib, building HLOS systems which use glib]))

if (test "x${with_glib}" = "xyes"); then
        PKG_CHECK_MODULES(GTHREAD, gthread-2.0 >= 2.16, dummy=yes,
                                AC_MSG_ERROR(GThread >= 2.16 is required))
        PKG_CHECK_MODULES(GLIB, glib-2.0 >= 2.16, dummy=yes,
                                AC_MSG_ERROR(GLib >= 2.16 is required))
        GLIB_CFLAGS="$GLIB_CFLAGS $GTHREAD_CFLAGS"
        GLIB_LIBS="$GLIB_LIBS $GTHREAD_LIBS"

        AC_SUBST(GLIB_CFLAGS)
        AC_SUBST(GLIB_LIBS)
fi

AM_CONDITIONAL(USE_GLIB, test "x${with_glib}" = "xyes")

PKG_CHECK_MODULES([CFG], [libcfg],
    [AC_MSG_RESULT([cfg-lib found: $CFG_CFLAGS $CFG_LIBS])],
    [AC_MSG_ERROR([libcfg is required])])

AC_SUBST([CFG_CFLAGS])
AC_SUBST([CFG_LIBS])

CLOG_PC_REQUIRES_PRIVATE="libcfg"
CLOG_PC_CFLAGS_EXTRA=""

# ============================================================================
# Check for pthread (always required)
# ============================================================================

AC_CHECK_LIB([pthread], [pthread_mutex_init], [], [
    AC_MSG_ERROR([pthread library is required])
])
AC_SUBST([PTHREAD_LIBS], [-lpthread])

# ============================================================================
# Check for syslog.h (always required - used by clog_log_internal() for
# the library's own diagnostic logging, independent of the SYSLOG backend)
# ============================================================================

AC_CHECK_HEADER([syslog.h], [], [
    AC_MSG_ERROR([syslog.h is required])
])

# ============================================================================
# DLT Backend Support
# ============================================================================

AC_ARG_WITH([dlt],
    [AS_HELP_STRING([--with-dlt], [Enable DLT (Diagnostic Log and Trace) backend support])],
    [],
    [with_dlt=no])

AM_CONDITIONAL([CLOG_WITH_DLT], [test "x$with_dlt" = "xyes"])

AM_COND_IF([CLOG_WITH_DLT], [
    PKG_CHECK_MODULES([DLT], [automotive-dlt],
        [AC_MSG_RESULT([DLT found: $DLT_CFLAGS $DLT_LIBS])],
        [AC_MSG_ERROR([automotive-dlt (dlt-daemon) is required for --with-dlt])])
    AC_DEFINE([CLOG_WITH_DLT], [1], [Define to enable DLT backend])
])

AC_SUBST([DLT_CFLAGS])
AC_SUBST([DLT_LIBS])

# ============================================================================
# DIAG Backend Support
# ============================================================================

AC_ARG_WITH([diag],
    [AS_HELP_STRING([--with-diag], [Enable DIAG (Qualcomm Diagnostic) backend support])],
    [],
    [with_diag=no])

AM_CONDITIONAL([CLOG_WITH_DIAG], [test "x$with_diag" = "xyes"])

AM_COND_IF([CLOG_WITH_DIAG], [
    PKG_CHECK_MODULES([DIAG], [diag],
        [AC_MSG_RESULT([DIAG found: $DIAG_CFLAGS $DIAG_LIBS])],
        [AC_MSG_ERROR([diag is required for --with-diag])])
    AC_DEFINE([CLOG_WITH_DIAG], [1], [Define to enable DIAG backend])
    CLOG_PC_REQUIRES_PRIVATE="$CLOG_PC_REQUIRES_PRIVATE diag"
    CLOG_PC_CFLAGS_EXTRA="-DCLOG_WITH_DIAG"
])

AC_SUBST([DIAG_CFLAGS])
AC_SUBST([DIAG_LIBS])
AC_SUBST([CLOG_PC_REQUIRES_PRIVATE])
AC_SUBST([CLOG_PC_CFLAGS_EXTRA])

# ============================================================================
# Syslog Backend Support
# ============================================================================

AC_ARG_WITH([syslog],
    [AS_HELP_STRING([--with-syslog], [Enable syslog backend support])],
    [],
    [with_syslog=no])

AM_CONDITIONAL([CLOG_WITH_SYSLOG], [test "x$with_syslog" = "xyes"])

AM_COND_IF([CLOG_WITH_SYSLOG], [
    AC_DEFINE([CLOG_WITH_SYSLOG], [1], [Define to enable syslog backend])
])

# ============================================================================
# Logcat Backend Support (Android)
# ============================================================================

AC_ARG_WITH([logcat],
    [AS_HELP_STRING([--with-logcat], [Enable Android logcat backend support])],
    [],
    [with_logcat=no])

AM_CONDITIONAL([CLOG_WITH_LOGCAT], [test "x$with_logcat" = "xyes"])

AM_COND_IF([CLOG_WITH_LOGCAT], [
    PKG_CHECK_MODULES([LIBLOG], [liblog],
        [AC_MSG_RESULT([liblog found: $LIBLOG_CFLAGS $LIBLOG_LIBS])],
        [AC_MSG_ERROR([liblog is required for --with-logcat])])
    AC_DEFINE([CLOG_WITH_LOGCAT], [1], [Define to enable logcat backend])
])

AC_SUBST([LIBLOG_CFLAGS])
AC_SUBST([LIBLOG_LIBS])

# ============================================================================
# File Backend Support
# ============================================================================

AC_ARG_WITH([file],
    [AS_HELP_STRING([--with-file], [Enable file backend support (default: yes)])],
    [],
    [with_file=yes])

AM_CONDITIONAL([CLOG_WITH_FILE], [test "x$with_file" = "xyes"])

AM_COND_IF([CLOG_WITH_FILE], [
    AC_DEFINE([CLOG_WITH_FILE], [1], [Define to enable file backend])
])

# ============================================================================
# Console Backend Support
# ============================================================================

AC_ARG_WITH([console],
    [AS_HELP_STRING([--with-console], [Enable console backend support (default: yes)])],
    [],
    [with_console=yes])

AM_CONDITIONAL([CLOG_WITH_CONSOLE], [test "x$with_console" = "xyes"])

AM_COND_IF([CLOG_WITH_CONSOLE], [
    AC_DEFINE([CLOG_WITH_CONSOLE], [1], [Define to enable console backend])
])

# ============================================================================
# Debug/Trace Support
# ============================================================================

AC_ARG_WITH([debug],
    [AS_HELP_STRING([--with-debug], [Enable verbose per-message internal trace logging])],
    [],
    [with_debug=no])

AM_CONDITIONAL([CLOG_WITH_DEBUG], [test "x$with_debug" = "xyes"])

AM_COND_IF([CLOG_WITH_DEBUG], [
    AC_DEFINE([CLOG_WITH_DEBUG], [1], [Define to enable verbose per-message internal trace logging])
])

# ============================================================================
# Configuration Files
# ============================================================================

AC_CONFIG_FILES([
    Makefile
    libclog.pc
])

# ============================================================================
# Installation directories
# ============================================================================

AC_SUBST([pkgconfigdir], [${libdir}/pkgconfig])
AC_SUBST([sysconfdir], [${sysconfdir}])

# ============================================================================
# Configuration Summary
# ============================================================================

AC_MSG_NOTICE([])
AC_MSG_NOTICE([========================================])
AC_MSG_NOTICE([CLOG Configuration Summary])
AC_MSG_NOTICE([========================================])
AC_MSG_NOTICE([Version:        $PACKAGE_VERSION])
AC_MSG_NOTICE([Prefix:         $prefix])
AC_MSG_NOTICE([])
AC_MSG_NOTICE([Backends:])
AS_IF([test "x$with_dlt" = "xyes"],
    [AC_MSG_NOTICE([  DLT Backend:     ENABLED])],
    [AC_MSG_NOTICE([  DLT Backend:     DISABLED])])
AS_IF([test "x$with_diag" = "xyes"],
    [AC_MSG_NOTICE([  DIAG Backend:    ENABLED])],
    [AC_MSG_NOTICE([  DIAG Backend:    DISABLED])])
AS_IF([test "x$with_syslog" = "xyes"],
    [AC_MSG_NOTICE([  Syslog Backend:  ENABLED])],
    [AC_MSG_NOTICE([  Syslog Backend:  DISABLED])])
AS_IF([test "x$with_logcat" = "xyes"],
    [AC_MSG_NOTICE([  Logcat Backend:  ENABLED])],
    [AC_MSG_NOTICE([  Logcat Backend:  DISABLED])])
AS_IF([test "x$with_file" = "xyes"],
    [AC_MSG_NOTICE([  File Backend:    ENABLED])],
    [AC_MSG_NOTICE([  File Backend:    DISABLED])])
AS_IF([test "x$with_console" = "xyes"],
    [AC_MSG_NOTICE([  Console Backend: ENABLED])],
    [AC_MSG_NOTICE([  Console Backend: DISABLED])])
AS_IF([test "x$with_debug" = "xyes"],
    [AC_MSG_NOTICE([  Debug Trace:     ENABLED])],
    [AC_MSG_NOTICE([  Debug Trace:     DISABLED])])
AC_MSG_NOTICE([])
AC_MSG_NOTICE([========================================])
AC_MSG_NOTICE([])

AC_OUTPUT
