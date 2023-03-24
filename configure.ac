dnl
dnl Configuration script for libcups.
dnl
dnl Copyright © 2020-2023 by OpenPrinting
dnl Copyright © 2007-2020 by Apple Inc.
dnl Copyright © 1997-2007 by Easy Software Products, all rights reserved.
dnl
dnl Licensed under Apache License v2.0.  See the file "LICENSE" for more
dnl information.
dnl

dnl We need at least autoconf 2.71...
AC_PREREQ([2.71])

dnl Package name and version...
AC_INIT([libcups],[3.0b2],[https://github.com/OpenPrinting/libcups/issues],[cups],[https://openprinting.github.io/cups])
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
AC_PATH_PROG([RMDIR], [rmdir])
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


dnl Library prefix...
AC_ARG_ENABLE([libcups3-prefix], AS_HELP_STRING([--disable-libcups3-prefix], [do not add libcups3 prefix to header/library files]))

AS_IF([test x$enable_libcups3_prefix != xno], [
    CUPS_PC="cups3.pc"
    INCLUDE_PREFIX="/libcups3"
    LIBCUPS_BASE="libcups3"
    LINKCUPS="-lcups3"
], [
    CUPS_PC="cups.pc"
    INCLUDE_PREFIX=""
    LIBCUPS_BASE="libcups"
    LINKCUPS="-lcups"
])
AC_SUBST([LINKCUPS])


dnl Check for pkg-config, which is used for some other tests later on...
AC_PATH_TOOL([PKGCONFIG], [pkg-config])

PKGCONFIG_CFLAGS=""
PKGCONFIG_LIBS="-L\${libdir} $LINKCUPS"
PKGCONFIG_LIBS_PRIVATE="-lz -lm"
PKGCONFIG_REQUIRES=""
PKGCONFIG_REQUIRES_PRIVATE="zlib"
AC_SUBST([PKGCONFIG_CFLAGS])
AC_SUBST([PKGCONFIG_LIBS])
AC_SUBST([PKGCONFIG_LIBS_PRIVATE])
AC_SUBST([PKGCONFIG_REQUIRES])
AC_SUBST([PKGCONFIG_REQUIRES_PRIVATE])


dnl String functions...
AC_CHECK_FUNCS([strlcat strlcpy])


dnl DNS resolver functions...
SAVELIBS="$LIBS"
LIBS=""
AC_SEARCH_LIBS([hstrerror], [nsl socket resolv], [
    AC_DEFINE([HAVE_HSTRERROR], [1], [Have the hstrerror function?])
    PKGCONFIG_LIBS_PRIVATE="$LIBS $PKGCONFIG_LIBS_PRIVATE"
    SAVELIBS="$SAVELIBS $LIBS"
])
AC_SEARCH_LIBS([__res_init], [resolv bind], [
    AC_DEFINE([HAVE_RES_INIT], [1], [Have res_init function?])
    PKGCONFIG_LIBS_PRIVATE="$LIBS $PKGCONFIG_LIBS_PRIVATE"
    SAVELIBS="$SAVELIBS $LIBS"
], [
    AC_SEARCH_LIBS([res_9_init], [resolv bind], [
        AC_DEFINE([HAVE_RES_INIT], [1], [Have res_init function?])
	PKGCONFIG_LIBS_PRIVATE="$LIBS $PKGCONFIG_LIBS_PRIVATE"
	SAVELIBS="$SAVELIBS $LIBS"
    ], [
	AC_SEARCH_LIBS([res_init], [resolv bind], [
	    AC_DEFINE([HAVE_RES_INIT], [1], [Have res_init function?])
	    PKGCONFIG_LIBS_PRIVATE="$LIBS $PKGCONFIG_LIBS_PRIVATE"
	    SAVELIBS="$SAVELIBS $LIBS"
	])
    ])
])
LIBS="$SAVELIBS"


dnl macOS system configuration functions...
SAVELIBS="$LIBS"
LIBS="-framework SystemConfiguration $LIBS"
AC_CHECK_FUNCS([SCDynamicStoreCopyComputerName], [
    AC_DEFINE([HAVE_SCDYNAMICSTORECOPYCOMPUTERNAME], [1], [Have SCDynamicStoreCopyComputerName function?])
],[
    LIBS="$SAVELIBS"
])


dnl Header files...
AC_CHECK_HEADER([CoreFoundation/CoreFoundation.h], [
    AC_DEFINE([HAVE_COREFOUNDATION_H], [1], [Have <CoreFoundation/CoreFoundation.h>?])
])
AC_CHECK_HEADER([langinfo.h], [
    AC_DEFINE([HAVE_LANGINFO_H], [1], [Have <langinfo.h> header?])
])
AC_CHECK_HEADER([resolv.h], [
    AC_DEFINE([HAVE_RESOLV_H], [1], [Have the <resolv.h> header?])
], [
], [
    #include <sys/socket.h>
    #include <netdb.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <netinet/in_systm.h>
    #include <netinet/ip.h>
])


dnl See if the stat structure has the st_gen member...
AC_MSG_CHECKING([for st_gen member in stat structure])
AC_COMPILE_IFELSE([
    AC_LANG_PROGRAM([[#include <sys/stat.h>]], [[
        struct stat t;
	int o = t.st_gen;
    ]])
], [
    AC_MSG_RESULT([yes])
    AC_DEFINE([HAVE_ST_GEN], [1], [Have st_gen member in struct stat?])
], [
    AC_MSG_RESULT([no])
])

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


dnl Checks for iconv.h and iconv_open
AC_CHECK_HEADER([iconv.h], [
    SAVELIBS="$LIBS"
    LIBS=""
    AC_SEARCH_LIBS([iconv_open], [iconv], [
        AC_DEFINE([HAVE_ICONV_H], [1], [Do we have the <iconv.h> header?])
	AS_IF([test "x$LIBS" = x -a x$host_os_name = xfreebsd], [
	    # FreeBSD (at least) has a false-positive match for iconv_open without -liconv...
	    LIBS="-liconv"
	])
	PKGCONFIG_LIBS_PRIVATE="$LIBS $PKGCONFIG_LIBS_PRIVATE"
	SAVELIBS="$SAVELIBS $LIBS"
    ])
    LIBS="$SAVELIBS"
])


dnl DNS-SD support...
AC_ARG_WITH([dnssd], AS_HELP_STRING([--with-dnssd=LIBRARY], [set DNS-SD library (auto, avahi, mdnsresponder)]))
DNSSD=0

AS_IF([test "x$with_dnssd" = xno], [
    AC_MSG_ERROR([Missing required Avahi/mDNSResponder DNS-SD library.])
], [test "x$with_dnssd" != xmdnsresponder -a "x$PKGCONFIG" != x -a x$host_os_name != xdarwin], [
    AC_MSG_CHECKING([for Avahi])
    AS_IF([$PKGCONFIG --exists avahi-client], [
	AC_MSG_RESULT([yes])
	CPPFLAGS="$CPPFLAGS $($PKGCONFIG --cflags avahi-client)"
	LIBS="$LIBS $($PKGCONFIG --libs avahi-client)"
	PKGCONFIG_LIBS_PRIVATE="$($PKGCONFIG --libs avahi-client) $PKGCONFIG_LIBS_PRIVATE"
	PKGCONFIG_REQUIRES_PRIVATE="avahi-client, $PKGCONFIG_REQUIRES_PRIVATE"
	DNSSD=1
	AC_DEFINE([HAVE_DNSSD], 1, [Have DNS-SD support?])
	AC_DEFINE([HAVE_AVAHI], 1, [Have Avahi?])
    ], [
	AC_MSG_RESULT([no])
	AS_IF([test x$with_dnssd = xavahi], [
	    AC_MSG_ERROR([libavahi-client-dev needed for --with-dnssd=avahi.])
	])
    ])
], [test x$with_dnssd = xavahi], [
    AC_MSG_ERROR([pkgconfig and libavahi-client-dev needed for --with-dnssd=avahi.])
], [
    AC_CHECK_HEADER(dns_sd.h, [
        AS_CASE(["$host_os_name"],
	[darwin*], [
	    # Darwin and macOS...
	    DNSSD=1
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
		DNSSD=1
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

AS_IF([test $DNSSD = 0], [
    AC_MSG_ERROR([Missing required Avahi/mDNSResponder DNS-SD library.])
])


dnl TLS support...
AC_ARG_WITH([tls], AS_HELP_STRING([--with-tls=...], [use gnutls or openssl/libressl for TLS support]))
AS_IF([test "x$with_tls" = x], [
    with_tls="yes"
], [test "$with_tls" != gnutls -a "$with_tls" != libressl -a "$with_tls" != openssl -a "$with_tls" != yes], [
    AC_MSG_ERROR([Unsupported --with-tls value "$with_tls" specified.])
])

have_tls="0"

dnl First look for OpenSSL/LibreSSL...
AS_IF([test $with_tls = yes -o $with_tls = libressl -o $with_tls = openssl], [
    AS_IF([test "x$PKGCONFIG" != x], [
        AC_MSG_CHECKING([for OpenSSL/LibreSSL package])
	AS_IF([$PKGCONFIG --exists openssl], [
	    AC_MSG_RESULT([yes])
	    have_tls="1"
	    AC_DEFINE([HAVE_TLS], [1], [Do we support TLS?])
	    AC_DEFINE([HAVE_OPENSSL], [1], [Do we have the OpenSSL/LibreSSL library?])
	    CPPFLAGS="$CPPFLAGS $($PKGCONFIG --cflags openssl)"
	    LIBS="$LIBS $($PKGCONFIG --libs openssl)"
	    PKGCONFIG_LIBS_PRIVATE="$($PKGCONFIG --libs openssl) $PKGCONFIG_LIBS_PRIVATE"
	    PKGCONFIG_REQUIRES_PRIVATE="openssl, $PKGCONFIG_REQUIRES"
	], [
	    AC_MSG_RESULT([no])
	])
    ])
])

AS_IF([test $have_tls = 0], [
    AS_IF([test $with_tls = yes -o $with_tls = gnutls], [
	AS_IF([test "x$PKGCONFIG" != x], [
	    AC_MSG_CHECKING([for GNU TLS package])
	    AS_IF([$PKGCONFIG --exists gnutls], [
		AC_MSG_RESULT([yes])
	        have_tls="1"
		AC_DEFINE([HAVE_TLS], [1], [Do we support TLS?])
		AC_DEFINE([HAVE_GNUTLS], [1], [Do we have the GNU TLS library?])
		CPPFLAGS="$CPPFLAGS `$PKGCONFIG --cflags gnutls`"
		LIBS="$LIBS `$PKGCONFIG --libs gnutls`"
		PKGCONFIG_LIBS_PRIVATE="$($PKGCONFIG --libs gnutls) $PKGCONFIG_LIBS_PRIVATE"
		PKGCONFIG_REQUIRES_PRIVATE="gnutls >= 3.0, $PKGCONFIG_REQUIRES_PRIVATE"
	    ], [
		AC_MSG_RESULT([no])
	    ])
	])
    ])
])

AS_IF([test $have_tls = 0], [
    AC_MSG_ERROR([TLS support is required.])
])


dnl Library targets...
AC_ARG_ENABLE([static], AS_HELP_STRING([--disable-static], [do not install static library]))
AC_ARG_ENABLE([shared], AS_HELP_STRING([--disable-shared], [do not install shared library]))

LIBCUPS_STATIC="$LIBCUPS_BASE.a"

AS_IF([test x$enable_shared != xno], [
    AS_CASE(["$host_os_name"], [linux* | gnu*], [
	LIBCUPS="$LIBCUPS_BASE.so.3"
	DSO="\$(CC)"
	DSOFLAGS="$DSOFLAGS -Wl,-soname,\`basename \$@\` -shared"
    ], [*bsd*], [
	LIBCUPS="$LIBCUPS_BASE.so.3"
	DSO="\$(CC)"
	DSOFLAGS="$DSOFLAGS -Wl,-soname,\`basename \$@\` -shared -lc"
    ], [darwin*], [
	LIBCUPS="$LIBCUPS_BASE.3.dylib"
	DSO="\$(CC)"
	DSOFLAGS="$DSOFLAGS -Wl,-no_warn_inits -dynamiclib -single_module -lc"
    ], [sunos*], [
	LIBCUPS="$LIBCUPS_BASE.so.3"
	DSO="\$(CC)"
	DSOFLAGS="$DSOFLAGS -Wl,-h,\`basename \$@\` -G"
    ], [*], [
	AC_MSG_NOTICE([Warning: Shared libraries may not work, trying -shared option.])
	LIBCUPS="$LIBCUPS_BASE.so.3"
	DSO="\$(CC)"
	DSOFLAGS="$DSOFLAGS -Wl,-soname,\`basename \$@\` -shared"
    ])

    AS_IF([test x$enable_static != xno], [
	AC_MSG_NOTICE([Installing static libraries...])
	INSTALL_STATIC="install-static"
    ], [
	INSTALL_STATIC=""
    ])
], [
    INSTALL_STATIC=""
    LIBCUPS="$LIBCUPS_BASE.a"
    PKGCONFIG_LIBS="$PKGCONFIG_LIBS $PKGCONFIG_LIBS_PRIVATE"
    PKGCONFIG_LIBS_PRIVATE=""
    PKGCONFIG_REQUIRES="$PKGCONFIG_REQUIRES $PKGCONFIG_REQUIRES_PRIVATE"
    PKGCONFIG_REQUIRES_PRIVATE=""
])

AC_SUBST([CUPS_PC])
AC_SUBST([INSTALL_STATIC])
AC_SUBST([LIBCUPS])
AC_SUBST([LIBCUPS_STATIC])


dnl Extra compiler options...
AC_ARG_ENABLE([debug], AS_HELP_STRING([--enable-debug], [turn on debugging, default=no]))
AC_ARG_ENABLE([maintainer], AS_HELP_STRING([--enable-maintainer], [turn on maintainer mode, default=no]))
AC_ARG_ENABLE([sanitizer], AS_HELP_STRING([--enable-sanitizer], [build with AddressSanitizer, default=no]))

AS_IF([test x$enable_debug = xyes], [
    CSFLAGS=""
    OPTIM="$OPTIM -g"
    OPTIONS="-DDEBUG -DDEBUG_GUARDS"
], [
    CSFLAGS="-o runtime"
    OPTIM="$OPTIM -g -Os"
    OPTIONS=""
])

AC_SUBST([CSFLAGS])
AC_SUBST([OPTIONS])

WARNINGS=""
AC_SUBST([WARNINGS])

AS_IF([test -n "$GCC"], [
    AS_IF([test x$enable_sanitizer = xyes], [
	# Use -fsanitize=address with debugging...
	OPTIM="$OPTIM -fsanitize=address"
    ], [
	# Otherwise use the Fortify enhancements to catch any unbounded
	# string operations...
	CPPFLAGS="$CPPFLAGS -D_FORTIFY_SOURCE=3"
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
    	# Make sure we get the full set of Linux APIs from the headers...
	CPPFLAGS="$CPPFLAGS -D__USE_MISC -D_GNU_SOURCE"

	# Mark read-only sections as relocatable to random addresses...
	LDFLAGS="$LDFLAGS -Wl,-z,relro,-z,now"
    ], [darwin*], [
        # When not building for debug, target macOS 10.14 or later, "universal"
        # binaries when possible...
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


dnl Fix prefix, datadir, datarootdir, includedir, and sysconfdir...
AS_IF([test "$prefix" = NONE], [
    # Default prefix isn't bound until AC_OUTPUT...
    realprefix="/usr/local"
], [
    realprefix="$prefix"
])

AS_IF([test "$datarootdir" = "\${prefix}/share"], [
    AS_IF([test "$prefix" = "/"], [
	datarootdir="/usr/share"
    ], [
	datarootdir="$prefix/share"
    ])
])

AS_IF([test "$datadir" = "\${prefix}/share"], [
    AS_IF([test "$prefix" = "/"], [
	datadir="/usr/share"
    ], [
	datadir="$prefix/share"
    ])
], [test "$datadir" = "\${datarootdir}"], [
    datadir="$datarootdir"
])

AS_IF([test "$includedir" = NONE -o "$includedir" = "\${prefix}/include"], [
    includedir="$realprefix/include$INCLUDE_PREFIX"
])
AS_IF([test "$includedir" != "/usr/include" -a "$includedir" != "/usr/local/include"], [
    PKGCONFIG_CFLAGS="-I\${includedir} $PKGCONFIG_CFLAGS"
])

AS_IF([test "$sysconfdir" = "\${prefix}/etc"], [
    AS_IF([test "$prefix" = "/"], [
	AS_IF([test "$host_os_name" = darwin], [
	    sysconfdir="/private/etc"
	], [
	    sysconfdir="/etc"
	])
    ], [
	sysconfdir="$prefix/etc"
    ])
])


dnl CUPS_DATADIR and CUPS_SERVERROOT
CUPS_DATADIR="$datadir/libcups3"
AC_DEFINE_UNQUOTED([CUPS_DATADIR], ["$datadir/libcups3"], [Location of data files.])
AC_SUBST([CUPS_DATADIR])

CUPS_SERVERROOT="$sysconfdir/cups"
AC_DEFINE_UNQUOTED([CUPS_SERVERROOT], ["$sysconfdir/cups"], [Location of server configuration files.])
AC_SUBST([CUPS_SERVERROOT])


dnl Domain socket support...
CUPS_DEFAULT_DOMAINSOCKET=""

AC_ARG_WITH([domainsocket], AS_HELP_STRING([--with-domainsocket], [set unix domain socket name]), [
    default_domainsocket="$withval"
], [
    default_domainsocket=""
])

AS_IF([test x$default_domainsocket != xno], [
    AS_IF([test "x$default_domainsocket" = x], [
        AS_CASE(["$host_os_name"], [darwin*], [
	    # Darwin and macOS do their own thing...
	    CUPS_DEFAULT_DOMAINSOCKET="/private/var/run/cupsd"
	], [*], [
	    # All others use FHS standard...
	    CUPS_DEFAULT_DOMAINSOCKET="/var/run/cups.sock"
	])
    ], [
	CUPS_DEFAULT_DOMAINSOCKET="$default_domainsocket"
    ])

    AC_DEFINE_UNQUOTED([CUPS_DEFAULT_DOMAINSOCKET], ["$CUPS_DEFAULT_DOMAINSOCKET"], [Domain socket path.])
])


dnl Clean up pkg-config "requires" content...
[PKGCONFIG_REQUIRES="$(echo $PKGCONFIG_REQUIRES | sed -e '1,$s/,$//')"]
[PKGCONFIG_REQUIRES_PRIVATE="$(echo $PKGCONFIG_REQUIRES_PRIVATE | sed -e '1,$s/,$//')"]


dnl Provide autoconf with a list of files to generate and output them...
AC_CONFIG_FILES([Makedefs cups3.pc packaging/libcups3.list])
AC_OUTPUT
