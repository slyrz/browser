#-----------------------------------------------------------------------------
# Autoconf Requirements
#-----------------------------------------------------------------------------
AC_PREREQ(2.61)
AC_INIT([browser],[0.1])
AC_LANG([C])
AC_CONFIG_SRCDIR([Makefile.am])
AC_CONFIG_HEADERS([src/config.h])

AM_INIT_AUTOMAKE([foreign subdir-objects silent-rules])
AM_MAINTAINER_MODE([enable])

#-----------------------------------------------------------------------------
# Compiler and Compiler Characteristics
#-----------------------------------------------------------------------------
AM_PROG_CC_C_O
AC_PROG_CC_STDC
AC_USE_SYSTEM_EXTENSIONS
AC_GNU_SOURCE

AC_PROG_RANLIB
PKG_PROG_PKG_CONFIG

AC_C_INLINE
AC_C_CONST
AC_C_RESTRICT

#-----------------------------------------------------------------------------
# Functions, Headers and Libraries
#-----------------------------------------------------------------------------
AC_CHECK_HEADERS([stddef.h])
AC_CHECK_HEADERS([stdbool.h])

PKG_CHECK_MODULES([glib], [glib-2.0])
PKG_CHECK_MODULES([gtk], [gtk+-3.0])
PKG_CHECK_MODULES([webkit], [webkit2gtk-4.0])

#-----------------------------------------------------------------------------
# Programs
#-----------------------------------------------------------------------------
AC_CHECK_PROG([have_dmenu], [dmenu], [yes])
AS_IF([test "x$have_dmenu" != "xyes"], [
  AC_MSG_ERROR([could not find dmenu])
])

AC_DEFINE_UNQUOTED([INPUT_COMMAND], ["$(which dmenu)"], [Command to call for user input.])

#-----------------------------------------------------------------------------
# Settings
#-----------------------------------------------------------------------------
AC_DEFINE([KEY_NAVIGATE], ['g'], [Key binding for action.])
AC_DEFINE([KEY_STOP_LOADING], ['c'], [Key binding for action.])
AC_DEFINE([KEY_SEARCH_START], ['f'], [Key binding for action.])
AC_DEFINE([KEY_SEARCH_NEXT], ['n'], [Key binding for action.])
AC_DEFINE([KEY_SEARCH_PREVIOUS], ['b'], [Key binding for action.])
AC_DEFINE([KEY_HISTORY_MOVE_BACK], ['h'], [Key binding for action.])
AC_DEFINE([KEY_HISTORY_MOVE_FORWARD], ['j'], [Key binding for action.])
AC_DEFINE([KEY_HISTORY_RELOAD], ['r'], [Key binding for action.])
AC_DEFINE([KEY_ZOOM_IN], ['+'], [Key binding for action.])
AC_DEFINE([KEY_ZOOM_OUT], ['-'], [Key binding for action.])
AC_DEFINE([KEY_ZOOM_RESET], ['0'], [Key binding for action.])
AC_DEFINE([KEY_EXIT], ['x'], [Key binding for action.])

AC_DEFINE([PREFERRED_LANGUAGES], ["en-US"], [Preferred languages.])
AC_DEFINE([SUPPORTED_PROTOCOLS], ["https", "http"], [Supported protocols.])
AC_DEFINE([DEFAULT_PROTOCOL], ["https"], [Default protocol.])
AC_DEFINE([DEFAULT_CHARSET], ["utf-8"], [Default charset.])

AC_DEFINE([DEFAULT_FONT_SIZE], [16], [Default font size.])
AC_DEFINE([DEFAULT_MONOSPACE_FONT_SIZE], [16], [Default monospace font size.])
AC_DEFINE([MINIMUM_FONT_SIZE], [10], [Minimum font size.])

AC_DEFINE([DEFAULT_FONT_FAMILY], ["Roboto"], [Default font family.])
AC_DEFINE([SANS_SERIF_FONT_FAMILY], ["Roboto"], [Sans-serif font family.])
AC_DEFINE([MONOSPACE_FONT_FAMILY], ["Roboto Mono"], [Monospace font family.])
AC_DEFINE([SERIF_FONT_FAMILY], ["Source Serif Pro"], [Serif font family.])

AC_DEFINE([ZOOM_STEP], [0.25], [Increase/decrease zoom by this level.])

#-----------------------------------------------------------------------------
# End
#-----------------------------------------------------------------------------
AC_CONFIG_FILES([Makefile])

AC_OUTPUT
AC_MSG_RESULT([
        $PACKAGE $VERSION
        ===========

        prefix: ............... ${prefix}
        sysconfdir: ........... ${sysconfdir}
        libdir: ............... ${libdir}
        includedir: ........... ${includedir}

        compiler: ............. ${CC}
        cflags: ............... ${CFLAGS}
])
