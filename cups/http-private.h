//
// Private HTTP definitions for CUPS.
//
// Copyright © 2021-2023 by OpenPrinting.
// Copyright © 2007-2018 by Apple Inc.
// Copyright © 1997-2007 by Easy Software Products, all rights reserved.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#ifndef _CUPS_HTTP_PRIVATE_H_
#  define _CUPS_HTTP_PRIVATE_H_
#  include "config.h"
#  include <cups/language.h>
#  include <stdlib.h>
#  ifdef __sun
#    include <sys/select.h>
#  endif // __sun
#  ifdef _WIN32
#    include <io.h>
#    include <winsock2.h>
#    define CUPS_SOCAST (const char *)
#  else
#    include <unistd.h>
#    include <fcntl.h>
#    include <sys/socket.h>
#    define CUPS_SOCAST
#  endif // _WIN32
#  include "http.h"
#  include "ipp-private.h"
#  ifdef HAVE_OPENSSL
#    include <openssl/err.h>
#    include <openssl/rand.h>
#    include <openssl/ssl.h>
#  elif defined(HAVE_GNUTLS)
#    include <gnutls/gnutls.h>
#    include <gnutls/x509.h>
#  endif // HAVE_OPENSSL
#  ifndef _WIN32
#    include <net/if.h>
#    include <resolv.h>
#  endif // !_WIN32
#  ifdef __cplusplus
extern "C" {
#  endif // __cplusplus


//
// Constants...
//

#  define _HTTP_MAX_BUFFER	32768	// Max length of data buffer
#  define _HTTP_MAX_SBUFFER	65536	// Size of (de)compression buffer
#  define _HTTP_MAX_VALUE	256	// Max header field value length

#  define _HTTP_TLS_NONE	0	// No TLS options
#  define _HTTP_TLS_ALLOW_RC4	1	// Allow RC4 cipher suites
#  define _HTTP_TLS_ALLOW_DH	2	// Allow DH/DHE key negotiation
#  define _HTTP_TLS_DENY_CBC	4	// Deny CBC cipher suites
#  define _HTTP_TLS_NO_SYSTEM	8	// No system crypto policy
#  define _HTTP_TLS_SET_DEFAULT 128     // Setting the default TLS options

#  define _HTTP_TLS_SSL3	0	// Min/max version is SSL/3.0
#  define _HTTP_TLS_1_0		1	// Min/max version is TLS/1.0
#  define _HTTP_TLS_1_1		2	// Min/max version is TLS/1.1
#  define _HTTP_TLS_1_2		3	// Min/max version is TLS/1.2
#  define _HTTP_TLS_1_3		4	// Min/max version is TLS/1.3
#  define _HTTP_TLS_MAX		5	// Highest known TLS version


//
// Types and functions for SSL support...
//

#  ifdef HAVE_OPENSSL
typedef SSL *_http_tls_t;
typedef struct _http_tls_credentials_s	// Internal credentials
{
  size_t	use;			// Use count
  STACK_OF(X509) *certs;		// X.509 certificates
  EVP_PKEY	*key;			// Private key
} _http_tls_credentials_t;
#  else // HAVE_GNUTLS
typedef gnutls_session_t _http_tls_t;
typedef struct _http_tls_credentials_s	// Internal credentials
{
  size_t	use;			// Use count
  gnutls_certificate_credentials_t creds;
					// X.509 certificates and private key
} _http_tls_credentials_t;
#  endif // HAVE_OPENSSL

typedef enum _http_coding_e		// HTTP content coding enumeration
{
  _HTTP_CODING_IDENTITY,		// No content coding
  _HTTP_CODING_GZIP,			// LZ77+gzip compression
  _HTTP_CODING_DEFLATE,			// LZ77+zlib compression
  _HTTP_CODING_GUNZIP,			// LZ77+gzip decompression
  _HTTP_CODING_INFLATE			// LZ77+zlib decompression
} _http_coding_t;

typedef enum _http_mode_e		// HTTP mode enumeration
{
  _HTTP_MODE_CLIENT,			// Client connected to server
  _HTTP_MODE_SERVER			// Server connected (accepted) from client
} _http_mode_t;

struct _http_s				// HTTP connection structure
{
  _http_mode_t		mode;		// _HTTP_MODE_CLIENT or _HTTP_MODE_SERVER
  char			hostname[HTTP_MAX_HOST];
  					// Name of connected host
  http_addr_t		*hostaddr;	// Current host address and port
  http_addrlist_t	*hostlist;	// List of valid addresses
  int			fd;		// File descriptor for this socket
  bool			blocking;	// To block or not to block
  int			error;		// Last error on read
  time_t		activity;	// Time since last read/write
  http_state_t		state;		// State of client
  http_status_t		status;		// Status of last request
  http_version_t	version;	// Protocol version
  char			*fields[HTTP_FIELD_MAX],
					// Allocated field values
  			*default_fields[HTTP_FIELD_MAX];
					// Default field values, if any
  char			*authstring;	// Current Authorization field
  char			*cookie;	// Cookie value(s)
  http_status_t		expect;		// Expect: header
  http_keepalive_t	keep_alive;	// Keep-alive supported?
  unsigned		nonce_count;	// Nonce count
  int			digest_tries;	// Number of tries for digest auth
  char			userpass[_HTTP_MAX_VALUE];
					// Username:password string
  char			*data;		// Pointer to data buffer
  http_encoding_t	data_encoding;	// Chunked or not
  off_t			data_remaining;	// Number of bytes left
  int			used;		// Number of bytes used in buffer
  char			buffer[_HTTP_MAX_BUFFER];
					// Buffer for incoming data
  char			algorithm[65],	// Algorithm from WWW-Authenticate
			nextnonce[_HTTP_MAX_VALUE],
					// Next nonce value from Authentication-Info
			nonce[_HTTP_MAX_VALUE],
					// Nonce value
			opaque[_HTTP_MAX_VALUE],
					// Opaque value from WWW-Authenticate
			qop[_HTTP_MAX_VALUE],
					// Quality of Protection (qop) value from WWW-Authenticate
			realm[_HTTP_MAX_VALUE];
					// Realm from WWW-Authenticate
  http_encryption_t	encryption;	// Encryption requirements
  _http_tls_t		tls;		// TLS state information
  _http_tls_credentials_t *tls_credentials;
					// TLS credentials
  bool			tls_upgrade;	// `true` if we are doing an upgrade
  char			wbuffer[_HTTP_MAX_BUFFER];
					// Buffer for outgoing data
  int			wused;		// Write buffer bytes used
					// TLS credentials
  http_timeout_cb_t	timeout_cb;	// Timeout callback
  void			*timeout_data;	// User data pointer
  double		timeout_value;	// Timeout in seconds
  int			wait_value;	// httpWait value for timeout
  _http_coding_t	coding;		// _HTTP_CODING_xxx
  void			*stream;	// (De)compression stream
  unsigned char		*sbuffer;	// (De)compression buffer
};


//
// Some OS's don't have hstrerror(), most notably Solaris...
//

#  ifndef HAVE_HSTRERROR
extern const char *_cups_hstrerror(int error);
#    define hstrerror _cups_hstrerror
#  endif // !HAVE_HSTRERROR


//
// Prototypes...
//

extern _http_tls_credentials_t *_httpCreateCredentials(const char *credentials, const char *key) _CUPS_PRIVATE;
extern char		*_httpDecodeURI(char *dst, const char *src, size_t dstsize) _CUPS_PRIVATE;
extern void		_httpDisconnect(http_t *http) _CUPS_PRIVATE;
extern char		*_httpEncodeURI(char *dst, const char *src, size_t dstsize) _CUPS_PRIVATE;
extern void		_httpFreeCredentials(_http_tls_credentials_t *hcreds) _CUPS_PRIVATE;
extern bool		_httpSetDigestAuthString(http_t *http, const char *nonce, const char *method, const char *resource) _CUPS_PRIVATE;
extern const char	*_httpStatusString(cups_lang_t *lang, http_status_t status) _CUPS_PRIVATE;
extern void		_httpTLSInitialize(void) _CUPS_PRIVATE;
extern size_t		_httpTLSPending(http_t *http) _CUPS_PRIVATE;
extern int		_httpTLSRead(http_t *http, char *buf, int len) _CUPS_PRIVATE;
extern void		_httpTLSSetOptions(int options, int min_version, int max_version) _CUPS_PRIVATE;
extern bool		_httpTLSStart(http_t *http) _CUPS_PRIVATE;
extern void		_httpTLSStop(http_t *http) _CUPS_PRIVATE;
extern int		_httpTLSWrite(http_t *http, const char *buf, int len) _CUPS_PRIVATE;
extern bool		_httpUpdate(http_t *http, http_status_t *status) _CUPS_PRIVATE;
extern _http_tls_credentials_t *_httpUseCredentials(_http_tls_credentials_t *hcreds) _CUPS_PRIVATE;
extern bool		_httpWait(http_t *http, int msec, bool usessl) _CUPS_PRIVATE;


#  ifdef __cplusplus
}
#  endif // __cplusplus
#endif // !_CUPS_HTTP_PRIVATE_H_
