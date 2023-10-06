//
// Configuration file for libcups with Xcode.
//
// Copyright © 2020-2023 by OpenPrinting
// Copyright © 2007-2019 by Apple Inc.
// Copyright © 1997-2007 by Easy Software Products.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#ifndef CUPS_CONFIG_H
#define CUPS_CONFIG_H


//
// Version of software...
//

#define LIBCUPS_VERSION "3.0b2"
#define LIBCUPS_VERSION_MAJOR 3
#define LIBCUPS_VERSION_MINOR 0


//
// Do we have domain socket support, and if so what is the default one?
//

// #undef CUPS_DEFAULT_DOMAINSOCKET


//
// Where are files stored?
//
// Note: These are defaults, which can be overridden by environment
//       variables at run-time...
//

#define CUPS_DATADIR    "/usr/share/cups"
#define CUPS_SERVERROOT	"/etc/cups"


//
// Use <stdint.h>?
//

#define HAVE_STDINT_H 1


//
// Do we have the long long type?
//

#define HAVE_LONG_LONG 1

#ifdef HAVE_LONG_LONG
#  define CUPS_LLFMT	"%lld"
#  define CUPS_LLCAST	(long long)
#else
#  define CUPS_LLFMT	"%ld"
#  define CUPS_LLCAST	(long)
#endif // HAVE_LONG_LONG


//
// Do we have the strtoll() function?
//

#define HAVE_STRTOLL 1

#ifndef HAVE_STRTOLL
#  define strtoll(nptr,endptr,base) strtol((nptr), (endptr), (base))
#endif // !HAVE_STRTOLL


//
// Do we have the strlcat() or strlcpy() functions?
//

#define HAVE_STRLCAT 1
#define HAVE_STRLCPY 1


//
// Do we have the geteuid() function?
//

// #undef HAVE_GETEUID


//
// Do we have the langinfo.h header file?
//

#define HAVE_LANGINFO_H 1


//
// Which encryption libraries do we have?
//

#define HAVE_OPENSSL 1
// #undef HAVE_GNUTLS


//
// Do we have the gnutls_transport_set_pull_timeout_function function?
//

// #undef HAVE_GNUTLS_TRANSPORT_SET_PULL_TIMEOUT_FUNCTION


//
// Do we have the gnutls_priority_set_direct function?
//

// #undef HAVE_GNUTLS_PRIORITY_SET_DIRECT


//
// Do we have DNS Service Discovery (aka Bonjour) support?
//

#define HAVE_DNSSD 1


//
// Do we have mDNSResponder for DNS-SD?
//

#define HAVE_MDNSRESPONDER 1


//
// Do we have Avahi for DNS-SD?
//

// #undef HAVE_AVAHI


//
// Does the "stat" structure contain the "st_gen" member?
//

// #undef HAVE_ST_GEN


//
// Does the "tm" structure contain the "tm_gmtoff" member?
//

// #undef HAVE_TM_GMTOFF


//
// Do we have hstrerror()?
//

#define HAVE_HSTRERROR 1


//
// Do we have res_init()?
//

#define HAVE_RES_INIT 1


//
// Do we have <resolv.h>
//

#define HAVE_RESOLV_H 1


//
// Do we have CoreFoundation?
//

#define HAVE_COREFOUNDATION_H 1


//
// Do we have CoreGraphics?
//

#define HAVE_COREGRAPHICS_H 1


//
// Do we have the SCDynamicStoreCopyComputerName function?
//

// #undef HAVE_SCDYNAMICSTORECOPYCOMPUTERNAME


//
// Do we have the <iconv.h> header?
//

#define HAVE_ICONV_H 1


//
// Do we have statfs or statvfs and one of the corresponding headers?
//

// #undef HAVE_STATFS
// #undef HAVE_STATVFS
// #undef HAVE_SYS_MOUNT_H
// #undef HAVE_SYS_STATFS_H
// #undef HAVE_SYS_STATVFS_H
// #undef HAVE_SYS_VFS_H


#endif // !CUPS_CONFIG_H
