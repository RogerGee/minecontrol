AC_PREREQ(2.69)
AC_INIT([minecontrol],[1.7.0],[])
AM_INIT_AUTOMAKE([foreign -Wall -Werror])
AC_PROG_CXX

dnl Configure MINECONTROL_TEST if enabled.
AC_ARG_ENABLE([testing],
    AS_HELP_STRING([--enable-testing], [Enable testing functionality]))
AS_IF([test "x$enable_testing" = "xyes"], [
    AC_DEFINE([MINECONTROL_TEST],["1"],[Build binary for testing])
])

dnl rlibrary is a C++ library: just check a header since most if not all of
dnl  its exports are C++ routines.
AC_CHECK_HEADER(rlibrary/rstream.h)
AC_CHECK_LIB(pthread,pthread_create)

dnl Search for these libraries but do not add them to LDFLAGS. They are added by
dnl the Makefile later on to differentiate which binary gets what flags when getting
dnl compiled.
AC_CHECK_LIB(crypt,crypt,[],[AC_MSG_ERROR([crypt library not found])])
AC_CHECK_LIB(ssl,OPENSSL_init_ssl,[],[AC_MSG_ERROR([OpenSSL library not found])])
AC_CHECK_LIB(ncurses,initscr,[],[AC_MSG_ERROR([ncurses library not found])])
AC_CHECK_LIB(readline,rl_stuff_char,[],[AC_MSG_ERROR([readline library not found])])

dnl Force c++11 flags.
CXXFLAGS="$CXXFLAGS -std=c++11"

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile])

AC_OUTPUT
