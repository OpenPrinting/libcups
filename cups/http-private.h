/*
 * Private HTTP definitions for CUPS.
 *
 * Copyright © 2021 by OpenPrinting.
 * Copyright © 2007-2018 by Apple Inc.
 * Copyright © 1997-2007 by Easy Software Products, all rights reserved.
 *
 * Licensed under Apache License v2.0.  See the file "LICENSE" for more
 * information.
 */

#ifndef _CUPS_HTTP_PRIVATE_H_
#  define _CUPS_HTTP_PRIVATE_H_

/*
 * Include necessary headers...
 */

#  include "config.h"
#  include <cups/language.h>
#  include <stddef.h>
#  include <stdlib.h>

#  ifdef __sun
#    include <sys/select.h>
#  endif /* __sun */

#  include <limits.h>
#  ifdef _WIN32
#    define _WINSOCK_DEPRECATED_NO_WARNINGS 1
#    include <io.h>
#    include <winsock2.h>
#    define CUPS_SOCAST (const char *)
#  else
#    include <unistd.h>
#    include <fcntl.h>
#    include <sys/socket.h>
#    define CUPS_SOCAST
#  endif /* _WIN32 */

#  ifdef HAVE_AUTHORIZATION_H
#    include <Security/Authorization.h>
#  endif /* HAVE_AUTHORIZATION_H */

#  if defined(__APPLE__) && !defined(_SOCKLEN_T)
/*
 * macOS 10.2.x does not define socklen_t, and in fact uses an int instead of
 * unsigned type for length values...
 */

typedef int socklen_t;
#  endif /* __APPLE__ && !_SOCKLEN_T */

#  include <cups/http.h>
#  include "ipp-private.h"

#  ifdef HAVE_GNUTLS
#    include <gnutls/gnutls.h>
#    include <gnutls/x509.h>
#  elif defined(HAVE_CDSASSL)
#    include <CoreFoundation/CoreFoundation.h>
#    include <Security/Security.h>
#    include <Security/SecureTransport.h>
#    ifdef HAVE_SECITEM_H
#      include <Security/SecItem.h>
#    endif /* HAVE_SECITEM_H */
#    ifdef HAVE_SECCERTIFICATE_H
#      include <Security/SecCertificate.h>
#      include <Security/SecIdentity.h>
#    endif /* HAVE_SECCERTIFICATE_H */
#  elif defined(HAVE_SSPISSL)
#    include <wincrypt.h>
#    include <wintrust.h>
#    include <schannel.h>
#    define SECURITY_WIN32
#    include <security.h>
#    include <sspi.h>
#  endif /* HAVE_GNUTLS */

#  ifndef _WIN32
#    include <net/if.h>
#    include <resolv.h>
#  endif /* !_WIN32 */


/*
 * C++ magic...
 */

#  ifdef __cplusplus
extern "C" {
#  endif /* __cplusplus */


/*
 * Constants...
 */

#  define _HTTP_MAX_SBUFFER	65536	/* Size of (de)compression buffer */
#  define _HTTP_RESOLVE_DEFAULT	0	/* Just resolve with default options */
#  define _HTTP_RESOLVE_STDERR	1	/* Log resolve progress to stderr */
#  define _HTTP_RESOLVE_FQDN	2	/* Resolve to a FQDN */
#  define _HTTP_RESOLVE_FAXOUT	4	/* Resolve FaxOut service? */

#  define _HTTP_TLS_NONE	0	/* No TLS options */
#  define _HTTP_TLS_ALLOW_RC4	1	/* Allow RC4 cipher suites */
#  define _HTTP_TLS_ALLOW_DH	2	/* Allow DH/DHE key negotiation */
#  define _HTTP_TLS_DENY_CBC	4	/* Deny CBC cipher suites */
#  define _HTTP_TLS_SET_DEFAULT 128     /* Setting the default TLS options */

#  define _HTTP_TLS_SSL3	0	/* Min/max version is SSL/3.0 */
#  define _HTTP_TLS_1_0		1	/* Min/max version is TLS/1.0 */
#  define _HTTP_TLS_1_1		2	/* Min/max version is TLS/1.1 */
#  define _HTTP_TLS_1_2		3	/* Min/max version is TLS/1.2 */
#  define _HTTP_TLS_1_3		4	/* Min/max version is TLS/1.3 */
#  define _HTTP_TLS_MAX		5	/* Highest known TLS version */


/*
 * Types and functions for SSL support...
 */

#  ifdef HAVE_GNUTLS
/*
 * The GNU TLS library is more of a "bare metal" SSL/TLS library...
 */

typedef gnutls_session_t http_tls_t;
typedef gnutls_certificate_credentials_t *http_tls_credentials_t;

#  elif defined(HAVE_CDSASSL)
/*
 * Darwin's Security framework provides its own SSL/TLS context structure
 * for its IO and protocol management...
 */

typedef SSLContextRef	http_tls_t;
typedef CFArrayRef	http_tls_credentials_t;

#  elif defined(HAVE_SSPISSL)
/*
 * Windows' SSPI library gets a CUPS wrapper...
 */

typedef struct _http_sspi_s		/**** SSPI/SSL data structure ****/
{
  CredHandle	creds;			/* Credentials */
  CtxtHandle	context;		/* SSL context */
  BOOL		contextInitialized;	/* Is context init'd? */
  SecPkgContext_StreamSizes streamSizes;/* SSL data stream sizes */
  BYTE		*decryptBuffer;		/* Data pre-decryption*/
  size_t	decryptBufferLength;	/* Length of decrypt buffer */
  size_t	decryptBufferUsed;	/* Bytes used in buffer */
  BYTE		*readBuffer;		/* Data post-decryption */
  int		readBufferLength;	/* Length of read buffer */
  int		readBufferUsed;		/* Bytes used in buffer */
  BYTE		*writeBuffer;		/* Data pre-encryption */
  int		writeBufferLength;	/* Length of write buffer */
  PCCERT_CONTEXT localCert,		/* Local certificate */
		remoteCert;		/* Remote (peer's) certificate */
  char		error[256];		/* Most recent error message */
} _http_sspi_t;
typedef _http_sspi_t *http_tls_t;
typedef PCCERT_CONTEXT http_tls_credentials_t;

#  else
/*
 * Otherwise define stub types since we have no SSL support...
 */

typedef void *http_tls_t;
typedef void *http_tls_credentials_t;
#  endif /* HAVE_GNUTLS */

typedef enum _http_coding_e		/**** HTTP content coding enumeration ****/
{
  _HTTP_CODING_IDENTITY,		/* No content coding */
  _HTTP_CODING_GZIP,			/* LZ77+gzip decompression */
  _HTTP_CODING_DEFLATE,			/* LZ77+zlib compression */
  _HTTP_CODING_GUNZIP,			/* LZ77+gzip decompression */
  _HTTP_CODING_INFLATE			/* LZ77+zlib decompression */
} _http_coding_t;

typedef enum _http_mode_e		/**** HTTP mode enumeration ****/
{
  _HTTP_MODE_CLIENT,			/* Client connected to server */
  _HTTP_MODE_SERVER			/* Server connected (accepted) from client */
} _http_mode_t;

#  ifndef _HTTP_NO_PRIVATE
struct _http_s				/**** HTTP connection structure ****/
{
  _http_mode_t		mode;		/* _HTTP_MODE_CLIENT or _HTTP_MODE_SERVER */
  char			hostname[HTTP_MAX_HOST];
  					/* Name of connected host */
  http_addr_t		*hostaddr;	/* Current host address and port */
  http_addrlist_t	*hostlist;	/* List of valid addresses */
  int			fd;		/* File descriptor for this socket */
  bool			blocking;	/* To block or not to block */
  int			error;		/* Last error on read */
  time_t		activity;	/* Time since last read/write */
  http_state_t		state;		/* State of client */
  http_status_t		status;		/* Status of last request */
  http_version_t	version;	/* Protocol version */
  char			*fields[HTTP_FIELD_MAX],
					/* Allocated field values */
  			*default_fields[HTTP_FIELD_MAX];
					/* Default field values, if any */
  char			*authstring;	/* Current Authorization field */
  char			*cookie;	/* Cookie value(s) */
  http_status_t		expect;		/* Expect: header */
  http_keepalive_t	keep_alive;	/* Keep-alive supported? */
  unsigned		nonce_count;	/* Nonce count */
  int			digest_tries;	/* Number of tries for digest auth */
  char			userpass[HTTP_MAX_VALUE];
					/* Username:password string */
  char			*data;		/* Pointer to data buffer */
  http_encoding_t	data_encoding;	/* Chunked or not */
  off_t			data_remaining;	/* Number of bytes left */
  int			used;		/* Number of bytes used in buffer */
  char			buffer[HTTP_MAX_BUFFER];
					/* Buffer for incoming data */
  char			algorithm[65],	/* Algorithm from WWW-Authenticate */
			nextnonce[HTTP_MAX_VALUE],
					/* Next nonce value from Authentication-Info */
			nonce[HTTP_MAX_VALUE],
					/* Nonce value */
			opaque[HTTP_MAX_VALUE],
					/* Opaque value from WWW-Authenticate */
			realm[HTTP_MAX_VALUE];
					/* Realm from WWW-Authenticate */
  http_encryption_t	encryption;	/* Encryption requirements */
  http_tls_t		tls;		/* TLS state information */
  http_tls_credentials_t tls_credentials;
  bool			tls_upgrade;	/* `true` if we are doing an upgrade */
  char			wbuffer[HTTP_MAX_BUFFER];
					/* Buffer for outgoing data */
  int			wused;		/* Write buffer bytes used */
					/* TLS credentials */
  http_timeout_cb_t	timeout_cb;	/* Timeout callback */
  void			*timeout_data;	/* User data pointer */
  double		timeout_value;	/* Timeout in seconds */
  int			wait_value;	/* httpWait value for timeout */
  _http_coding_t	coding;		/* _HTTP_CODING_xxx */
  void			*stream;	/* (De)compression stream */
  unsigned char		*sbuffer;	/* (De)compression buffer */
};
#  endif /* !_HTTP_NO_PRIVATE */


/*
 * Some OS's don't have hstrerror(), most notably Solaris...
 */

#  ifndef HAVE_HSTRERROR
extern const char *_cups_hstrerror(int error);
#    define hstrerror _cups_hstrerror
#  endif /* !HAVE_HSTRERROR */


/*
 * Prototypes...
 */

extern void		_httpAddrSetPort(http_addr_t *addr, int port) _CUPS_PRIVATE;
extern http_tls_credentials_t _httpCreateCredentials(cups_array_t *credentials) _CUPS_PRIVATE;
extern char		*_httpDecodeURI(char *dst, const char *src, size_t dstsize) _CUPS_PRIVATE;
extern void		_httpDisconnect(http_t *http) _CUPS_PRIVATE;
extern char		*_httpEncodeURI(char *dst, const char *src, size_t dstsize) _CUPS_PRIVATE;
extern void		_httpFreeCredentials(http_tls_credentials_t credentials) _CUPS_PRIVATE;
extern const char	*_httpResolveURI(const char *uri, char *resolved_uri, size_t resolved_size, int options, int (*cb)(void *context), void *context) _CUPS_PRIVATE;
extern int		_httpSetDigestAuthString(http_t *http, const char *nonce, const char *method, const char *resource) _CUPS_PRIVATE;
extern const char	*_httpStatus(cups_lang_t *lang, http_status_t status) _CUPS_PRIVATE;
extern void		_httpTLSInitialize(void) _CUPS_PRIVATE;
extern size_t		_httpTLSPending(http_t *http) _CUPS_PRIVATE;
extern int		_httpTLSRead(http_t *http, char *buf, int len) _CUPS_PRIVATE;
extern void		_httpTLSSetOptions(int options, int min_version, int max_version) _CUPS_PRIVATE;
extern int		_httpTLSStart(http_t *http) _CUPS_PRIVATE;
extern void		_httpTLSStop(http_t *http) _CUPS_PRIVATE;
extern int		_httpTLSWrite(http_t *http, const char *buf, int len) _CUPS_PRIVATE;
extern int		_httpUpdate(http_t *http, http_status_t *status) _CUPS_PRIVATE;
extern int		_httpWait(http_t *http, int msec, int usessl) _CUPS_PRIVATE;


/*
 * C++ magic...
 */

#  ifdef __cplusplus
}
#  endif /* __cplusplus */

#endif /* !_CUPS_HTTP_PRIVATE_H_ */
