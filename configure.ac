AC_PREREQ(2.69)
AC_INIT([minecontrol],[1.2.3],[])
AM_INIT_AUTOMAKE([foreign -Wall -Werror])
AC_PROG_CXX

dnl rlibrary is a C++ library: just check a header since most if not all of
dnl  its exports are C++ routines.
AC_CHECK_HEADER(rlibrary/rstream.h)
AC_CHECK_LIB(pthread,pthread_create)

dnl Search for these libraries but do not add them to LDFLAGS. They are added by
dnl the Makefile later on to differentiate which binary gets what flags when getting
dnl compiled.
AC_CHECK_LIB(crypt,crypt,[echo Found crypt])
AC_CHECK_LIB(crypto,RSA_generate_key_ex,[echo Found crypto])
AC_CHECK_LIB(ncurses,initscr,[echo Found ncurses])

dnl Force c++11 flags.
CXXFLAGS="$CXXFLAGS -std=c++11"

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile])

AC_OUTPUT
