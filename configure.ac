dnl
dnl Configuration script for libcups.
dnl
dnl Copyright © 2020-2021 by OpenPrinting
dnl Copyright © 2007-2020 by Apple Inc.
dnl Copyright © 1997-2007 by Easy Software Products, all rights reserved.
dnl
dnl Licensed under Apache License v2.0.  See the file "LICENSE" for more
dnl information.
dnl

dnl We need at least autoconf 2.71...
AC_PREREQ([2.71])

dnl Package name and version...
AC_INIT([libcups],[3.0b1],[https://github.com/michaelrsweet/libcups/issues],[cups],[https://openprinting.github.io/cups])
AC_CONFIG_HEADERS([config.h])

LIBCUPS_VERSION="AC_PACKAGE_VERSION"
LIBCUPS_VERSION_MAJOR="`echo AC_PACKAGE_VERSION | awk -F. '{print $1}'`"
LIBCUPS_VERSION_MINOR="`echo AC_PACKAGE_VERSION | awk -F. '{printf("%d\n",$2);}'`"

AC_SUBST([LIBCUPS_VERSION])
AC_SUBST([LIBCUPS_VERSION_MAJOR])
AC_SUBST([LIBCUPS_VERSION_MINOR])
AC_DEFINE_UNQUOTED([LIBCUPS_VERSION], "$LIBCUPS_VERSION", [Version number])
AC_DEFINE_UNQUOTED([LIBCUPS_VERSION_MAJOR], $LIBCUPS_VERSION_MAJOR, [Major version number])
AC_DEFINE_UNQUOTED([LIBCUPS_VERSION_MINOR], $LIBCUPS_VERSION_MINOR, [Minor version number])

dnl This line is provided to ensure that you don't run the autoheader program
dnl against this project.  Doing so is completely unsupported and WILL cause
dnl problems!
AH_TOP([#error "Somebody ran autoheader on this project which is unsupported and WILL cause problems."])


dnl Get the build and host platforms and split the host_os value
AC_CANONICAL_BUILD
AC_CANONICAL_HOST

[host_os_name="$(echo $host_os | sed -e '1,$s/[0-9.]*$//g')"]
[host_os_version="$(echo $host_os | sed -e '1,$s/^[^0-9.]*//g' | awk -F. '{print $1 $2}')"]
# Linux often does not yield an OS version we can use...
AS_IF([test "x$host_os_version" = x], [
    host_os_version="0"
])


dnl Determine whether we are cross-compiling...
AS_IF([test "$build" = "$host"], [
    # No, build local targets
    LOCALTARGET="local"
], [
    # Yes, don't build local targets
    LOCALTARGET=""
])
AC_SUBST([LOCALTARGET])

AC_PATH_PROGS([CODE_SIGN], [codesign true])


dnl Unit tests take up time during a compile...
AC_ARG_ENABLE([unit_tests], AS_HELP_STRING([--enable-unit-tests], [build and run unit tests]))
AS_IF([test x$enable_unit_tests = xyes], [
    AS_IF([test "$build" != "$host"], [
	AC_MSG_ERROR([Sorry, cannot build unit tests when cross-compiling.])
    ])

    UNITTESTS="unittests"
], [
    UNITTESTS=""
])
AC_SUBST([UNITTESTS])


dnl Compiler options...
CFLAGS="${CFLAGS:=}"
CPPFLAGS="${CPPFLAGS:=}"
DSOFLAGS="${DSOFLAGS:=}"
LDFLAGS="${LDFLAGS:=}"
LIBS="${LIBS:=}"
OPTIM="${OPTIM:=}"

AC_SUBST([DSOFLAGS])
AC_SUBST([OPTIM])


dnl Standard programs...
AC_PROG_CC
AC_PROG_RANLIB
AC_PATH_PROG([AR], [ar])
AC_PATH_PROGS([CODE_SIGN], [codesign true])
AC_MSG_CHECKING([for install-sh script])
INSTALL="`pwd`/install-sh"
AC_SUBST([INSTALL])
AC_MSG_RESULT([using $INSTALL])
AC_PATH_PROG([MKDIR], [mkdir])
AC_PATH_PROG([RM], [rm])
AC_PATH_PROG([LN], [ln])


dnl Figure out the correct "ar" command flags...
AS_IF([test "$ac_cv_prog_ranlib" = ":"], [
    ARFLAGS="crs"
], [
    ARFLAGS="cr"
])
AC_SUBST([ARFLAGS])


dnl install-sh
AC_MSG_CHECKING([for install-sh script])
INSTALL="$(pwd)/install-sh"
AC_SUBST([INSTALL])
AC_MSG_RESULT([using $INSTALL])


dnl Check for pkg-config, which is used for some other tests later on...
AC_PATH_TOOL([PKGCONFIG], [pkg-config])

PKGCONFIG_CFLAGS=""
PKGCONFIG_LIBS="-L\${libdir} -lcups"
PKGCONFIG_LIBS_PRIVATE="-lz -lm"
PKGCONFIG_REQUIRES=""
PKGCONFIG_REQUIRES_PRIVATE="zlib"
AC_SUBST([PKGCONFIG_CFLAGS])
AC_SUBST([PKGCONFIG_LIBS])
AC_SUBST([PKGCONFIG_LIBS_PRIVATE])
AC_SUBST([PKGCONFIG_REQUIRES])
AC_SUBST([PKGCONFIG_REQUIRES_PRIVATE])


dnl String functions...
AC_CHECK_FUNCS([strlcpy])


dnl Check for "long long" support...
AC_CACHE_CHECK([for long long int], [ac_cv_c_long_long], [
    AS_IF([test "$GCC" = yes], [
	ac_cv_c_long_long="yes"
    ], [
	AC_COMPILE_IFELSE([
	    AC_LANG_PROGRAM([[ ]], [[long long int i;]])
	], [
	    ac_cv_c_long_long="yes"
	], [
	    ac_cv_c_long_long="no"
	])
    ])
])

AS_IF([test $ac_cv_c_long_long = yes], [
    AC_DEFINE([HAVE_LONG_LONG], [1], [Does the compiler support the long long type?])
])

AC_CHECK_FUNC([strtoll], [
    AC_DEFINE([HAVE_STRTOLL], [1], [Do we have the strtoll function?])
])


dnl POSIX threads (required)
AC_CHECK_HEADER([pthread.h])

AS_IF([test x$ac_cv_header_pthread_h != xyes], [
    AC_MSG_ERROR([Sorry, this software requires POSIX threading support.])
])

for flag in -lpthreads -lpthread -pthread; do
    AC_MSG_CHECKING([for pthread_create using $flag])
    SAVELIBS="$LIBS"
    LIBS="$flag $LIBS"
    AC_LANG([C])
    AC_LINK_IFELSE([AC_LANG_PROGRAM([#include <pthread.h>],[pthread_create(0, 0, 0, 0);])], [
        have_pthread=yes
        PKGCONFIG_LIBS_PRIVATE="$PKGCONFIG_LIBS_PRIVATE $flag"
    ], [
        LIBS="$SAVELIBS"
    ])

    AS_IF([test x$have_pthread = xyes], [
	AC_MSG_RESULT([yes])
	CPPFLAGS="$CPPFLAGS -D_THREAD_SAFE -D_REENTRANT"
	break
    ], [
	AC_MSG_RESULT([no])
    ])
done


dnl ZLIB (required)
AC_CHECK_HEADER([zlib.h])
AC_CHECK_LIB([z], [inflateCopy])

AS_IF([test x$ac_cv_header_zlib_h != xyes -o x$ac_cv_lib_z_inflateCopy != xyes], [
    AC_MSG_ERROR([Sorry, this software requires ZLIB 1.1 or higher.])
])


dnl DNS-SD support...
AC_ARG_WITH([dnssd], AS_HELP_STRING([--with-dnssd=LIBRARY], [set DNS-SD library (auto, avahi, mdnsresponder)]))

AS_IF([test "x$with_dnssd" != xmdnsresponder -a "x$with_dnssd" != xno -a "x$PKGCONFIG" != x -a x$host_os_name != xdarwin], [
    AC_MSG_CHECKING([for Avahi])
    AS_IF([$PKGCONFIG --exists avahi-client], [
	AC_MSG_RESULT([yes])
	CPPFLAGS="$CPPFLAGS $($PKGCONFIG --cflags avahi-client)"
	LIBS="$LIBS $($PKGCONFIG --libs avahi-client)"
	PKGCONFIG_LIBS_PRIVATE="$($PKGCONFIG --libs avahi-client) $PKGCONFIG_LIBS_PRIVATE"
	PKGCONFIG_REQUIRES_PRIVATE="avahi-client, $PKGCONFIG_REQUIRES_PRIVATE"
	AC_DEFINE([HAVE_DNSSD], 1, [Have DNS-SD support?])
	AC_DEFINE([HAVE_AVAHI], 1, [Have Avahi?])
    ], [
	AC_MSG_RESULT([no])
	AS_IF([test x$with_dnssd = xavahi],
	    AC_MSG_ERROR([libavahi-client-dev needed for --with-dnssd=avahi.]))
    ])
], [test x$with_dnssd = xavahi], [
    AC_MSG_ERROR([pkgconfig and libavahi-client-dev needed for --with-dnssd=avahi.])
], [test x$with_dnssd != xavahi -a "x$with_dnssd" != xno], [
    AC_CHECK_HEADER(dns_sd.h, [
        AS_CASE(["$host_os_name"],
	[darwin*], [
	    # Darwin and macOS...
	    AC_DEFINE([HAVE_DNSSD], 1, [Have DNS-SD support?])
	    AC_DEFINE([HAVE_MDNSRESPONDER], 1, [Have mDNSResponder?)])
	    LIBS="$LIBS -framework CoreFoundation -framework SystemConfiguration"
	    PKGCONFIG_LIBS_PRIVATE="$PKGCONFIG_LIBS_PRIVATE -framework CoreFoundation -framework SystemConfiguration"
	], [*], [
	    # All others...
	    AC_MSG_CHECKING(for current version of dns_sd library)
	    SAVELIBS="$LIBS"
	    LIBS="$LIBS -ldns_sd"
	    AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <dns_sd.h>]], [[
		int constant = kDNSServiceFlagsShareConnection;
		unsigned char txtRecord[100];
		uint8_t valueLen;
		TXTRecordGetValuePtr(sizeof(txtRecord), txtRecord, "value", &valueLen);]])
	    ],[
		AC_MSG_RESULT([yes])
		AC_DEFINE([HAVE_DNSSD], 1, [Have DNS-SD support?])
		AC_DEFINE([HAVE_MDNSRESPONDER], 1, [Have mDNSResponder?)])
		PKGCONFIG_LIBS_PRIVATE="$PKGCONFIG_LIBS_PRIVATE -ldns_sd"
	    ],[
		AC_MSG_RESULT([no])
		LIBS="$SAVELIBS"
		AS_IF([test x$with_dnssd = xmdnsresponder],
		    AC_MSG_ERROR([mDNSResponder required for --with-dnssd=mdnsresponder.]))
	    ])
	])
    ])
])
AC_SUBST([PKGCONFIG_AVAHI])


dnl TLS support...
AS_IF([test $host_os_name = darwin], [
    AC_CHECK_HEADER([Security/SecureTransport.h], [
	AC_DEFINE([HAVE_TLS], [1], [Do we support TLS?])
	AC_DEFINE([HAVE_CDSASSL], [1], [Do we have the macOS SecureTransport API?])
	LIBS="$LIBS -framework Security"

	dnl Check for the various security headers...
	AC_CHECK_HEADER([Security/SecCertificate.h], [
	    AC_DEFINE([HAVE_SECCERTIFICATE_H], [1], [Have the <Security/SecCertificate.h> header?])
	])
	AC_CHECK_HEADER([Security/SecItem.h], [
	    AC_DEFINE([HAVE_SECITEM_H], [1], [Have the <Security/SecItem.h> header?])
	])
	AC_CHECK_HEADER([Security/SecPolicy.h], [
	    AC_DEFINE([HAVE_SECPOLICY_H], [1], [Have the <Security/SecPolicy.h header?])
	])
    ])
], [
    AS_IF([test "x$PKGCONFIG" != x], [
	AS_IF([$PKGCONFIG --exists gnutls], [
	    AC_DEFINE([HAVE_TLS], [1], [Do we support TLS?])
	    AC_DEFINE([HAVE_GNUTLS], [1], [Do we have the GNU TLS library?])
	    CPPFLAGS="$CPPFLAGS `$PKGCONFIG --cflags gnutls`"
	    LIBS="$LIBS `$PKGCONFIG --libs gnutls`"
	    PKGCONFIG_LIBS_PRIVATE="$($PKGCONFIG --libs gnutls) $PKGCONFIG_LIBS_PRIVATE"
	    PKGCONFIG_REQUIRES_PRIVATE="gnutls >= 3.0, $PKGCONFIG_REQUIRES_PRIVATE"
	    AC_DEFINE(HAVE_GNUTLS, 1, [Have GNU TLS?])
	], [
	    AC_MSG_ERROR([Sorry, this software requires GNU TLS or the corresponding platform TLS support.])
	])
    ])
])

dnl Random number support...
AC_CHECK_HEADER([sys/random.h], [
    AC_DEFINE([HAVE_SYS_RANDOM_H], 1, [Have <sys/random.h> header?])
])
AC_CHECK_FUNCS([arc4random getrandom gnutls_rnd])


dnl Library targets...
AC_ARG_ENABLE([static], AS_HELP_STRING([--disable-static], [do not install static library]))
AC_ARG_ENABLE([shared], AS_HELP_STRING([--disable-shared], [do not install shared library]))

AS_IF([test x$enable_shared != xno], [
    AS_IF([test "$host_os_name" = darwin], [
	LIBCUPS="libcups.3.dylib"
    ], [
	LIBCUPS="libcups.so.3"
    ])

    AS_IF([test x$enable_static != xno], [
	AC_MSG_NOTICE([Installing static libraries...])
	INSTALLSTATIC="installstatic"
        LIBCUPS_STATIC="libcups.a"
    ], [
	INSTALLSTATIC=""
	LIBCUPS_STATIC=""
    ])
], [
    INSTALLSTATIC=""
    LIBCUPS="libcups.a"
    LIBCUPS_STATIC=""
    PKGCONFIG_LIBS="$PKGCONFIG_LIBS $PKGCONFIG_LIBS_PRIVATE"
    PKGCONFIG_LIBS_PRIVATE=""
    PKGCONFIG_REQUIRES="$PKGCONFIG_REQUIRES $PKGCONFIG_REQUIRES_PRIVATE"
    PKGCONFIG_REQUIRES_PRIVATE=""
])

AC_SUBST([INSTALLSTATIC])
AC_SUBST([LIBCUPS])
AC_SUBST([LIBCUPS_STATIC])


dnl Extra compiler options...
AC_ARG_ENABLE([debug], AS_HELP_STRING([--enable-debug], [turn on debugging, default=no]))
AC_ARG_ENABLE([maintainer], AS_HELP_STRING([--enable-maintainer], [turn on maintainer mode, default=no]))
AC_ARG_ENABLE([sanitizer], AS_HELP_STRING([--enable-sanitizer], [build with AddressSanitizer, default=no]))

AS_IF([test x$enable_debug = xyes], [
    OPTIM="$OPTIM -g"
    CSFLAGS=""
], [
    OPTIM="$OPTIM -g -Os"
    CSFLAGS="-o runtime"
])

AC_SUBST([CSFLAGS])

WARNINGS=""
AC_SUBST([WARNINGS])

AS_IF([test -n "$GCC"], [
    AS_IF([test x$enable_sanitizer = xyes], [
	# Use -fsanitize=address with debugging...
	OPTIM="$OPTIM -fsanitize=address"
    ], [
	# Otherwise use the Fortify enhancements to catch any unbounded
	# string operations...
	CPPFLAGS="$CPPFLAGS -D_FORTIFY_SOURCE=2"
    ])

    dnl Show all standard warnings + unused variables when compiling...
    WARNINGS="-Wall -Wunused"

    dnl Drop some not-useful/unreliable warnings...
    for warning in char-subscripts deprecated-declarations format-truncation format-y2k switch unused-result; do
	AC_MSG_CHECKING([whether compiler supports -Wno-$warning])

	OLDCFLAGS="$CFLAGS"
	CFLAGS="$CFLAGS -Wno-$warning -Werror"

	AC_COMPILE_IFELSE([AC_LANG_PROGRAM()], [
	    AC_MSG_RESULT(yes)
	    WARNINGS="$WARNINGS -Wno-$warning"
        ], [
	    AC_MSG_RESULT(no)
	])

	CFLAGS="$OLDCFLAGS"
    done

    dnl Maintainer mode enables -Werror...
    AS_IF([test x$enable_maintainer = xyes], [
	WARNINGS="$WARNINGS -Werror"
    ])

    dnl See if PIE options are supported...
    AC_MSG_CHECKING(whether compiler supports -fPIE)
    OLDCFLAGS="$CFLAGS"
    AS_CASE(["$host_os_name"],
    [darwin*], [
	CFLAGS="$CFLAGS -fPIC -fPIE -Wl,-pie"
	AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]], [[]])],[
	    OLDCFLAGS="-fPIC $OLDCFLAGS"
	    LDFLAGS="-fPIE -Wl,-pie $LDFLAGS"
	    AC_MSG_RESULT(yes)
	],[
	    AC_MSG_RESULT(no)
	])
    ], [*], [
	CFLAGS="$CFLAGS -fPIC -fPIE -pie"
	AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]], [[]])],[
	    OLDCFLAGS="-fPIC $OLDCFLAGS"
	    LDFLAGS="-fPIE -pie $LDFLAGS"
	    AC_MSG_RESULT(yes)
	],[
	    AC_MSG_RESULT(no)
	])
    ])
    CFLAGS="$OLDCFLAGS"

    dnl OS-specific compiler options...
    AS_CASE(["$host_os_name"], [linux*], [
	CPPFLAGS="$CPPFLAGS -D__USE_MISC -D_TIME_BITS=64"
    ], [darwin*], [
        AS_IF([test "$host_os_version" -ge 200 -a x$enable_debug != xyes], [
            # macOS 11.0 and higher support the Apple Silicon (arm64) CPUs
	    OPTIM="$OPTIM -mmacosx-version-min=10.14 -arch x86_64 -arch arm64"
	], [test x$enable_debug != xyes], [
	    OPTIM="$OPTIM -mmacosx-version-min=10.14 -arch x86_64"
	])
    ])
])


dnl Extra linker options...
AC_ARG_WITH([dsoflags], AS_HELP_STRING([--with-dsoflags=...], [Specify additional DSOFLAGS]), [
    DSOFLAGS="$withval $DSOFLAGS"
])
AC_ARG_WITH([ldflags], AS_HELP_STRING([--with-ldflags=...], [Specify additional LDFLAGS]), [
    LDFLAGS="$withval $LDFLAGS"
])

dnl State and run directories for root servers...
AS_IF([test "$prefix" = NONE], [
    # Default prefix isn't bound until AC_OUTPUT...
    realprefix="/usr/local"
], [
    realprefix="$prefix"
])


dnl Provide autoconf with a list of files to generate and output them...
AC_CONFIG_FILES([Makedefs cups.pc packaging/libcups.list])
AC_OUTPUT
