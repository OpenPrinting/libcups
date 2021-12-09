/*
 * Hyper-Text Transport Protocol definitions for CUPS.
 *
 * Copyright © 2021 by OpenPrinting.
 * Copyright © 2007-2018 by Apple Inc.
 * Copyright © 1997-2007 by Easy Software Products, all rights reserved.
 *
 * Licensed under Apache License v2.0.  See the file "LICENSE" for more
 * information.
 */

#ifndef _CUPS_HTTP_H_
#  define _CUPS_HTTP_H_
#  include "array.h"
#  include <string.h>
#  include <time.h>
#  ifdef _WIN32
#    include <winsock2.h>
#    include <ws2tcpip.h>
#  else
#    include <unistd.h>
#    include <sys/time.h>
#    include <sys/socket.h>
#    include <netdb.h>
#    include <netinet/in.h>
#    include <arpa/inet.h>
#    include <netinet/in_systm.h>
#    include <netinet/ip.h>
#    if !defined(__APPLE__) || !defined(TCP_NODELAY)
#      include <netinet/tcp.h>
#    endif /* !__APPLE__ || !TCP_NODELAY */
#    if defined(AF_UNIX) && !defined(AF_LOCAL)
#      define AF_LOCAL AF_UNIX		/* Older UNIX's have old names... */
#    endif /* AF_UNIX && !AF_LOCAL */
#    ifdef AF_LOCAL
#      include <sys/un.h>
#    endif /* AF_LOCAL */
#    if defined(LOCAL_PEERCRED) && !defined(SO_PEERCRED)
#      define SO_PEERCRED LOCAL_PEERCRED
#    endif /* LOCAL_PEERCRED && !SO_PEERCRED */
#  endif /* _WIN32 */
#  ifdef __cplusplus
extern "C" {
#  endif /* __cplusplus */


/*
 * Oh, the wonderful world of IPv6 compatibility.  Apparently some
 * implementations expose the (more logical) 32-bit address parts
 * to everyone, while others only expose it to kernel code...  To
 * make supporting IPv6 even easier, each vendor chose different
 * core structure and union names, so the same defines or code
 * can't be used on all platforms.
 *
 * The following will likely need tweaking on new platforms that
 * support IPv6 - the "s6_addr32" define maps to the 32-bit integer
 * array in the in6_addr union, which is named differently on various
 * platforms.
 */

#if defined(AF_INET6) && !defined(s6_addr32)
#  if defined(__sun)
#    define s6_addr32	_S6_un._S6_u32
#  elif defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__APPLE__)|| defined(__DragonFly__)
#    define s6_addr32	__u6_addr.__u6_addr32
#  elif defined(_WIN32)
/*
 * Windows only defines byte and 16-bit word members of the union and
 * requires special casing of all raw address code...
 */
#    define s6_addr32	error_need_win32_specific_code
#  endif /* __sun */
#endif /* AF_INET6 && !s6_addr32 */


/*
 * Limits...
 */

#  define HTTP_MAX_URI		1024	/* Max length of URI string */
#  define HTTP_MAX_HOST		256	/* Max length of hostname string */
#  define HTTP_MAX_BUFFER	2048	/* Max length of data buffer */
#  define HTTP_MAX_VALUE	256	/* Max header field value length */


/*
 * Types and structures...
 */

typedef enum http_auth_e		/**** HTTP authentication types @exclude all@ ****/
{
  HTTP_AUTH_NONE,			/* No authentication in use */
  HTTP_AUTH_BASIC,			/* Basic authentication in use */
  HTTP_AUTH_MD5,			/* Digest authentication in use */
  HTTP_AUTH_MD5_SESS,			/* MD5-session authentication in use */
  HTTP_AUTH_MD5_INT,			/* Digest authentication in use for body */
  HTTP_AUTH_MD5_SESS_INT,		/* MD5-session authentication in use for body */
  HTTP_AUTH_NEGOTIATE			/* GSSAPI authentication in use */
} http_auth_t;

typedef enum http_encoding_e		/**** HTTP transfer encoding values ****/
{
  HTTP_ENCODING_LENGTH,			/* Data is sent with Content-Length */
  HTTP_ENCODING_CHUNKED,		/* Data is chunked */
  HTTP_ENCODING_FIELDS			/* Sending HTTP fields */
} http_encoding_t;

typedef enum http_encryption_e		/**** HTTP encryption values ****/
{
  HTTP_ENCRYPTION_IF_REQUESTED,		/* Encrypt if requested (TLS upgrade) */
  HTTP_ENCRYPTION_NEVER,		/* Never encrypt */
  HTTP_ENCRYPTION_REQUIRED,		/* Encryption is required (TLS upgrade) */
  HTTP_ENCRYPTION_ALWAYS		/* Always encrypt (HTTPS) */
} http_encryption_t;

typedef enum http_field_e		/**** HTTP field names ****/
{
  HTTP_FIELD_UNKNOWN = -1,		/* Unknown field */
  HTTP_FIELD_ACCEPT,			/* Accept field */
  HTTP_FIELD_ACCEPT_CH,			/* RFC 8942 Accept-CH field */
  HTTP_FIELD_ACCEPT_ENCODING,		/* Accept-Encoding field */
  HTTP_FIELD_ACCEPT_LANGUAGE,		/* Accept-Language field */
  HTTP_FIELD_ACCEPT_RANGES,		/* Accept-Ranges field */
  HTTP_FIELD_ACCESS_CONTROL_ALLOW_CREDENTIALS,
					/* CORS/Fetch Access-Control-Allow-Cresdentials field */
  HTTP_FIELD_ACCESS_CONTROL_ALLOW_HEADERS,
					/* CORS/Fetch Access-Control-Allow-Headers field */
  HTTP_FIELD_ACCESS_CONTROL_ALLOW_METHODS,
					/* CORS/Fetch Access-Control-Allow-Methods field */
  HTTP_FIELD_ACCESS_CONTROL_ALLOW_ORIGIN,
					/* CORS/Fetch Access-Control-Allow-Origin field */
  HTTP_FIELD_ACCESS_CONTROL_EXPOSE_HEADERS,
					/* CORS/Fetch Access-Control-Expose-Headers field */
  HTTP_FIELD_ACCESS_CONTROL_MAX_AGE,	/* CORS/Fetch Access-Control-Max-Age field */
  HTTP_FIELD_ACCESS_CONTROL_REQUEST_HEADERS,
					/* CORS/Fetch Access-Control-Request-Headers field */
  HTTP_FIELD_ACCESS_CONTROL_REQUEST_METHOD,
					/* CORS/Fetch Access-Control-Request-Method field */
  HTTP_FIELD_AGE,			/* Age field */
  HTTP_FIELD_ALLOW,			/* Allow field */
  HTTP_FIELD_AUTHENTICATION_CONTROL,	/* RFC 8053 Authentication-Control field */
  HTTP_FIELD_AUTHENTICATION_INFO,	/* Authentication-Info field */
  HTTP_FIELD_AUTHORIZATION,		/* Authorization field */
  HTTP_FIELD_CACHE_CONTROL,		/* Cache-Control field */
  HTTP_FIELD_CACHE_STATUS,		/* Cache-Status field */
  HTTP_FIELD_CERT_NOT_AFTER,		/* RFC 8739 (ACME) Cert-Not-After field */
  HTTP_FIELD_CERT_NOT_BEFORE,		/* RFC 8739 (ACME) Cert-Not-Before field */
  HTTP_FIELD_CONNECTION,		/* Connection field */
  HTTP_FIELD_CONTENT_DISPOSITION,	/* RFC 6266 Content-Disposition field */
  HTTP_FIELD_CONTENT_ENCODING,		/* Content-Encoding field */
  HTTP_FIELD_CONTENT_LANGUAGE,		/* Content-Language field */
  HTTP_FIELD_CONTENT_LENGTH,		/* Content-Length field */
  HTTP_FIELD_CONTENT_LOCATION,		/* Content-Location field */
  HTTP_FIELD_CONTENT_RANGE,		/* Content-Range field */
  HTTP_FIELD_CONTENT_SECURITY_POLICY,	/* CSPL3 Content-Security-Policy field */
  HTTP_FIELD_CONTENT_SECURITY_POLICY_REPORT_ONLY,
					/* CSPL3 Content-Security-Policy-Report-Only field */
  HTTP_FIELD_CONTENT_TYPE,		/* Content-Type field */
  HTTP_FIELD_CROSS_ORIGIN_EMBEDDER_POLICY,
					/* WHATWG Cross-Origin-Embedder-Policy field */
  HTTP_FIELD_CROSS_ORIGIN_EMBEDDER_POLICY_REPORT_ONLY,
					/* WHATWG Cross-Origin-Embedder-Policy-Report-Only field */
  HTTP_FIELD_CROSS_ORIGIN_OPENER_POLICY,/* WHATWG Cross-Origin-Opener-Policy field */
  HTTP_FIELD_CROSS_ORIGIN_OPENER_POLICY_REPORT_ONLY,
					/* WHATWG Cross-Origin-Opener-Policy-Report-Only field */
  HTTP_FIELD_CROSS_ORIGIN_RESOURCE_POLICY,
					/* WHATWG Cross-Origin-Resource-Policy field */
  HTTP_FIELD_DASL,			/* RFC 5323 (WebDAV) DASL field */
  HTTP_FIELD_DATE,			/* Date field */
  HTTP_FIELD_DAV,			/* RFC 4918 (WebDAV) DAV field */
  HTTP_FIELD_DEPTH,			/* RFC 4918 (WebDAV) Depth field */
  HTTP_FIELD_DESTINATION,		/* RFC 4918 (WebDAV) Destination field */
  HTTP_FIELD_ETAG,			/* ETag field */
  HTTP_FIELD_EXPIRES,			/* Expires field */
  HTTP_FIELD_FORWARDED,			/* RFC 7239 Forwarded field */
  HTTP_FIELD_FROM,			/* From field */
  HTTP_FIELD_HOST,			/* Host field */
  HTTP_FIELD_IF,			/* RFC 4918 (WebDAV) If field */
  HTTP_FIELD_IF_MATCH,			/* If-Match field */
  HTTP_FIELD_IF_MODIFIED_SINCE,		/* If-Modified-Since field */
  HTTP_FIELD_IF_NONE_MATCH,		/* If-None-Match field */
  HTTP_FIELD_IF_RANGE,			/* If-Range field */
  HTTP_FIELD_IF_SCHEDULE_TAG_MATCH,	/* RFC 6638 (CalDAV) If-Schedule-Tag-Match field */
  HTTP_FIELD_IF_UNMODIFIED_SINCE,	/* If-Unmodified-Since field */
  HTTP_FIELD_KEEP_ALIVE,		/* Keep-Alive field */
  HTTP_FIELD_LAST_MODIFIED,		/* Last-Modified field */
  HTTP_FIELD_LINK,			/* Link field */
  HTTP_FIELD_LOCATION,			/* Location field */
  HTTP_FIELD_LOCK_TOKEN,		/* RFC 4918 (WebDAV) Lock-Token field */
  HTTP_FIELD_MAX_FORWARDS,		/* Max-Forwards field */
  HTTP_FIELD_OPTIONAL_WWW_AUTHENTICATE,	/* RFC 8053 Optional-WWW-Authenticate field */
  HTTP_FIELD_ORIGIN,			/* RFC 6454 Origin field */
  HTTP_FIELD_OSCORE,			/* RFC 8613 OSCORE field */
  HTTP_FIELD_OVERWRITE,			/* RFC 4918 (WebDAV) Overwrite field */
  HTTP_FIELD_PRAGMA,			/* Pragma field */
  HTTP_FIELD_PROXY_AUTHENTICATE,	/* Proxy-Authenticate field */
  HTTP_FIELD_PROXY_AUTHENTICATION_INFO,	/* Proxy-Authentication-Info field */
  HTTP_FIELD_PROXY_AUTHORIZATION,	/* Proxy-Authorization field */
  HTTP_FIELD_PROXY_STATUS,		/* Proxy-Status field */
  HTTP_FIELD_PUBLIC,			/* Public field */
  HTTP_FIELD_RANGE,			/* Range field */
  HTTP_FIELD_REFERER,			/* Referer field */
  HTTP_FIELD_REFRESH,			/* WHATWG Refresh field */
  HTTP_FIELD_REPLAY_NONCE,		/* RFC 8555 (ACME) Replay-Nonce field */
  HTTP_FIELD_RETRY_AFTER,		/* Retry-After field */
  HTTP_FIELD_SCHEDULE_REPLY,		/* RFC 6638 (CalDAV) Schedule-Reply field */
  HTTP_FIELD_SCHEDULE_TAG,		/* RFC 6638 (CalDAV) Schedule-Tag field */
  HTTP_FIELD_SERVER,			/* Server field */
  HTTP_FIELD_STRICT_TRANSPORT_SECURITY,	/* HSTS Strict-Transport-Security field */
  HTTP_FIELD_TE,			/* TE field */
  HTTP_FIELD_TIMEOUT,			/* RFC 4918 Timeout field */
  HTTP_FIELD_TRAILER,			/* Trailer field */
  HTTP_FIELD_TRANSFER_ENCODING,		/* Transfer-Encoding field */
  HTTP_FIELD_UPGRADE,			/* Upgrade field */
  HTTP_FIELD_USER_AGENT,		/* User-Agent field */
  HTTP_FIELD_VARY,			/* Vary field */
  HTTP_FIELD_VIA,			/* Via field */
  HTTP_FIELD_WWW_AUTHENTICATE,		/* WWW-Authenticate field */
  HTTP_FIELD_X_CONTENT_OPTIONS,		/* WHATWG X-Content-Options field */
  HTTP_FIELD_X_FRAME_OPTIONS,		/* WHATWG X-Frame-Options field */
  HTTP_FIELD_MAX			/* Maximum field index */
} http_field_t;

typedef enum http_keepalive_e		/**** HTTP keep-alive values ****/
{
  HTTP_KEEPALIVE_OFF = 0,		/* No keep alive support */
  HTTP_KEEPALIVE_ON			/* Use keep alive */
} http_keepalive_t;

typedef enum http_state_e		/**** HTTP state values; states are server-oriented... ****/
{
  HTTP_STATE_ERROR = -1,		/* Error on socket */
  HTTP_STATE_WAITING,			/* Waiting for command */
  HTTP_STATE_OPTIONS,			/* OPTIONS command, waiting for blank line */
  HTTP_STATE_GET,			/* GET command, waiting for blank line */
  HTTP_STATE_GET_SEND,			/* GET command, sending data */
  HTTP_STATE_HEAD,			/* HEAD command, waiting for blank line */
  HTTP_STATE_POST,			/* POST command, waiting for blank line */
  HTTP_STATE_POST_RECV,			/* POST command, receiving data */
  HTTP_STATE_POST_SEND,			/* POST command, sending data */
  HTTP_STATE_PUT,			/* PUT command, waiting for blank line */
  HTTP_STATE_PUT_RECV,			/* PUT command, receiving data */
  HTTP_STATE_DELETE,			/* DELETE command, waiting for blank line */
  HTTP_STATE_TRACE,			/* TRACE command, waiting for blank line */
  HTTP_STATE_CONNECT,			/* CONNECT command, waiting for blank line */
  HTTP_STATE_STATUS,			/* Command complete, sending status */
  HTTP_STATE_UNKNOWN_METHOD,		/* Unknown request method, waiting for blank line */
  HTTP_STATE_UNKNOWN_VERSION		/* Unknown request method, waiting for blank line */
} http_state_t;

typedef enum http_status_e		/**** HTTP status codes ****/
{
  HTTP_STATUS_ERROR = -1,		/* An error response from httpXxxx() */
  HTTP_STATUS_NONE = 0,			/* No Expect value */

  HTTP_STATUS_CONTINUE = 100,		/* Everything OK, keep going... */
  HTTP_STATUS_SWITCHING_PROTOCOLS,	/* HTTP upgrade to TLS/SSL */

  HTTP_STATUS_OK = 200,			/* OPTIONS/GET/HEAD/POST/TRACE command was successful */
  HTTP_STATUS_CREATED,			/* PUT command was successful */
  HTTP_STATUS_ACCEPTED,			/* DELETE command was successful */
  HTTP_STATUS_NOT_AUTHORITATIVE,	/* Information isn't authoritative */
  HTTP_STATUS_NO_CONTENT,		/* Successful command, no new data */
  HTTP_STATUS_RESET_CONTENT,		/* Content was reset/recreated */
  HTTP_STATUS_PARTIAL_CONTENT,		/* Only a partial file was received/sent */

  HTTP_STATUS_MULTIPLE_CHOICES = 300,	/* Multiple files match request */
  HTTP_STATUS_MOVED_PERMANENTLY,	/* Document has moved permanently */
  HTTP_STATUS_FOUND,			/* Document was found at a different URI */
  HTTP_STATUS_SEE_OTHER,		/* See this other link */
  HTTP_STATUS_NOT_MODIFIED,		/* File not modified */
  HTTP_STATUS_USE_PROXY,		/* Must use a proxy to access this URI */
  HTTP_STATUS_TEMPORARY_REDIRECT = 307,	/* Temporary redirection */

  HTTP_STATUS_BAD_REQUEST = 400,	/* Bad request */
  HTTP_STATUS_UNAUTHORIZED,		/* Unauthorized to access host */
  HTTP_STATUS_PAYMENT_REQUIRED,		/* Payment required */
  HTTP_STATUS_FORBIDDEN,		/* Forbidden to access this URI */
  HTTP_STATUS_NOT_FOUND,		/* URI was not found */
  HTTP_STATUS_METHOD_NOT_ALLOWED,	/* Method is not allowed */
  HTTP_STATUS_NOT_ACCEPTABLE,		/* Not Acceptable */
  HTTP_STATUS_PROXY_AUTHENTICATION,	/* Proxy Authentication is Required */
  HTTP_STATUS_REQUEST_TIMEOUT,		/* Request timed out */
  HTTP_STATUS_CONFLICT,			/* Request is self-conflicting */
  HTTP_STATUS_GONE,			/* Server has gone away */
  HTTP_STATUS_LENGTH_REQUIRED,		/* A content length or encoding is required */
  HTTP_STATUS_PRECONDITION,		/* Precondition failed */
  HTTP_STATUS_REQUEST_TOO_LARGE,	/* Request entity too large */
  HTTP_STATUS_URI_TOO_LONG,		/* URI too long */
  HTTP_STATUS_UNSUPPORTED_MEDIATYPE,	/* The requested media type is unsupported */
  HTTP_STATUS_REQUESTED_RANGE,		/* The requested range is not satisfiable */
  HTTP_STATUS_EXPECTATION_FAILED,	/* The expectation given in an Expect header field was not met */
  HTTP_STATUS_UPGRADE_REQUIRED = 426,	/* Upgrade to SSL/TLS required */

  HTTP_STATUS_SERVER_ERROR = 500,	/* Internal server error */
  HTTP_STATUS_NOT_IMPLEMENTED,		/* Feature not implemented */
  HTTP_STATUS_BAD_GATEWAY,		/* Bad gateway */
  HTTP_STATUS_SERVICE_UNAVAILABLE,	/* Service is unavailable */
  HTTP_STATUS_GATEWAY_TIMEOUT,		/* Gateway connection timed out */
  HTTP_STATUS_NOT_SUPPORTED,		/* HTTP version not supported */

  HTTP_STATUS_CUPS_AUTHORIZATION_CANCELED = 1000,
					/* User canceled authorization */
  HTTP_STATUS_CUPS_PKI_ERROR,		/* Error negotiating a secure connection */
  HTTP_STATUS_CUPS_WEBIF_DISABLED	/* Web interface is disabled @private@ */
} http_status_t;

typedef enum http_trust_e		/**** Level of trust for credentials */
{
  HTTP_TRUST_OK = 0,			/* Credentials are OK/trusted */
  HTTP_TRUST_INVALID,			/* Credentials are invalid */
  HTTP_TRUST_CHANGED,			/* Credentials have changed */
  HTTP_TRUST_EXPIRED,			/* Credentials are expired */
  HTTP_TRUST_RENEWED,			/* Credentials have been renewed */
  HTTP_TRUST_UNKNOWN			/* Credentials are unknown/new */
} http_trust_t;

typedef enum http_uri_status_e		/**** URI separation status ****/
{
  HTTP_URI_STATUS_OVERFLOW = -8,	/* URI buffer for httpAssembleURI is too small */
  HTTP_URI_STATUS_BAD_ARGUMENTS = -7,	/* Bad arguments to function (error) */
  HTTP_URI_STATUS_BAD_RESOURCE = -6,	/* Bad resource in URI (error) */
  HTTP_URI_STATUS_BAD_PORT = -5,	/* Bad port number in URI (error) */
  HTTP_URI_STATUS_BAD_HOSTNAME = -4,	/* Bad hostname in URI (error) */
  HTTP_URI_STATUS_BAD_USERNAME = -3,	/* Bad username in URI (error) */
  HTTP_URI_STATUS_BAD_SCHEME = -2,	/* Bad scheme in URI (error) */
  HTTP_URI_STATUS_BAD_URI = -1,		/* Bad/empty URI (error) */
  HTTP_URI_STATUS_OK = 0,		/* URI decoded OK */
  HTTP_URI_STATUS_MISSING_SCHEME,	/* Missing scheme in URI (warning) */
  HTTP_URI_STATUS_UNKNOWN_SCHEME,	/* Unknown scheme in URI (warning) */
  HTTP_URI_STATUS_MISSING_RESOURCE	/* Missing resource in URI (warning) */
} http_uri_status_t;

typedef enum http_uri_coding_e		/**** URI en/decode flags ****/
{
  HTTP_URI_CODING_NONE = 0,		/* Don't en/decode anything */
  HTTP_URI_CODING_USERNAME = 1,		/* En/decode the username portion */
  HTTP_URI_CODING_HOSTNAME = 2,		/* En/decode the hostname portion */
  HTTP_URI_CODING_RESOURCE = 4,		/* En/decode the resource portion */
  HTTP_URI_CODING_MOST = 7,		/* En/decode all but the query */
  HTTP_URI_CODING_QUERY = 8,		/* En/decode the query portion */
  HTTP_URI_CODING_ALL = 15,		/* En/decode everything */
  HTTP_URI_CODING_RFC6874 = 16		/* Use RFC 6874 address format */
} http_uri_coding_t;

typedef enum http_version_e		/**** HTTP version numbers @exclude all@ ****/
{
  HTTP_VERSION_0_9 = 9,			/* HTTP/0.9 */
  HTTP_VERSION_1_0 = 100,		/* HTTP/1.0 */
  HTTP_VERSION_1_1 = 101		/* HTTP/1.1 */
} http_version_t;

typedef union _http_addr_u		/* Socket address union */
{
  struct sockaddr	addr;		/* Base structure for family value */
  struct sockaddr_in	ipv4;		/* IPv4 address */
#ifdef AF_INET6
  struct sockaddr_in6	ipv6;		/* IPv6 address */
#endif /* AF_INET6 */
#ifdef AF_LOCAL
  struct sockaddr_un	un;		/* Domain socket file */
#endif /* AF_LOCAL */
  char			pad[256];	/* Padding to ensure binary compatibility @private@ */
} http_addr_t;

typedef struct http_addrlist_s		/**** Socket address list, which is used to enumerate all of the addresses that are associated with a hostname. ****/
{
  struct http_addrlist_s *next;		/* Pointer to next address in list */
  http_addr_t		addr;		/* Address */
} http_addrlist_t;

typedef struct _http_s http_t;		/**** HTTP connection type ****/

typedef struct http_credential_s	/**** HTTP credential data @exclude all@ ****/
{
  void		*data;			/* Pointer to credential data */
  size_t	datalen;		/* Credential length */
} http_credential_t;

typedef bool (*http_timeout_cb_t)(http_t *http, void *user_data);
					/**** HTTP timeout callback ****/


/*
 * Prototypes...
 */

extern http_t		*httpAcceptConnection(int fd, int blocking) _CUPS_PUBLIC;
extern int		httpAddCredential(cups_array_t *credentials, const void *data, size_t datalen) _CUPS_PUBLIC;
extern int		httpAddrAny(const http_addr_t *addr) _CUPS_PUBLIC;
extern int		httpAddrClose(http_addr_t *addr, int fd) _CUPS_PUBLIC;
extern http_addrlist_t	*httpAddrConnect(http_addrlist_t *addrlist, int *sock, int msec, int *cancel) _CUPS_PUBLIC;
extern http_addrlist_t	*httpAddrCopyList(http_addrlist_t *src) _CUPS_PUBLIC;
extern int		httpAddrEqual(const http_addr_t *addr1, const http_addr_t *addr2) _CUPS_PUBLIC;
extern int		httpAddrFamily(http_addr_t *addr) _CUPS_PUBLIC;
extern void		httpAddrFreeList(http_addrlist_t *addrlist) _CUPS_PUBLIC;
extern http_addrlist_t	*httpAddrGetList(const char *hostname, int family, const char *service) _CUPS_PUBLIC;
extern int		httpAddrLength(const http_addr_t *addr) _CUPS_PUBLIC;
extern int		httpAddrListen(http_addr_t *addr, int port) _CUPS_PUBLIC;
extern int		httpAddrLocalhost(const http_addr_t *addr) _CUPS_PUBLIC;
extern char		*httpAddrLookup(const http_addr_t *addr, char *name, int namelen) _CUPS_PUBLIC;
extern int		httpAddrPort(http_addr_t *addr) _CUPS_PUBLIC;
extern char		*httpAddrString(const http_addr_t *addr, char *s, int slen) _CUPS_PUBLIC;
extern http_uri_status_t httpAssembleURI(http_uri_coding_t encoding, char *uri, int urilen, const char *scheme, const char *username, const char *host, int port, const char *resource) _CUPS_PUBLIC;
extern http_uri_status_t httpAssembleURIf(http_uri_coding_t encoding, char *uri, int urilen, const char *scheme, const char *username, const char *host, int port, const char *resourcef, ...) _CUPS_FORMAT(8, 9) _CUPS_PUBLIC;
extern char		*httpAssembleUUID(const char *server, int port, const char *name, int number, char *buffer, size_t bufsize) _CUPS_PUBLIC;
extern int		httpCheck(http_t *http) _CUPS_PUBLIC;
extern void		httpClearFields(http_t *http) _CUPS_PUBLIC;
extern void		httpClose(http_t *http) _CUPS_PUBLIC;
extern void		httpClearCookie(http_t *http) _CUPS_PUBLIC;
extern int		httpCompareCredentials(cups_array_t *cred1, cups_array_t *cred2) _CUPS_PUBLIC;
extern http_t		*httpConnect(const char *host, int port, http_addrlist_t *addrlist, int family, http_encryption_t encryption, bool blocking, int msec, int *cancel) _CUPS_PUBLIC;
extern int		httpCopyCredentials(http_t *http, cups_array_t **credentials) _CUPS_PUBLIC;
extern int		httpCredentialsAreValidForName(cups_array_t *credentials, const char *common_name);
extern time_t		httpCredentialsGetExpiration(cups_array_t *credentials) _CUPS_PUBLIC;
extern http_trust_t	httpCredentialsGetTrust(cups_array_t *credentials, const char *common_name) _CUPS_PUBLIC;
extern size_t		httpCredentialsString(cups_array_t *credentials, char *buffer, size_t bufsize) _CUPS_PUBLIC;
extern char		*httpDecode64(char *out, size_t *outlen, const char *in) _CUPS_PUBLIC;
extern int		httpDelete(http_t *http, const char *uri) _CUPS_PUBLIC;
extern char		*httpEncode64(char *out, size_t outlen, const char *in, size_t inlen) _CUPS_PUBLIC;
extern int		httpEncryption(http_t *http, http_encryption_t e) _CUPS_PUBLIC;
extern int		httpError(http_t *http) _CUPS_PUBLIC;
extern http_field_t	httpFieldValue(const char *name) _CUPS_PUBLIC;
extern void		httpFlush(http_t *http) _CUPS_PUBLIC;
extern int		httpFlushWrite(http_t *http) _CUPS_PUBLIC;
extern void		httpFreeCredentials(cups_array_t *certs) _CUPS_PUBLIC;
extern int		httpGet(http_t *http, const char *uri) _CUPS_PUBLIC;
extern time_t		httpGetActivity(http_t *http) _CUPS_PUBLIC;
extern http_addr_t	*httpGetAddress(http_t *http) _CUPS_PUBLIC;
extern char		*httpGetAuthString(http_t *http) _CUPS_PUBLIC;
extern bool		httpGetBlocking(http_t *http) _CUPS_PUBLIC;
extern const char	*httpGetContentEncoding(http_t *http) _CUPS_PUBLIC;
extern const char	*httpGetCookie(http_t *http) _CUPS_PUBLIC;
extern const char	*httpGetDateString(time_t t, char *s, size_t slen) _CUPS_PUBLIC;
extern time_t		httpGetDateTime(const char *s) _CUPS_PUBLIC;
extern http_encryption_t httpGetEncryption(http_t *http) _CUPS_PUBLIC;
extern http_status_t	httpGetExpect(http_t *http) _CUPS_PUBLIC;
extern int		httpGetFd(http_t *http) _CUPS_PUBLIC;
extern const char	*httpGetField(http_t *http, http_field_t field) _CUPS_PUBLIC;
extern struct hostent	*httpGetHostByName(const char *name) _CUPS_PUBLIC;
extern const char	*httpGetHostname(http_t *http, char *s, int slen) _CUPS_PUBLIC;
extern http_keepalive_t	httpGetKeepAlive(http_t *http) _CUPS_PUBLIC;
extern off_t		httpGetLength(http_t *http) _CUPS_PUBLIC;
extern size_t		httpGetPending(http_t *http) _CUPS_PUBLIC;
extern size_t		httpGetReady(http_t *http) _CUPS_PUBLIC;
extern size_t		httpGetRemaining(http_t *http) _CUPS_PUBLIC;
extern http_state_t	httpGetState(http_t *http) _CUPS_PUBLIC;
extern http_status_t	httpGetStatus(http_t *http) _CUPS_PUBLIC;
extern char		*httpGetSubField(http_t *http, http_field_t field, const char *name, char *value, size_t valuelen) _CUPS_PUBLIC;
extern http_version_t	httpGetVersion(http_t *http) _CUPS_PUBLIC;
extern char		*httpGets(http_t *http, char *line, size_t length) _CUPS_PUBLIC;
extern int		httpHead(http_t *http, const char *uri) _CUPS_PUBLIC;
extern void		httpInitialize(void) _CUPS_PUBLIC;
extern int		httpIsChunked(http_t *http) _CUPS_PUBLIC;
extern int		httpIsEncrypted(http_t *http) _CUPS_PUBLIC;
extern int		httpLoadCredentials(const char *path, cups_array_t **credentials, const char *common_name) _CUPS_PUBLIC;
extern int		httpOptions(http_t *http, const char *uri) _CUPS_PUBLIC;
extern ssize_t		httpPeek(http_t *http, char *buffer, size_t length) _CUPS_PUBLIC;
extern int		httpPost(http_t *http, const char *uri) _CUPS_PUBLIC;
extern int		httpPrintf(http_t *http, const char *format, ...) _CUPS_FORMAT(2, 3) _CUPS_PUBLIC;
extern int		httpPut(http_t *http, const char *uri) _CUPS_PUBLIC;
extern ssize_t		httpRead(http_t *http, char *buffer, size_t length) _CUPS_PUBLIC;
extern http_state_t	httpReadRequest(http_t *http, char *resource, size_t resourcelen) _CUPS_PUBLIC;
extern int		httpReconnect(http_t *http, int msec, int *cancel) _CUPS_PUBLIC;
extern const char	*httpResolveHostname(http_t *http, char *buffer, size_t bufsize) _CUPS_PUBLIC;
extern int		httpSaveCredentials(const char *path, cups_array_t *credentials, const char *common_name) _CUPS_PUBLIC;
extern http_uri_status_t httpSeparateURI(http_uri_coding_t decoding, const char *uri, char *scheme, int schemelen, char *username, int usernamelen, char *host, int hostlen, int *port, char *resource, int resourcelen) _CUPS_PUBLIC;
extern void		httpSetAuthString(http_t *http, const char *scheme, const char *data) _CUPS_PUBLIC;
extern void		httpSetBlocking(http_t *http, bool b) _CUPS_PUBLIC;
extern void		httpSetCookie(http_t *http, const char *cookie) _CUPS_PUBLIC;
extern int		httpSetCredentials(http_t *http, cups_array_t *certs) _CUPS_PUBLIC;
extern void		httpSetDefaultField(http_t *http, http_field_t field, const char *value) _CUPS_PUBLIC;
extern void		httpSetExpect(http_t *http, http_status_t expect) _CUPS_PUBLIC;
extern void		httpSetField(http_t *http, http_field_t field, const char *value) _CUPS_PUBLIC;
extern void		httpSetKeepAlive(http_t *http, http_keepalive_t keep_alive) _CUPS_PUBLIC;
extern void		httpSetLength(http_t *http, size_t length) _CUPS_PUBLIC;
extern void		httpSetTimeout(http_t *http, double timeout, http_timeout_cb_t cb, void *user_data) _CUPS_PUBLIC;
extern void		httpShutdown(http_t *http) _CUPS_PUBLIC;
extern const char	*httpStateString(http_state_t state) _CUPS_PUBLIC;
extern const char	*httpStatus(http_status_t status) _CUPS_PUBLIC;
extern int		httpTrace(http_t *http, const char *uri) _CUPS_PUBLIC;
extern http_status_t	httpUpdate(http_t *http) _CUPS_PUBLIC;
extern const char	*httpURIStatusString(http_uri_status_t status) _CUPS_PUBLIC;
extern int		httpWait(http_t *http, int msec) _CUPS_PUBLIC;
extern ssize_t		httpWrite(http_t *http, const char *buffer, size_t length) _CUPS_PUBLIC;
extern http_state_t	httpWriteResponse(http_t *http, http_status_t status) _CUPS_PUBLIC;


#  ifdef __cplusplus
}
#  endif /* __cplusplus */
#endif /* !_CUPS_HTTP_H_ */
