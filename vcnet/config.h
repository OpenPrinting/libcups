//
// Configuration file for libcups with Visual Studio on Windows.
//
// Copyright © 2021-2024 by OpenPrinting
// Copyright © 2007-2019 by Apple Inc.
// Copyright © 1997-2007 by Easy Software Products.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#ifndef CUPS_CONFIG_H
#define CUPS_CONFIG_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <io.h>
#include <fcntl.h>
#include <direct.h>


//
// Microsoft renames the POSIX functions to _name, and introduces
// a broken compatibility layer using the original names.  As a result,
// random crashes can occur when, for example, strdup() allocates memory
// from a different heap than used by malloc() and free().
//
// To avoid moronic problems like this, we #define the POSIX function
// names to the corresponding non-standard Microsoft names.
//

#define access		_access
#define close		_close
#define fileno		_fileno
#define lseek		_lseek
#define mkdir(d,p)	_mkdir(d)
#define open		_open
#define read	        _read
#define rmdir		_rmdir
#define snprintf	_snprintf
#define strdup		_strdup
#define unlink		_unlink
#define vsnprintf	_vsnprintf
#define write		_write
#define poll		WSAPoll		// WinSock...


//
// Microsoft "safe" functions use a different argument order than POSIX...
//

#define gmtime_r(t,tm)	gmtime_s(tm,t)
#define localtime_r(t,tm) localtime_s(tm,t)


//
// Map the POSIX strcasecmp() and strncasecmp() functions to the Win32
// _stricmp() and _strnicmp() functions...
//

#define strcasecmp	_stricmp
#define strncasecmp	_strnicmp


//
// Map the POSIX sleep() and usleep() functions to the Win32 Sleep() function...
//

typedef unsigned long useconds_t;
#define sleep(X)	Sleep(1000 * (X))
#define usleep(X)	Sleep((X)/1000)


//
// Map various parameters to Posix style system calls
//

#  define F_OK		00
#  define W_OK		02
#  define R_OK		04
#  define O_RDONLY	_O_RDONLY
#  define O_WRONLY	_O_WRONLY
#  define O_CREAT	_O_CREAT
#  define O_TRUNC	_O_TRUNC


//
// Compiler stuff...
//

#undef const
#undef __CHAR_UNSIGNED__


//
// Version of software...
//

#define LIBCUPS_VERSION "3.0rc4"
#define LIBCUPS_VERSION_MAJOR 3
#define LIBCUPS_VERSION_MINOR 0


//
// Do we have domain socket support, and if so what is the default one?
//

#undef CUPS_DEFAULT_DOMAINSOCKET


//
// Where are files stored?
//
// Note: These are defaults, which can be overridden by environment
//       variables at run-time...
//

#define CUPS_DATADIR "C:/CUPS/share"
#define CUPS_SERVERROOT "C:/CUPS/etc"


//
// Use <stdint.h>?
//

// #undef HAVE_STDINT_H


//
// Do we have the long long type?
//

// #undef HAVE_LONG_LONG

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

// #undef HAVE_STRTOLL

#ifndef HAVE_STRTOLL
#  define strtoll(nptr,endptr,base) strtol((nptr), (endptr), (base))
#endif // !HAVE_STRTOLL


//
// Do we have the geteuid() function?
//

// #undef HAVE_GETEUID


//
// Do we have the langinfo.h header file?
//

// #undef HAVE_LANGINFO_H


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

// #undef HAVE_HSTRERROR


//
// Do we have res_init()?
//

// #undef HAVE_RES_INIT


//
// Do we have <resolv.h>
//

// #undef HAVE_RESOLV_H


//
// Do we have CoreFoundation?
//

// #undef HAVE_COREFOUNDATION_H


//
// Do we have CoreGraphics?
//

// #undef HAVE_COREGRAPHICS_H


//
// Do we have the SCDynamicStoreCopyComputerName function?
//

// #undef HAVE_SCDYNAMICSTORECOPYCOMPUTERNAME


//
// Do we have the <iconv.h> header?
//

// #undef HAVE_ICONV_H


//
// Do we have statfs or statvfs and one of the corresponding headers?
//

// #undef HAVE_STATFS
// #undef HAVE_STATVFS
// #undef HAVE_SYS_MOUNT_H
// #undef HAVE_SYS_STATFS_H
// #undef HAVE_SYS_STATVFS_H
// #undef HAVE_SYS_VFS_H


//
// Have dbus library?
//

// #undef HAVE_DBUS


#endif // !CUPS_CONFIG_H
