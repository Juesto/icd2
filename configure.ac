AC_INIT([Internet Connectivity daemon 2], patsubst(esyscmd([dpkg-parsechangelog | sed -n '/^Version: \(.*\)$/ {s//\1/;p}']), [
]), [], [icd2])
AM_CONFIG_HEADER(config.h)

AC_CONFIG_MACRO_DIR([m4])
AM_MAINTAINER_MODE
AM_INIT_AUTOMAKE([foreign])

AC_PROG_CC
AC_PROG_INSTALL
AM_PROG_LIBTOOL

AC_HEADER_STDC

AC_PATH_X
AC_PATH_XTRA
AC_SUBST(X_CFLAGS)

AM_GCONF_SOURCE_2
AC_PATH_PROG(GCONFTOOL, gconftool-2)

PKG_PROG_PKG_CONFIG

#+++++++++++++++
# Misc programs 
#+++++++++++++++


AC_SUBST(CFLAGS)

PKG_CHECK_MODULES(ICD,
			[glib-2.0 dnl
			dbus-glib-1 dnl
			osso-ic dnl
			mce dnl
			gconf-2.0])

AC_SUBST(ICD_LIBS)
AC_SUBST(ICD_CFLAGS)

#+++++++++++++++++++
# Directories setup
#+++++++++++++++++++

AC_OUTPUT([
icd2.pc
icd/network_api.h
Makefile
support/Makefile
icd/Makefile
policy/Makefile
])

