//
// HTTP routines for CUPS.
//
// Copyright © 2021-2023 by OpenPrinting.
// Copyright © 2007-2021 by Apple Inc.
// Copyright © 1997-2007 by Easy Software Products, all rights reserved.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#include "cups-private.h"
#include "debug-internal.h"
#include <fcntl.h>
#include <math.h>
#ifdef _WIN32
#  include <tchar.h>
#else
#  include <poll.h>
#  include <signal.h>
#  include <sys/time.h>
#  include <sys/resource.h>
#endif // _WIN32
#include <zlib.h>


//
// Local functions...
//

static void		http_add_field(http_t *http, http_field_t field, const char *value, bool append);
static void		http_content_coding_finish(http_t *http);
static void		http_content_coding_start(http_t *http, const char *value);
static http_t		*http_create(const char *host, int port, http_addrlist_t *addrlist, int family, http_encryption_t encryption, bool blocking, _http_mode_t mode);
#ifdef DEBUG
static void		http_debug_hex(const char *prefix, const char *buffer, int bytes);
#endif // DEBUG
static void		http_free_credential(http_credential_t *c);
static ssize_t		http_read(http_t *http, char *buffer, size_t length);
static ssize_t		http_read_buffered(http_t *http, char *buffer, size_t length);
static ssize_t		http_read_chunk(http_t *http, char *buffer, size_t length);
static bool		http_send(http_t *http, http_state_t request, const char *uri);
static ssize_t		http_write(http_t *http, const char *buffer, size_t length);
static ssize_t		http_write_chunk(http_t *http, const char *buffer, size_t length);
static off_t		http_set_length(http_t *http);
static void		http_set_timeout(int fd, double timeout);
static void		http_set_wait(http_t *http);

#ifdef HAVE_TLS
static bool		http_tls_upgrade(http_t *http);
#endif // HAVE_TLS


//
// Local globals...
//

static const char * const http_fields[HTTP_FIELD_MAX] =
{
  "Accept",
  "Accept-CH",
  "Accept-Encoding",
  "Accept-Language",
  "Accept-Ranges",
  "Access-Control-Allow-Credentials",
  "Access-Control-Allow-Headers",
  "Access-Control-Allow-Methods",
  "Access-Control-Allow-Origin",
  "Access-Control-Expose-Headers",
  "Access-Control-Max-Age",
  "Access-Control-Request-Headers",
  "Access-Control-Request-Method",
  "Age",
  "Allow",
  "Authentication-Control",
  "Authentication-Info",
  "Authorization",
  "Cache-Control",
  "Cache-Status",
  "Cert-Not-After",
  "Cert-Not-Before",
  "Connection",
  "Content-Disposition",
  "Content-Encoding",
  "Content-Language",
  "Content-Length",
  "Content-Location",
  "Content-Range",
  "Content-Security-Policy",
  "Content-Security-Policy-Report-Only",
  "Content-Type",
  "Cross-Origin-Embedder-Policy",
  "Cross-Origin-Embedder-Policy-Report-Only",
  "Cross-Origin-Opener-Policy",
  "Cross-Origin-Opener-Policy-Report-Only",
  "Cross-Origin-Resource-Policy",
  "DASL",
  "Date",
  "DAV",
  "Depth",
  "Destination",
  "ETag",
  "Expires",
  "Forwarded",
  "From",
  "Host",
  "If",
  "If-Match",
  "If-Modified-Since",
  "If-None-Match",
  "If-Range",
  "If-Schedule-Tag-Match",
  "If-Unmodified-since",
  "Keep-Alive",
  "Last-Modified",
  "Link",
  "Location",
  "Lock-Token",
  "Max-Forwards",
  "Optional-WWW-Authenticate",
  "Origin",
  "OSCORE",
  "Overwrite",
  "Pragma",
  "Proxy-Authenticate",
  "Proxy-Authentication-Info",
  "Proxy-Authorization",
  "Proxy-Status",
  "Public",
  "Range",
  "Referer",
  "Refresh",
  "Replay-Nonce",
  "Retry-After",
  "Schedule-Reply",
  "Schedule-Tag",
  "Server",
  "Strict-Transport-Security",
  "TE",
  "Timeout",
  "Trailer",
  "Transfer-Encoding",
  "Upgrade",
  "User-Agent",
  "Vary",
  "Via",
  "WWW-Authenticate",
  "X-Content-Options",
  "X-Frame-Options"
};


//
// 'httpAcceptConnection()' - Accept a new HTTP client connection.
//
// This function accepts a new HTTP client connection from the specified
// listening socket "fd".  The "blocking" argument specifies whether the new
// HTTP connection is blocking.
//

http_t *				// O - HTTP connection or `NULL`
httpAcceptConnection(int  fd,		// I - Listen socket file descriptor
                     bool blocking)	// I - `true` if the connection should be blocking, `false` otherwise
{
  http_t		*http;		// HTTP connection
  http_addrlist_t	addrlist;	// Dummy address list
  socklen_t		addrlen;	// Length of address
  int			val;		// Socket option value


  // Range check input...
  if (fd < 0)
    return (NULL);

  // Create the client connection...
  memset(&addrlist, 0, sizeof(addrlist));

  if ((http = http_create(NULL, 0, &addrlist, AF_UNSPEC, HTTP_ENCRYPTION_IF_REQUESTED, blocking, _HTTP_MODE_SERVER)) == NULL)
    return (NULL);

  // Accept the client and get the remote address...
  addrlen = sizeof(http_addr_t);

  if ((http->fd = accept(fd, (struct sockaddr *)&(http->hostlist->addr), &addrlen)) < 0)
  {
    _cupsSetHTTPError(HTTP_STATUS_ERROR);
    httpClose(http);

    return (NULL);
  }

  http->hostaddr = &(http->hostlist->addr);

  if (httpAddrIsLocalhost(http->hostaddr))
    cupsCopyString(http->hostname, "localhost", sizeof(http->hostname));
  else
    httpAddrGetString(http->hostaddr, http->hostname, sizeof(http->hostname));

#ifdef SO_NOSIGPIPE
  // Disable SIGPIPE for this socket.
  val = 1;
  if (setsockopt(http->fd, SOL_SOCKET, SO_NOSIGPIPE, CUPS_SOCAST &val, sizeof(val)))
    DEBUG_printf("httpAcceptConnection: setsockopt(SO_NOSIGPIPE) failed - %s", strerror(errno));
#endif // SO_NOSIGPIPE

  // Using TCP_NODELAY improves responsiveness, especially on systems with a
  // slow loopback interface.  Since we write large buffers when sending print
  // files and requests, there shouldn't be any performance penalty for this...
  val = 1;
  if (setsockopt(http->fd, IPPROTO_TCP, TCP_NODELAY, CUPS_SOCAST &val, sizeof(val)))
    DEBUG_printf("httpAcceptConnection: setsockopt(TCP_NODELAY) failed - %s", strerror(errno));

#ifdef FD_CLOEXEC
  // Close this socket when starting another process...
  if (fcntl(http->fd, F_SETFD, FD_CLOEXEC))
    DEBUG_printf("httpAcceptConnection: fcntl(F_SETFD, FD_CLOEXEC) failed - %s", strerror(errno));
#endif // FD_CLOEXEC

  return (http);
}


//
// 'httpAddCredential()' - Allocates and adds a single credential to an array.
//
// Use @code httpCreateCredentials@ to create a credentials array.
//

bool					// O - `true` on success, `false` on error
httpAddCredential(
    cups_array_t *credentials,		// I - Credentials array
    const void   *data,			// I - PEM-encoded X.509 data
    size_t       datalen)		// I - Length of data
{
  http_credential_t	*credential;	// Credential data


  if (!credentials)
    return (false);

  if ((credential = malloc(sizeof(http_credential_t))) != NULL)
  {
    credential->datalen = datalen;

    if ((credential->data = malloc(datalen)) != NULL)
    {
      memcpy(credential->data, data, datalen);
      cupsArrayAdd(credentials, credential);
      return (true);
    }

    free(credential);
  }

  return (false);
}


//
// 'httpClearCookie()' - Clear the cookie value(s).
//

void
httpClearCookie(http_t *http)		// I - HTTP connection
{
  if (!http)
    return;

  if (http->cookie)
  {
    free(http->cookie);
    http->cookie = NULL;
  }
}


//
// 'httpClearFields()' - Clear HTTP request/response fields.
//

void
httpClearFields(http_t *http)		// I - HTTP connection
{
  http_field_t	field;			// Current field


  DEBUG_printf("httpClearFields(http=%p)", (void *)http);

  if (http)
  {
    for (field = HTTP_FIELD_ACCEPT; field < HTTP_FIELD_MAX; field ++)
    {
      free(http->fields[field]);
      http->fields[field] = NULL;
    }

    if (http->mode == _HTTP_MODE_CLIENT)
    {
      if (http->hostname[0] == '/')
	httpSetField(http, HTTP_FIELD_HOST, "localhost");
      else
	httpSetField(http, HTTP_FIELD_HOST, http->hostname);
    }

    http->expect = (http_status_t)0;
  }
}


//
// 'httpClose()' - Close a HTTP connection.
//

void
httpClose(http_t *http)			// I - HTTP connection
{
  DEBUG_printf("httpClose(http=%p)", (void *)http);

  // Range check input...
  if (!http)
    return;

  // Close any open connection...
  _httpDisconnect(http);

  // Free memory used...
  httpAddrFreeList(http->hostlist);

  httpClearFields(http);

  free(http->fields[HTTP_FIELD_HOST]);
  free(http->authstring);
  free(http->cookie);

  free(http);
}


//
// 'httpCompareCredentials()' - Compare two sets of X.509 credentials.
//

bool					// O - `true` if they match, `false` if they do not
httpCompareCredentials(
    cups_array_t *cred1,		// I - First set of X.509 credentials
    cups_array_t *cred2)		// I - Second set of X.509 credentials
{
  http_credential_t	*temp1, *temp2;	// Temporary credentials


  for (temp1 = (http_credential_t *)cupsArrayGetFirst(cred1), temp2 = (http_credential_t *)cupsArrayGetFirst(cred2); temp1 && temp2; temp1 = (http_credential_t *)cupsArrayGetNext(cred1), temp2 = (http_credential_t *)cupsArrayGetNext(cred2))
  {
    if (temp1->datalen != temp2->datalen)
      return (false);
    else if (memcmp(temp1->data, temp2->data, temp1->datalen))
      return (false);
  }

  return (temp1 == temp2);
}


//
// 'httpConnect()' - Connect to a HTTP server.
//
// This function creates a connection to a HTTP server.  The "host" and "port"
// arguments specify a hostname or IP address and port number to use while the
// "addrlist" argument specifies a list of addresses to use or `NULL` to do a
// fresh lookup.  The "family" argument specifies the address family to use -
// `AF_UNSPEC` to try both IPv4 and IPv6, `AF_INET` for IPv4, or `AF_INET6` for
// IPv6.
//
// The "encryption" argument specifies whether to encrypt the connection and the
// "blocking" argument specifies whether to use blocking behavior when reading
// or writing data.
//
// The "msec" argument specifies how long to try to connect to the server or `0`
// to just create an unconnected `http_t` object.  The "cancel" argument
// specifies an integer variable that can be set to a non-zero value to cancel
// the connection process.
//

http_t *				// O - New HTTP connection
httpConnect(
    const char        *host,		// I - Host to connect to
    int               port,		// I - Port number
    http_addrlist_t   *addrlist,	// I - List of addresses or `NULL` to lookup
    int               family,		// I - Address family to use or `AF_UNSPEC` for any
    http_encryption_t encryption,	// I - Type of encryption to use
    bool              blocking,		// I - `true` for blocking connection, `false` for non-blocking
    int               msec,		// I - Connection timeout in milliseconds, 0 means don't connect
    int               *cancel)		// I - Pointer to "cancel" variable
{
  http_t	*http;			// New HTTP connection


  DEBUG_printf("httpConnect(host=\"%s\", port=%d, addrlist=%p, family=%d, encryption=%d, blocking=%d, msec=%d, cancel=%p)", host, port, (void *)addrlist, family, encryption, blocking, msec, (void *)cancel);

  // Create the HTTP structure...
  if ((http = http_create(host, port, addrlist, family, encryption, blocking, _HTTP_MODE_CLIENT)) == NULL)
    return (NULL);

  // Optionally connect to the remote system...
  if (msec == 0 || httpReconnect(http, msec, cancel))
    return (http);

  // Could not connect to any known address - bail out!
  httpClose(http);

  return (NULL);
}


//
// 'httpCreateCredentials()' - Create a new array of HTTP credentials.
//
// This function creates a new array of HTTP credentials for use with the
// @link httpAddCredential@ and @link httpSetCredentials@ functions.
//

cups_array_t *				// O - Array
httpCreateCredentials(
    const void   *data,			// I - PEM-encoded X.509 data
    size_t       datalen)		// I - Length of data
{
  cups_array_t	*a = cupsArrayNew(NULL, NULL, NULL, 0, NULL, (cups_afree_cb_t)http_free_credential);


  if (data && datalen > 0 && !httpAddCredential(a, data, datalen))
  {
    cupsArrayDelete(a);
    return (NULL);
  }

  return (a);
}


//
// '_httpDisconnect()' - Disconnect a HTTP connection.
//

void
_httpDisconnect(http_t *http)		// I - HTTP connection
{
#ifdef HAVE_TLS
  if (http->tls)
    _httpTLSStop(http);
#endif // HAVE_TLS

  httpAddrClose(NULL, http->fd);

  http->fd = -1;
}


//
// 'httpFieldValue()' - Return the HTTP field enumeration value for a field
//                      name.
//

http_field_t				// O - Field index
httpFieldValue(const char *name)	// I - String name
{
  int	i;				// Looping var


  for (i = 0; i < HTTP_FIELD_MAX; i ++)
    if (!_cups_strcasecmp(name, http_fields[i]))
      return ((http_field_t)i);

  return (HTTP_FIELD_UNKNOWN);
}


//
// 'httpFlush()' - Flush data read from a HTTP connection.
//

void
httpFlush(http_t *http)			// I - HTTP connection
{
  char		buffer[8192];		// Junk buffer
  int		blocking;		// To block or not to block
  http_state_t	oldstate;		// Old state


  DEBUG_printf("httpFlush(http=%p), state=%s", (void *)http, httpStateString(http->state));

 /*
  * Nothing to do if we are in the "waiting" state...
  */

  if (http->state == HTTP_STATE_WAITING)
    return;

 /*
  * Temporarily set non-blocking mode so we don't get stuck in httpRead()...
  */

  blocking = http->blocking;
  http->blocking = 0;

 /*
  * Read any data we can...
  */

  oldstate = http->state;
  while (httpRead(http, buffer, sizeof(buffer)) > 0);

 /*
  * Restore blocking and reset the connection if we didn't get all of
  * the remaining data...
  */

  http->blocking = blocking;

  if (http->state == oldstate && http->state != HTTP_STATE_WAITING &&
      http->fd >= 0)
  {
   /*
    * Didn't get the data back, so close the current connection.
    */

    if (http->coding)
      http_content_coding_finish(http);

    DEBUG_puts("1httpFlush: Setting state to HTTP_STATE_WAITING and closing.");

    http->state = HTTP_STATE_WAITING;

#ifdef HAVE_TLS
    if (http->tls)
      _httpTLSStop(http);
#endif // HAVE_TLS

    httpAddrClose(NULL, http->fd);

    http->fd = -1;
  }
}


//
// 'httpFlushWrite()' - Flush data written to a HTTP connection.
//
//
//

int					// O - Bytes written or -1 on error
httpFlushWrite(http_t *http)		// I - HTTP connection
{
  ssize_t	bytes;			// Bytes written


  DEBUG_printf("httpFlushWrite(http=%p) data_encoding=%d", (void *)http, http ? http->data_encoding : 100);

  if (!http || !http->wused)
  {
    DEBUG_puts(http ? "1httpFlushWrite: Write buffer is empty." :
                      "1httpFlushWrite: No connection.");
    return (0);
  }

  if (http->data_encoding == HTTP_ENCODING_CHUNKED)
    bytes = http_write_chunk(http, http->wbuffer, (size_t)http->wused);
  else
    bytes = http_write(http, http->wbuffer, (size_t)http->wused);

  http->wused = 0;

  DEBUG_printf("1httpFlushWrite: Returning %d, errno=%d.", (int)bytes, errno);

  return ((int)bytes);
}


//
// 'httpFreeCredentials()' - Free an array of credentials.
//

void
httpFreeCredentials(
    cups_array_t *credentials)		// I - Array of credentials
{
  cupsArrayDelete(credentials);
}


//
// 'httpGetActivity()' - Get the most recent activity for a connection.
//
// The return value is the time in seconds of the last read or write.
//

time_t					// O - Time of last read or write
httpGetActivity(http_t *http)		// I - HTTP connection
{
  return (http ? http->activity : 0);
}


//
// 'httpGetAuthString()' - Get the current authorization string.
//
// The authorization string is set by @link cupsDoAuthentication@ and
// @link httpSetAuthString@.  Use @link httpGetAuthString@ to retrieve the
// string to use with @link httpSetField@ for the
// `HTTP_FIELD_AUTHORIZATION` value.
//
//
//

char *					// O - Authorization string
httpGetAuthString(http_t *http)		// I - HTTP connection
{
  if (http)
    return (http->authstring);
  else
    return (NULL);
}


//
// 'httpGetBlocking()' - Get the blocking/non-blocking state of a connection.
//
//
//

bool					// O - `true` if blocking, `false` if non-blocking
httpGetBlocking(http_t *http)		// I - HTTP connection
{
  return (http ? http->blocking : false);
}


//
// 'httpGetContentEncoding()' - Get a common content encoding, if any, between
//                              the client and server.
//
// This function uses the value of the Accepts-Encoding HTTP header and must be
// called after receiving a response from the server or a request from the
// client.  The value returned can be use in subsequent requests (for clients)
// or in the response (for servers) in order to compress the content stream.
//
//
//

const char *				/* O - Content-Coding value or
					       `NULL` for the identity
					       coding. */
httpGetContentEncoding(http_t *http)	// I - HTTP connection
{
  if (http && http->fields[HTTP_FIELD_ACCEPT_ENCODING])
  {
    int		i;			// Looping var
    char	temp[HTTP_MAX_VALUE],	// Copy of Accepts-Encoding value
		*start,			// Start of coding value
		*end;			// End of coding value
    double	qvalue;			// "qvalue" for coding
    struct lconv *loc = localeconv();	// Locale data
    static const char * const codings[] =
    {					// Supported content codings
      "deflate",
      "gzip",
      "x-deflate",
      "x-gzip"
    };

    cupsCopyString(temp, http->fields[HTTP_FIELD_ACCEPT_ENCODING], sizeof(temp));

    for (start = temp; *start; start = end)
    {
     /*
      * Find the end of the coding name...
      */

      qvalue = 1.0;
      end    = start;
      while (*end && *end != ';' && *end != ',' && !isspace(*end & 255))
        end ++;

      if (*end == ';')
      {
       /*
        * Grab the qvalue as needed...
        */

        if (!strncmp(end, ";q=", 3))
          qvalue = _cupsStrScand(end + 3, NULL, loc);

       /*
        * Skip past all attributes...
        */

        *end++ = '\0';
        while (*end && *end != ',' && !isspace(*end & 255))
          end ++;
      }
      else if (*end)
        *end++ = '\0';

      while (*end && isspace(*end & 255))
	end ++;

     /*
      * Check value if it matches something we support...
      */

      if (qvalue <= 0.0)
        continue;

      for (i = 0; i < (int)(sizeof(codings) / sizeof(codings[0])); i ++)
        if (!strcmp(start, codings[i]))
          return (codings[i]);
    }
  }

  return (NULL);
}


//
// 'httpGetCookie()' - Get any cookie data from the response.
//
//
//

const char *				// O - Cookie data or `NULL`
httpGetCookie(http_t *http)		// I - HTTP connection
{
  return (http ? http->cookie : NULL);
}


//
// 'httpGetEncryption()' - Get the current encryption mode of a connection.
//
// This function returns the encryption mode for the connection. Use the
// @link httpIsEncrypted@ function to determine whether a TLS session has
// been established.
//

http_encryption_t			// O - Current encryption mode
httpGetEncryption(http_t *http)		// I - HTTP connection
{
  return (http ? http->encryption : HTTP_ENCRYPTION_IF_REQUESTED);
}


//
// 'httpGetError()' - Get the last error on a connection.
//

int					// O - Error code (errno) value
httpGetError(http_t *http)		// I - HTTP connection
{
  if (http)
    return (http->error);
  else
    return (EINVAL);
}


//
// 'httpGetExpect()' - Get the value of the Expect header, if any.
//
// Returns `HTTP_STATUS_NONE` if there is no Expect header, otherwise
// returns the expected HTTP status code, typically `HTTP_STATUS_CONTINUE`.
//
//
//

http_status_t				// O - Expect: status, if any
httpGetExpect(http_t *http)		// I - HTTP connection
{
  if (!http)
    return (HTTP_STATUS_ERROR);
  else
    return (http->expect);
}


//
// 'httpGetFd()' - Get the file descriptor associated with a connection.
//
//
//

int					// O - File descriptor or -1 if none
httpGetFd(http_t *http)			// I - HTTP connection
{
  return (http ? http->fd : -1);
}


//
// 'httpGetField()' - Get a field value from a request/response.
//

const char *				// O - Field value
httpGetField(http_t       *http,	// I - HTTP connection
             http_field_t field)	// I - Field to get
{
  if (!http || field <= HTTP_FIELD_UNKNOWN || field >= HTTP_FIELD_MAX)
    return (NULL);
  else if (http->fields[field])
    return (http->fields[field]);
  else
    return ("");
}


//
// 'httpGetKeepAlive()' - Get the current Keep-Alive state of the connection.
//

http_keepalive_t			// O - Keep-Alive state
httpGetKeepAlive(http_t *http)		// I - HTTP connection
{
  return (http ? http->keep_alive : HTTP_KEEPALIVE_OFF);
}


//
// 'httpGetLength()' - Get the amount of data remaining from the content-length
//                     or transfer-encoding fields.
//
// This function returns the complete content length, even for content larger
// than 2^31 - 1.
//
//
//

off_t					// O - Content length
httpGetLength(http_t *http)		// I - HTTP connection
{
  off_t			remaining;	// Remaining length


  DEBUG_printf("2httpGetLength(http=%p), state=%s", (void *)http, http ? httpStateString(http->state) : "NONE");

  if (!http)
    return (-1);

  if (http->fields[HTTP_FIELD_TRANSFER_ENCODING] && !_cups_strcasecmp(http->fields[HTTP_FIELD_TRANSFER_ENCODING], "chunked"))
  {
    DEBUG_puts("4httpGetLength: chunked request!");
    remaining = 0;
  }
  else
  {
   /*
    * The following is a hack for HTTP servers that don't send a
    * Content-Length or Transfer-Encoding field...
    *
    * If there is no Content-Length then the connection must close
    * after the transfer is complete...
    */

    if (!http->fields[HTTP_FIELD_CONTENT_LENGTH] || !http->fields[HTTP_FIELD_CONTENT_LENGTH][0])
    {
     /*
      * Default content length is 0 for errors and certain types of operations,
      * and 2^31-1 for other successful requests...
      */

      if (http->status >= HTTP_STATUS_MULTIPLE_CHOICES ||
          http->state == HTTP_STATE_OPTIONS ||
          (http->state == HTTP_STATE_GET && http->mode == _HTTP_MODE_SERVER) ||
          http->state == HTTP_STATE_HEAD ||
          (http->state == HTTP_STATE_PUT && http->mode == _HTTP_MODE_CLIENT) ||
          http->state == HTTP_STATE_DELETE ||
          http->state == HTTP_STATE_TRACE ||
          http->state == HTTP_STATE_CONNECT)
        remaining = 0;
      else
        remaining = 2147483647;
    }
    else if ((remaining = strtoll(http->fields[HTTP_FIELD_CONTENT_LENGTH],
			          NULL, 10)) < 0)
      remaining = -1;

    DEBUG_printf("4httpGetLength: content_length=" CUPS_LLFMT, CUPS_LLCAST remaining);
  }

  return (remaining);
}


//
// 'httpGetPending()' - Get the number of bytes that are buffered for writing.
//

size_t					// O - Number of bytes buffered
httpGetPending(http_t *http)		// I - HTTP connection
{
  return (http ? (size_t)http->wused : 0);
}


//
// 'httpGetReady()' - Get the number of bytes that can be read without blocking.
//

size_t					// O - Number of bytes available
httpGetReady(http_t *http)		// I - HTTP connection
{
  if (!http)
    return (0);
  else if (http->used > 0)
    return ((size_t)http->used);
#ifdef HAVE_TLS
  else if (http->tls)
    return (_httpTLSPending(http));
#endif // HAVE_TLS

  return (0);
}


//
// 'httpGetRemaining()' - Get the number of remaining bytes in the message
//                        body or current chunk.
//
// The @link httpIsChunked@ function can be used to determine whether the
// message body is chunked or fixed-length.
//

size_t					// O - Remaining bytes
httpGetRemaining(http_t *http)		// I - HTTP connection
{
  return (http ? (size_t)http->data_remaining : 0);
}


//
// 'httpGets()' - Get a line of text from a HTTP connection.
//

char *					// O - Line or `NULL`
httpGets(http_t *http,			// I - HTTP connection
         char   *line,			// I - Line to read into
         size_t length)			// I - Max length of buffer
{
  char		*lineptr,		// Pointer into line
		*lineend,		// End of line
		*bufptr,		// Pointer into input buffer
	        *bufend;		// Pointer to end of buffer
  ssize_t	bytes;			// Number of bytes read
  int		eol;			// End-of-line?


  DEBUG_printf("2httpGets(http=%p, line=%p, length=%u)", (void *)http, (void *)line, (unsigned)length);

  if (!http || !line || length <= 1)
    return (NULL);

 /*
  * Read a line from the buffer...
  */

  http->error = 0;
  lineptr     = line;
  lineend     = line + length - 1;
  eol         = 0;

  while (lineptr < lineend)
  {
   /*
    * Pre-load the buffer as needed...
    */

#ifdef _WIN32
    WSASetLastError(0);
#else
    errno = 0;
#endif // _WIN32

    while (http->used == 0)
    {
     /*
      * No newline; see if there is more data to be read...
      */

      while (!_httpWait(http, http->wait_value, 1))
      {
	if (http->timeout_cb && (*http->timeout_cb)(http, http->timeout_data))
	  continue;

        DEBUG_puts("3httpGets: Timed out!");
#ifdef _WIN32
        http->error = WSAETIMEDOUT;
#else
        http->error = ETIMEDOUT;
#endif // _WIN32
        return (NULL);
      }

      bytes = http_read(http, http->buffer + http->used, (size_t)(HTTP_MAX_BUFFER - http->used));

      DEBUG_printf("4httpGets: read " CUPS_LLFMT " bytes.", CUPS_LLCAST bytes);

      if (bytes < 0)
      {
       /*
	* Nope, can't get a line this time...
	*/

#ifdef _WIN32
        DEBUG_printf("3httpGets: recv() error %d!", WSAGetLastError());

        if (WSAGetLastError() == WSAEINTR)
	  continue;
	else if (WSAGetLastError() == WSAEWOULDBLOCK)
	{
	  if (http->timeout_cb && (*http->timeout_cb)(http, http->timeout_data))
	    continue;

	  http->error = WSAGetLastError();
	}
	else if (WSAGetLastError() != http->error)
	{
	  http->error = WSAGetLastError();
	  continue;
	}

#else
        DEBUG_printf("3httpGets: recv() error %d!", errno);

        if (errno == EINTR)
	  continue;
	else if (errno == EWOULDBLOCK || errno == EAGAIN)
	{
	  if (http->timeout_cb && (*http->timeout_cb)(http, http->timeout_data))
	    continue;
	  else if (!http->timeout_cb && errno == EAGAIN)
	    continue;

	  http->error = errno;
	}
	else if (errno != http->error)
	{
	  http->error = errno;
	  continue;
	}
#endif // _WIN32

        return (NULL);
      }
      else if (bytes == 0)
      {
	http->error = EPIPE;

        return (NULL);
      }

     /*
      * Yup, update the amount used...
      */

      http->used += (int)bytes;
    }

   /*
    * Now copy as much of the current line as possible...
    */

    for (bufptr = http->buffer, bufend = http->buffer + http->used;
         lineptr < lineend && bufptr < bufend;)
    {
      if (*bufptr == 0x0a)
      {
        eol = 1;
	bufptr ++;
	break;
      }
      else if (*bufptr == 0x0d)
	bufptr ++;
      else
	*lineptr++ = *bufptr++;
    }

    http->used -= (int)(bufptr - http->buffer);
    if (http->used > 0)
      memmove(http->buffer, bufptr, (size_t)http->used);

    if (eol)
    {
     /*
      * End of line...
      */

      http->activity = time(NULL);

      *lineptr = '\0';

      DEBUG_printf("3httpGets: Returning \"%s\"", line);

      return (line);
    }
  }

  DEBUG_puts("3httpGets: No new line available!");

  return (NULL);
}


//
// 'httpGetState()' - Get the current state of the HTTP request.
//

http_state_t				// O - HTTP state
httpGetState(http_t *http)		// I - HTTP connection
{
  return (http ? http->state : HTTP_STATE_ERROR);
}


//
// 'httpGetStatus()' - Get the status of the last HTTP request.
//
//
//

http_status_t				// O - HTTP status
httpGetStatus(http_t *http)		// I - HTTP connection
{
  return (http ? http->status : HTTP_STATUS_ERROR);
}


//
// 'httpGetSubField()' - Get a sub-field value.
//
//
//

char *					// O - Value or `NULL`
httpGetSubField(http_t       *http,	// I - HTTP connection
                http_field_t field,	// I - Field index
                const char   *name,	// I - Name of sub-field
		char         *value,	// O - Value string
		size_t       valuelen)	// I - Size of value buffer
{
  const char	*fptr;			// Pointer into field
  char		temp[HTTP_MAX_VALUE],	// Temporary buffer for name
		*ptr,			// Pointer into string buffer
		*end;			// End of value buffer

  DEBUG_printf("2httpGetSubField(http=%p, field=%d, name=\"%s\", value=%p, valuelen=%u)", (void *)http, field, name, (void *)value, (unsigned)valuelen);

  if (value)
    *value = '\0';

  if (!http || !name || !value || valuelen < 2 ||
      field <= HTTP_FIELD_UNKNOWN || field >= HTTP_FIELD_MAX || !http->fields[field])
    return (NULL);

  end = value + valuelen - 1;

  for (fptr = http->fields[field]; *fptr;)
  {
   /*
    * Skip leading whitespace...
    */

    while (_cups_isspace(*fptr))
      fptr ++;

    if (*fptr == ',')
    {
      fptr ++;
      continue;
    }

   /*
    * Get the sub-field name...
    */

    for (ptr = temp;
         *fptr && *fptr != '=' && !_cups_isspace(*fptr) &&
	     ptr < (temp + sizeof(temp) - 1);
         *ptr++ = *fptr++);

    *ptr = '\0';

    DEBUG_printf("4httpGetSubField: name=\"%s\"", temp);

   /*
    * Skip trailing chars up to the '='...
    */

    while (_cups_isspace(*fptr))
      fptr ++;

    if (!*fptr)
      break;

    if (*fptr != '=')
      continue;

   /*
    * Skip = and leading whitespace...
    */

    fptr ++;

    while (_cups_isspace(*fptr))
      fptr ++;

    if (*fptr == '\"')
    {
     /*
      * Read quoted string...
      */

      for (ptr = value, fptr ++;
           *fptr && *fptr != '\"' && ptr < end;
	   *ptr++ = *fptr++);

      *ptr = '\0';

      while (*fptr && *fptr != '\"')
        fptr ++;

      if (*fptr)
        fptr ++;
    }
    else
    {
     /*
      * Read unquoted string...
      */

      for (ptr = value;
           *fptr && !_cups_isspace(*fptr) && *fptr != ',' && ptr < end;
	   *ptr++ = *fptr++);

      *ptr = '\0';

      while (*fptr && !_cups_isspace(*fptr) && *fptr != ',')
        fptr ++;
    }

    DEBUG_printf("4httpGetSubField: value=\"%s\"", value);

   /*
    * See if this is the one...
    */

    if (!strcmp(name, temp))
    {
      DEBUG_printf("3httpGetSubField: Returning \"%s\"", value);
      return (value);
    }
  }

  value[0] = '\0';

  DEBUG_puts("3httpGetSubField: Returning NULL");

  return (NULL);
}


//
// 'httpGetVersion()' - Get the HTTP version at the other end.
//

http_version_t				// O - Version number
httpGetVersion(http_t *http)		// I - HTTP connection
{
  return (http ? http->version : HTTP_VERSION_1_0);
}


//
// 'httpInitialize()' - Initialize the HTTP interface library and set the
//                      default HTTP proxy (if any).
//

void
httpInitialize(void)
{
  static int	initialized = 0;	// Have we been called before?
#ifdef _WIN32
  WSADATA	winsockdata;		// WinSock data
#endif // _WIN32


  _cupsGlobalLock();
  if (initialized)
  {
    _cupsGlobalUnlock();
    return;
  }

#ifdef _WIN32
  WSAStartup(MAKEWORD(2,2), &winsockdata);

#elif !defined(SO_NOSIGPIPE)
 /*
  * Ignore SIGPIPE signals...
  */

  struct sigaction	action;		// POSIX sigaction data

  memset(&action, 0, sizeof(action));
  action.sa_handler = SIG_IGN;
  sigaction(SIGPIPE, &action, NULL);
#endif // _WIN32

#  ifdef HAVE_TLS
  _httpTLSInitialize();
#  endif // HAVE_TLS

  initialized = 1;
  _cupsGlobalUnlock();
}


//
// 'httpIsChunked()' - Report whether a message body is chunked.
//
// This function returns `true` if the message body is composed of
// variable-length chunks.
//

bool					// O - `true` if chunked, `false` if not
httpIsChunked(http_t *http)		// I - HTTP connection
{
  return (http ? http->data_encoding == HTTP_ENCODING_CHUNKED : 0);
}


//
// 'httpIsEncrypted()' - Report whether a connection is encrypted.
//
// This function returns `true` if the connection is currently encrypted.
//

bool					// O - `true` if encrypted, `false` if not
httpIsEncrypted(http_t *http)		// I - HTTP connection
{
  return (http ? http->tls != NULL : 0);
}


//
// 'httpPeek()' - Peek at data from a HTTP connection.
//
// This function copies available data from the given HTTP connection, reading
// a buffer as needed.  The data is still available for reading using
// @link httpRead@.
//
// For non-blocking connections the usual timeouts apply.
//
//
//

ssize_t					// O - Number of bytes copied
httpPeek(http_t *http,			// I - HTTP connection
         char   *buffer,		// I - Buffer for data
	 size_t length)			// I - Maximum number of bytes
{
  ssize_t	bytes;			// Bytes read
  char		len[32];		// Length string


  DEBUG_printf("httpPeek(http=%p, buffer=%p, length=" CUPS_LLFMT ")", (void *)http, (void *)buffer, CUPS_LLCAST length);

  if (http == NULL || buffer == NULL)
    return (-1);

  http->activity = time(NULL);
  http->error    = 0;

  if (length <= 0)
    return (0);

  if (http->data_encoding == HTTP_ENCODING_CHUNKED &&
      http->data_remaining <= 0)
  {
    DEBUG_puts("2httpPeek: Getting chunk length...");

    if (httpGets(http, len, sizeof(len)) == NULL)
    {
      DEBUG_puts("1httpPeek: Could not get length!");
      return (0);
    }

    if (!len[0])
    {
      DEBUG_puts("1httpPeek: Blank chunk length, trying again...");
      if (!httpGets(http, len, sizeof(len)))
      {
	DEBUG_puts("1httpPeek: Could not get chunk length.");
	return (0);
      }
    }

    http->data_remaining = strtoll(len, NULL, 16);

    if (http->data_remaining < 0)
    {
      DEBUG_puts("1httpPeek: Negative chunk length!");
      return (0);
    }
  }

  DEBUG_printf("2httpPeek: data_remaining=" CUPS_LLFMT, CUPS_LLCAST http->data_remaining);

  if (http->data_remaining <= 0 && http->data_encoding != HTTP_ENCODING_FIELDS)
  {
   /*
    * A zero-length chunk ends a transfer; unless we are reading POST
    * data, go idle...
    */

    if (http->coding >= _HTTP_CODING_GUNZIP)
      http_content_coding_finish(http);

    if (http->data_encoding == HTTP_ENCODING_CHUNKED)
      httpGets(http, len, sizeof(len));

    if (http->state == HTTP_STATE_LOCK_RECV || http->state == HTTP_STATE_POST_RECV || http->state == HTTP_STATE_PROPFIND_RECV || http->state == HTTP_STATE_PROPPATCH_RECV)
      http->state ++;
    else
      http->state = HTTP_STATE_STATUS;

    DEBUG_printf("1httpPeek: 0-length chunk, set state to %s.", httpStateString(http->state));

   /*
    * Prevent future reads for this request...
    */

    http->data_encoding = HTTP_ENCODING_FIELDS;

    return (0);
  }
  else if (length > (size_t)http->data_remaining)
    length = (size_t)http->data_remaining;

  if (http->used == 0 &&
      (http->coding == _HTTP_CODING_IDENTITY ||
       (http->coding >= _HTTP_CODING_GUNZIP && ((z_stream *)http->stream)->avail_in == 0)))
  {
   /*
    * Buffer small reads for better performance...
    */

    ssize_t	buflen;			// Length of read for buffer

    if (!http->blocking)
    {
      while (!httpWait(http, http->wait_value))
      {
	if (http->timeout_cb && (*http->timeout_cb)(http, http->timeout_data))
	  continue;

	return (0);
      }
    }

    if ((size_t)http->data_remaining > sizeof(http->buffer))
      buflen = sizeof(http->buffer);
    else
      buflen = (ssize_t)http->data_remaining;

    DEBUG_printf("2httpPeek: Reading %d bytes into buffer.", (int)buflen);
    bytes = http_read(http, http->buffer, (size_t)buflen);

    DEBUG_printf("2httpPeek: Read " CUPS_LLFMT " bytes into buffer.", CUPS_LLCAST bytes);
    if (bytes > 0)
    {
#ifdef DEBUG
      http_debug_hex("httpPeek", http->buffer, (int)bytes);
#endif // DEBUG

      http->used = (int)bytes;
    }
  }

  if (http->coding >= _HTTP_CODING_GUNZIP)
  {
    int		zerr;			// Decompressor error
    z_stream	stream;			// Copy of decompressor stream

    memset(&stream, 0, sizeof(stream));

    if (http->used > 0 && ((z_stream *)http->stream)->avail_in < HTTP_MAX_BUFFER)
    {
      size_t buflen = HTTP_MAX_BUFFER - ((z_stream *)http->stream)->avail_in;
					// Number of bytes to copy

      if (((z_stream *)http->stream)->avail_in > 0 &&
	  ((z_stream *)http->stream)->next_in > http->sbuffer)
        memmove(http->sbuffer, ((z_stream *)http->stream)->next_in, ((z_stream *)http->stream)->avail_in);

      ((z_stream *)http->stream)->next_in = http->sbuffer;

      if (buflen > (size_t)http->data_remaining)
        buflen = (size_t)http->data_remaining;

      if (buflen > (size_t)http->used)
        buflen = (size_t)http->used;

      DEBUG_printf("1httpPeek: Copying %d more bytes of data into decompression buffer.", (int)buflen);

      memcpy(http->sbuffer + ((z_stream *)http->stream)->avail_in, http->buffer, buflen);
      ((z_stream *)http->stream)->avail_in += buflen;
      http->used            -= (int)buflen;
      http->data_remaining  -= (off_t)buflen;

      if (http->used > 0)
        memmove(http->buffer, http->buffer + buflen, (size_t)http->used);
    }

    DEBUG_printf("2httpPeek: length=%d, avail_in=%d", (int)length, (int)((z_stream *)http->stream)->avail_in);

    if (inflateCopy(&stream, (z_stream *)http->stream) != Z_OK)
    {
      DEBUG_puts("2httpPeek: Unable to copy decompressor stream.");
      http->error = ENOMEM;
      return (-1);
    }

    stream.next_out  = (Bytef *)buffer;
    stream.avail_out = (uInt)length;

    zerr = inflate(&stream, Z_SYNC_FLUSH);
    inflateEnd(&stream);

    if (zerr < Z_OK)
    {
      DEBUG_printf("2httpPeek: zerr=%d", zerr);
#ifdef DEBUG
      http_debug_hex("2httpPeek", (char *)http->sbuffer, (int)((z_stream *)http->stream)->avail_in);
#endif // DEBUG

      http->error = EIO;
      return (-1);
    }

    bytes = (ssize_t)(length - ((z_stream *)http->stream)->avail_out);
  }
  else if (http->used > 0)
  {
    if (length > (size_t)http->used)
      length = (size_t)http->used;

    bytes = (ssize_t)length;

    DEBUG_printf("2httpPeek: grabbing %d bytes from input buffer...", (int)bytes);

    memcpy(buffer, http->buffer, length);
  }
  else
    bytes = 0;

  if (bytes < 0)
  {
#ifdef _WIN32
    if (WSAGetLastError() == WSAEINTR || WSAGetLastError() == WSAEWOULDBLOCK)
      bytes = 0;
    else
      http->error = WSAGetLastError();
#else
    if (errno == EINTR || errno == EAGAIN)
      bytes = 0;
    else
      http->error = errno;
#endif // _WIN32
  }
  else if (bytes == 0)
  {
    http->error = EPIPE;
    return (0);
  }

  return (bytes);
}


//
// 'httpPrintf()' - Print a formatted string to a HTTP connection.
//
// @private@
//

ssize_t					// O - Number of bytes written or `-1` on error
httpPrintf(http_t     *http,		// I - HTTP connection
           const char *format,		// I - printf-style format string
	   ...)				// I - Additional args as needed
{
  ssize_t	bytes;			// Number of bytes to write
  char		buf[65536];		// Buffer for formatted string
  va_list	ap;			// Variable argument pointer


  DEBUG_printf("2httpPrintf(http=%p, format=\"%s\", ...)", (void *)http, format);

  va_start(ap, format);
  bytes = vsnprintf(buf, sizeof(buf), format, ap);
  va_end(ap);

  DEBUG_printf("3httpPrintf: (" CUPS_LLFMT " bytes) %s", CUPS_LLCAST bytes, buf);

  if (bytes > (ssize_t)(sizeof(buf) - 1))
  {
    http->error = ENOMEM;
    return (-1);
  }
  else if (http->data_encoding == HTTP_ENCODING_FIELDS)
  {
    return (httpWrite(http, buf, (size_t)bytes));
  }
  else
  {
    if (http->wused)
    {
      DEBUG_puts("4httpPrintf: flushing existing data...");

      if (httpFlushWrite(http) < 0)
	return (-1);
    }

    return (http_write(http, buf, (size_t)bytes));
  }
}


//
// 'httpRead()' - Read data from a HTTP connection.
//
//
//

ssize_t					// O - Number of bytes read
httpRead(http_t *http,			// I - HTTP connection
         char   *buffer,		// I - Buffer for data
	 size_t length)			// I - Maximum number of bytes
{
  ssize_t	bytes;			// Bytes read


  DEBUG_printf("httpRead(http=%p, buffer=%p, length=" CUPS_LLFMT ") coding=%d data_encoding=%d data_remaining=" CUPS_LLFMT, (void *)http, (void *)buffer, CUPS_LLCAST length, http ? http->coding : 0, http ? http->data_encoding : 0, CUPS_LLCAST (http ? http->data_remaining : -1));

  if (http == NULL || buffer == NULL)
    return (-1);

  http->activity = time(NULL);
  http->error    = 0;

  if (length <= 0)
    return (0);

  if (http->coding >= _HTTP_CODING_GUNZIP)
  {
    do
    {
      if (((z_stream *)http->stream)->avail_in > 0)
      {
	int	zerr;			// Decompressor error

	DEBUG_printf("2httpRead: avail_in=%d, avail_out=%d", (int)((z_stream *)http->stream)->avail_in, (int)length);

	((z_stream *)http->stream)->next_out  = (Bytef *)buffer;
	((z_stream *)http->stream)->avail_out = (uInt)length;

	if ((zerr = inflate((z_stream *)http->stream, Z_SYNC_FLUSH)) < Z_OK)
	{
	  DEBUG_printf("2httpRead: zerr=%d", zerr);
#ifdef DEBUG
          http_debug_hex("2httpRead", (char *)http->sbuffer, (int)((z_stream *)http->stream)->avail_in);
#endif // DEBUG

	  http->error = EIO;
	  return (-1);
	}

	bytes = (ssize_t)(length - ((z_stream *)http->stream)->avail_out);

	DEBUG_printf("2httpRead: avail_in=%d, avail_out=%d, bytes=%d", ((z_stream *)http->stream)->avail_in, ((z_stream *)http->stream)->avail_out, (int)bytes);
      }
      else
        bytes = 0;

      if (bytes == 0)
      {
        ssize_t buflen = HTTP_MAX_BUFFER - (ssize_t)((z_stream *)http->stream)->avail_in;
					// Additional bytes for buffer

        if (buflen > 0)
        {
          if (((z_stream *)http->stream)->avail_in > 0 &&
              ((z_stream *)http->stream)->next_in > http->sbuffer)
            memmove(http->sbuffer, ((z_stream *)http->stream)->next_in, ((z_stream *)http->stream)->avail_in);

	  ((z_stream *)http->stream)->next_in = http->sbuffer;

          DEBUG_printf("1httpRead: Reading up to %d more bytes of data into decompression buffer.", (int)buflen);

          if (http->data_remaining > 0)
          {
	    if (buflen > http->data_remaining)
	      buflen = (ssize_t)http->data_remaining;

	    bytes = http_read_buffered(http, (char *)http->sbuffer + ((z_stream *)http->stream)->avail_in, (size_t)buflen);
          }
          else if (http->data_encoding == HTTP_ENCODING_CHUNKED)
            bytes = http_read_chunk(http, (char *)http->sbuffer + ((z_stream *)http->stream)->avail_in, (size_t)buflen);
          else
            bytes = 0;

          if (bytes < 0)
            return (bytes);
          else if (bytes == 0)
            break;

          DEBUG_printf("1httpRead: Adding " CUPS_LLFMT " bytes to decompression buffer.", CUPS_LLCAST bytes);

          http->data_remaining  -= bytes;
          ((z_stream *)http->stream)->avail_in += (uInt)bytes;

	  if (http->data_remaining <= 0 &&
	      http->data_encoding == HTTP_ENCODING_CHUNKED)
	  {
	   /*
	    * Read the trailing blank line now...
	    */

	    char	len[32];		// Length string

	    httpGets(http, len, sizeof(len));
	  }

          bytes = 0;
        }
        else
          return (0);
      }
    }
    while (bytes == 0);
  }
  else if (http->data_remaining == 0 && http->data_encoding == HTTP_ENCODING_CHUNKED)
  {
    if ((bytes = http_read_chunk(http, buffer, length)) > 0)
    {
      http->data_remaining -= bytes;

      if (http->data_remaining <= 0)
      {
       /*
        * Read the trailing blank line now...
        */

        char	len[32];		// Length string

        httpGets(http, len, sizeof(len));
      }
    }
  }
  else if (http->data_remaining <= 0)
  {
   /*
    * No more data to read...
    */

    return (0);
  }
  else
  {
    DEBUG_printf("1httpRead: Reading up to %d bytes into buffer.", (int)length);

    if (length > (size_t)http->data_remaining)
      length = (size_t)http->data_remaining;

    if ((bytes = http_read_buffered(http, buffer, length)) > 0)
    {
      http->data_remaining -= bytes;

      if (http->data_remaining <= 0 &&
          http->data_encoding == HTTP_ENCODING_CHUNKED)
      {
       /*
        * Read the trailing blank line now...
        */

        char	len[32];		// Length string

        httpGets(http, len, sizeof(len));
      }
    }
  }

  if ((http->coding == _HTTP_CODING_IDENTITY ||
       (http->coding >= _HTTP_CODING_GUNZIP && ((z_stream *)http->stream)->avail_in == 0)) &&
      ((http->data_remaining <= 0 &&
        http->data_encoding == HTTP_ENCODING_LENGTH) ||
       (http->data_encoding == HTTP_ENCODING_CHUNKED && bytes == 0)))
  {
    if (http->coding >= _HTTP_CODING_GUNZIP)
      http_content_coding_finish(http);

    if (http->state == HTTP_STATE_LOCK_RECV || http->state == HTTP_STATE_POST_RECV || http->state == HTTP_STATE_PROPFIND_RECV || http->state == HTTP_STATE_PROPPATCH_RECV)
      http->state ++;
    else if (http->state == HTTP_STATE_COPY_SEND || http->state == HTTP_STATE_DELETE_SEND || http->state == HTTP_STATE_GET_SEND || http->state == HTTP_STATE_LOCK_SEND || http->state == HTTP_STATE_MOVE_SEND || http->state == HTTP_STATE_POST_SEND || http->state == HTTP_STATE_PROPFIND_SEND || http->state == HTTP_STATE_PROPPATCH_SEND)
      http->state = HTTP_STATE_WAITING;
    else
      http->state = HTTP_STATE_STATUS;

    DEBUG_printf("1httpRead: End of content, set state to %s.", httpStateString(http->state));
  }

  return (bytes);
}


//
// 'httpReadRequest()' - Read a HTTP request from a connection.
//
//
//

http_state_t				// O - New state of connection
httpReadRequest(http_t *http,		// I - HTTP connection
                char   *uri,		// I - URI buffer
		size_t urilen)		// I - Size of URI buffer
{
  char	line[4096],			// HTTP request line
	*req_method,			// HTTP request method
	*req_uri,			// HTTP request URI
	*req_version;			// HTTP request version number string


 /*
  * Range check input...
  */

  DEBUG_printf("httpReadRequest(http=%p, uri=%p, urilen=" CUPS_LLFMT ")", (void *)http, (void *)uri, CUPS_LLCAST urilen);

  if (uri)
    *uri = '\0';

  if (!http || !uri || urilen < 1)
  {
    DEBUG_puts("1httpReadRequest: Bad arguments, returning HTTP_STATE_ERROR.");
    return (HTTP_STATE_ERROR);
  }
  else if (http->state != HTTP_STATE_WAITING)
  {
    DEBUG_printf("1httpReadRequest: Bad state %s, returning HTTP_STATE_ERROR.", httpStateString(http->state));
    return (HTTP_STATE_ERROR);
  }

 /*
  * Reset state...
  */

  httpClearFields(http);

  http->activity       = time(NULL);
  http->data_encoding  = HTTP_ENCODING_FIELDS;
  http->data_remaining = 0;
  http->keep_alive     = HTTP_KEEPALIVE_OFF;
  http->status         = HTTP_STATUS_OK;
  http->version        = HTTP_VERSION_1_1;

 /*
  * Read a line from the socket...
  */

  if (!httpGets(http, line, sizeof(line)))
  {
    DEBUG_puts("1httpReadRequest: Unable to read, returning HTTP_STATE_ERROR");
    return (HTTP_STATE_ERROR);
  }

  if (!line[0])
  {
    DEBUG_puts("1httpReadRequest: Blank line, returning HTTP_STATE_WAITING");
    return (HTTP_STATE_WAITING);
  }

  DEBUG_printf("1httpReadRequest: %s", line);

 /*
  * Parse it...
  */

  req_method = line;
  req_uri    = line;

  while (*req_uri && !isspace(*req_uri & 255))
    req_uri ++;

  if (!*req_uri)
  {
    DEBUG_puts("1httpReadRequest: No request URI.");
    _cupsSetError(IPP_STATUS_ERROR_INTERNAL, _("No request URI."), 1);
    return (HTTP_STATE_ERROR);
  }

  *req_uri++ = '\0';

  while (*req_uri && isspace(*req_uri & 255))
    req_uri ++;

  req_version = req_uri;

  while (*req_version && !isspace(*req_version & 255))
    req_version ++;

  if (!*req_version)
  {
    DEBUG_puts("1httpReadRequest: No request protocol version.");
    _cupsSetError(IPP_STATUS_ERROR_INTERNAL, _("No request protocol version."), 1);
    return (HTTP_STATE_ERROR);
  }

  *req_version++ = '\0';

  while (*req_version && isspace(*req_version & 255))
    req_version ++;

 /*
  * Validate...
  */

  if (!strcmp(req_method, "CONNECT"))
    http->state = HTTP_STATE_CONNECT;
  else if (!strcmp(req_method, "COPY"))
    http->state = HTTP_STATE_COPY;
  else if (!strcmp(req_method, "DELETE"))
    http->state = HTTP_STATE_DELETE;
  else if (!strcmp(req_method, "GET"))
    http->state = HTTP_STATE_GET;
  else if (!strcmp(req_method, "HEAD"))
    http->state = HTTP_STATE_HEAD;
  else if (!strcmp(req_method, "LOCK"))
    http->state = HTTP_STATE_LOCK;
  else if (!strcmp(req_method, "MKCOL"))
    http->state = HTTP_STATE_MKCOL;
  else if (!strcmp(req_method, "MOVE"))
    http->state = HTTP_STATE_MOVE;
  else if (!strcmp(req_method, "OPTIONS"))
    http->state = HTTP_STATE_OPTIONS;
  else if (!strcmp(req_method, "POST"))
    http->state = HTTP_STATE_POST;
  else if (!strcmp(req_method, "PROPFIND"))
    http->state = HTTP_STATE_PROPFIND;
  else if (!strcmp(req_method, "PROPPATCH"))
    http->state = HTTP_STATE_PROPPATCH;
  else if (!strcmp(req_method, "PUT"))
    http->state = HTTP_STATE_PUT;
  else if (!strcmp(req_method, "TRACE"))
    http->state = HTTP_STATE_TRACE;
  else if (!strcmp(req_method, "UNLOCK"))
    http->state = HTTP_STATE_UNLOCK;
  else
  {
    DEBUG_printf("1httpReadRequest: Unknown method \"%s\".", req_method);
    _cupsSetError(IPP_STATUS_ERROR_INTERNAL, _("Unknown request method."), 1);
    return (HTTP_STATE_UNKNOWN_METHOD);
  }

  DEBUG_printf("1httpReadRequest: Set state to %s.", httpStateString(http->state));

  if (!strcmp(req_version, "HTTP/1.0"))
  {
    http->version    = HTTP_VERSION_1_0;
    http->keep_alive = HTTP_KEEPALIVE_OFF;
  }
  else if (!strcmp(req_version, "HTTP/1.1"))
  {
    http->version    = HTTP_VERSION_1_1;
    http->keep_alive = HTTP_KEEPALIVE_ON;
  }
  else
  {
    DEBUG_printf("1httpReadRequest: Unknown version \"%s\".", req_version);
    _cupsSetError(IPP_STATUS_ERROR_INTERNAL, _("Unknown request version."), 1);
    return (HTTP_STATE_UNKNOWN_VERSION);
  }

  DEBUG_printf("1httpReadRequest: URI is \"%s\".", req_uri);
  cupsCopyString(uri, req_uri, urilen);

  return (http->state);
}


//
// 'httpReconnect()' - Reconnect to a HTTP server with timeout and optional cancel.
//

bool					// O - `true` on success, `false` on failure
httpReconnect(http_t *http,		// I - HTTP connection
	      int    msec,		// I - Timeout in milliseconds
	      int    *cancel)		// I - Pointer to "cancel" variable
{
  http_addrlist_t	*addr;		// Connected address
#ifdef DEBUG
  http_addrlist_t	*current;	// Current address
  char			temp[256];	// Temporary address string
#endif // DEBUG


  DEBUG_printf("httpReconnect(http=%p, msec=%d, cancel=%p)", (void *)http, msec, (void *)cancel);

  // Range check input...
  if (!http)
  {
    _cupsSetError(IPP_STATUS_ERROR_INTERNAL, strerror(EINVAL), 0);
    return (false);
  }

#ifdef HAVE_TLS
  if (http->tls)
  {
    DEBUG_puts("2httpReconnect: Shutting down SSL/TLS...");
    _httpTLSStop(http);
  }
#endif // HAVE_TLS

  // Close any previously open socket...
  if (http->fd >= 0)
  {
    DEBUG_printf("2httpReconnect: Closing socket %d...", http->fd);

    httpAddrClose(NULL, http->fd);

    http->fd = -1;
  }

  // Reset all state (except fields, which may be reused)...
  http->state           = HTTP_STATE_WAITING;
  http->version         = HTTP_VERSION_1_1;
  http->keep_alive      = HTTP_KEEPALIVE_OFF;
  http->data_encoding   = HTTP_ENCODING_FIELDS;
  http->used            = 0;
  http->data_remaining  = 0;
  http->hostaddr        = NULL;
  http->wused           = 0;

 /*
  * Connect to the server...
  */

#ifdef DEBUG
  for (current = http->hostlist; current; current = current->next)
    DEBUG_printf("2httpReconnect: Address %s:%d", httpAddrGetString(&(current->addr), temp, sizeof(temp)), httpAddrGetPort(&(current->addr)));
#endif // DEBUG

  if ((addr = httpAddrConnect(http->hostlist, &(http->fd), msec, cancel)) == NULL)
  {
    // Unable to connect...
#ifdef _WIN32
    http->error  = WSAGetLastError();
#else
    http->error  = errno;
#endif // _WIN32
    http->status = HTTP_STATUS_ERROR;

    DEBUG_printf("1httpReconnect: httpAddrConnect failed: %s", strerror(http->error));

    return (false);
  }

  DEBUG_printf("2httpReconnect: New socket=%d", http->fd);

  if (http->timeout_value > 0)
    http_set_timeout(http->fd, http->timeout_value);

  http->hostaddr = &(addr->addr);
  http->error    = 0;

#ifdef HAVE_TLS
  if (http->encryption == HTTP_ENCRYPTION_ALWAYS)
  {
   /*
    * Always do encryption via SSL.
    */

    if (!_httpTLSStart(http))
    {
      httpAddrClose(NULL, http->fd);
      http->fd = -1;

      return (false);
    }
  }
  else if (http->encryption == HTTP_ENCRYPTION_REQUIRED && !http->tls_upgrade)
    return (http_tls_upgrade(http));
#endif // HAVE_TLS

  DEBUG_printf("1httpReconnect: Connected to %s:%d...", httpAddrGetString(http->hostaddr, temp, sizeof(temp)), httpAddrGetPort(http->hostaddr));

  return (true);
}


//
// 'httpSetAuthString()' - Set the current authorization string.
//
// This function stores a copy of the current authorization string in the HTTP
// connection object.  You must still call @link httpSetField@ to set
// `HTTP_FIELD_AUTHORIZATION` prior to issuing a HTTP request using
// @link httpWriteRequest@.
//

void
httpSetAuthString(http_t     *http,	// I - HTTP connection
                  const char *scheme,	// I - Auth scheme (`NULL` to clear it)
		  const char *data)	// I - Auth data (`NULL` for none)
{
 /*
  * Range check input...
  */

  if (!http)
    return;

  free(http->authstring);

  if (scheme)
  {
   /*
    * Set the current authorization string...
    */

    size_t len = strlen(scheme) + (data ? strlen(data) + 1 : 0) + 1;

    if ((http->authstring = malloc(len)) != NULL)
    {
      if (data)
	snprintf(http->authstring, len, "%s %s", scheme, data);
      else
	cupsCopyString(http->authstring, scheme, len);
    }
  }
  else
  {
   /*
    * Clear the current authorization string...
    */

    http->authstring = NULL;
  }
}


//
// 'httpSetBlocking()' - Set blocking/non-blocking behavior on a connection.
//
// This function sets whether a connection uses blocking behavior when reading
// or writing data.  The "http" argument specifies the HTTP connection and the
// "blocking" argument specifies whether to use blocking behavior.
//

void
httpSetBlocking(http_t *http,		// I - HTTP connection
                bool   blocking)	// I - `true` = blocking, `false` = non-blocking
{
  if (http)
  {
    http->blocking = blocking;
    http_set_wait(http);
  }
}


//
// 'httpSetCredentials()' - Set the credentials associated with an encrypted
//			    connection.
//

bool					// O - `true` on success, `false` on error
httpSetCredentials(
    http_t       *http,			// I - HTTP connection
    cups_array_t *credentials)		// I - Array of credentials
{
  if (!http || cupsArrayGetCount(credentials) < 1)
    return (false);

#ifdef HAVE_TLS
  _httpFreeCredentials(http->tls_credentials);

  http->tls_credentials = _httpCreateCredentials(credentials);
#endif // HAVE_TLS

  return (http->tls_credentials != NULL);
}


//
// 'httpSetCookie()' - Set the cookie value(s).
//

void
httpSetCookie(http_t     *http,		// I - Connection
              const char *cookie)	// I - Cookie string
{
  if (!http)
    return;

  free(http->cookie);

  if (cookie)
    http->cookie = strdup(cookie);
  else
    http->cookie = NULL;
}


//
// 'httpSetDefaultField()' - Set the default value of a HTTP header.
//
// This function sets the default value for a HTTP header.
//
// > *Note:* Currently only the `HTTP_FIELD_ACCEPT_ENCODING`,
// > `HTTP_FIELD_SERVER`, and `HTTP_FIELD_USER_AGENT` fields can be set.
//

void
httpSetDefaultField(http_t       *http,	// I - HTTP connection
                    http_field_t field,	// I - Field index
	            const char   *value)// I - Value
{
  DEBUG_printf("httpSetDefaultField(http=%p, field=%d(%s), value=\"%s\")", (void *)http, field, field >= HTTP_FIELD_ACCEPT_LANGUAGE && field < HTTP_FIELD_MAX ? http_fields[field] : "unknown", value);

  if (!http || field <= HTTP_FIELD_UNKNOWN || field >= HTTP_FIELD_MAX)
    return;

  free(http->default_fields[field]);

  http->default_fields[field] = value ? strdup(value) : NULL;
}


//
// 'httpSetEncryption()' - Set the required encryption on a HTTP connection.
//

bool					// O - `true` on success, `false` on error
httpSetEncryption(
    http_t            *http,		// I - HTTP connection
    http_encryption_t e)		// I - New encryption preference
{
  DEBUG_printf("httpSetEncryption(http=%p, e=%d)", (void *)http, e);

#ifdef HAVE_TLS
  if (!http)
    return (false);

  if (http->mode == _HTTP_MODE_CLIENT)
  {
    http->encryption = e;

    if ((http->encryption == HTTP_ENCRYPTION_ALWAYS && !http->tls) || (http->encryption == HTTP_ENCRYPTION_NEVER && http->tls))
      return (httpReconnect(http, 30000, NULL));
    else if (http->encryption == HTTP_ENCRYPTION_REQUIRED && !http->tls)
      return (http_tls_upgrade(http));
    else
      return (true);
  }
  else
  {
    if (e == HTTP_ENCRYPTION_NEVER && http->tls)
      return (false);

    http->encryption = e;
    if (e != HTTP_ENCRYPTION_IF_REQUESTED && !http->tls)
      return (_httpTLSStart(http));
    else
      return (true);
  }
#else
  if (e == HTTP_ENCRYPTION_ALWAYS || e == HTTP_ENCRYPTION_REQUIRED)
    return (false);
  else
    return (true);
#endif // HAVE_TLS
}


//
// 'httpSetExpect()' - Set the Expect: header in a request.
//
// This function sets the `Expect:` header in a request.  Currently only
// `HTTP_STATUS_NONE` or `HTTP_STATUS_CONTINUE` is supported for the "expect"
// argument.
//

void
httpSetExpect(http_t        *http,	// I - HTTP connection
              http_status_t expect)	// I - HTTP status to expect (`HTTP_STATUS_NONE` or `HTTP_STATUS_CONTINUE`)
{
  DEBUG_printf("httpSetExpect(http=%p, expect=%d)", (void *)http, expect);

  if (http)
    http->expect = expect;
}


//
// 'httpSetField()' - Set the value of a HTTP header.
//

void
httpSetField(http_t       *http,	// I - HTTP connection
             http_field_t field,	// I - Field index
	     const char   *value)	// I - Value
{
  DEBUG_printf("httpSetField(http=%p, field=%d(%s), value=\"%s\")", (void *)http, field, field >= HTTP_FIELD_ACCEPT_LANGUAGE && field < HTTP_FIELD_MAX ? http_fields[field] : "unknown", value);

  if (!http || field <= HTTP_FIELD_UNKNOWN || field >= HTTP_FIELD_MAX || !value)
    return;

  http_add_field(http, field, value, false);
}


//
// 'httpSetKeepAlive()' - Set the current Keep-Alive state of a connection.
//

void
httpSetKeepAlive(
    http_t           *http,		// I - HTTP connection
    http_keepalive_t keep_alive)	// I - New Keep-Alive value
{
  if (http)
    http->keep_alive = keep_alive;
}


//
// 'httpSetLength()' - Set the `Content-Length` and `Transfer-Encoding` fields.
//
// This function sets the `Content-Length` and `Transfer-Encoding` fields.
// Passing `0` to the "length" argument specifies a chunked transfer encoding
// while other values specify a fixed `Content-Length`.
//

void
httpSetLength(http_t *http,		// I - HTTP connection
              size_t length)		// I - Length (`0` for chunked)
{
  DEBUG_printf("httpSetLength(http=%p, length=" CUPS_LLFMT ")", (void *)http, CUPS_LLCAST length);

  if (!http)
    return;

  if (!length)
  {
    httpSetField(http, HTTP_FIELD_TRANSFER_ENCODING, "chunked");
    httpSetField(http, HTTP_FIELD_CONTENT_LENGTH, "");
  }
  else
  {
    char len[32];			// Length string

    snprintf(len, sizeof(len), CUPS_LLFMT, CUPS_LLCAST length);
    httpSetField(http, HTTP_FIELD_TRANSFER_ENCODING, "");
    httpSetField(http, HTTP_FIELD_CONTENT_LENGTH, len);
  }
}


//
// 'httpSetTimeout()' - Set read/write timeouts and an optional callback.
//
// This function sets the read/write timeouts for a HTTP connection with an
// optional timeout callback.  The timeout callback receives both the HTTP
// connection pointer and a user data pointer, and returns `true` to continue or
// `false` to error (time) out.
//
// ```
// bool // true to continue, false to stop
// timeout_cb(http_t *http, void *user_data)
// {
//   ... "http" contains the HTTP connection, "user_data" contains the callback pointer
// }
// ```
//

void
httpSetTimeout(
    http_t            *http,		// I - HTTP connection
    double            timeout,		// I - Number of seconds for timeout, must be greater than `0`
    http_timeout_cb_t cb,		// I - Callback function or `NULL` for none
    void              *cb_data)		// I - Callback data pointer
{
  if (!http || timeout <= 0.0)
    return;

  http->timeout_cb    = cb;
  http->timeout_data  = cb_data;
  http->timeout_value = timeout;

  if (http->fd >= 0)
    http_set_timeout(http->fd, timeout);

  http_set_wait(http);
}


//
// 'httpShutdown()' - Shutdown one side of a HTTP connection.
//

void
httpShutdown(http_t *http)		// I - HTTP connection
{
  if (!http || http->fd < 0)
    return;

#ifdef HAVE_TLS
  if (http->tls)
    _httpTLSStop(http);
#endif // HAVE_TLS

#ifdef _WIN32
  shutdown(http->fd, SD_RECEIVE);	// Microsoft-ism...
#else
  shutdown(http->fd, SHUT_RD);
#endif // _WIN32
}


//
// '_httpUpdate()' - Update the current HTTP status for incoming data.
//
// Note: Unlike httpUpdate(), this function does not flush pending write data
// and only retrieves a single status line from the HTTP connection.
//

int					// O - 1 to continue, 0 to stop
_httpUpdate(http_t        *http,	// I - HTTP connection
            http_status_t *status)	// O - Current HTTP status
{
  char		line[32768],		// Line from connection...
		*value;			// Pointer to value on line
  http_field_t	field;			// Field index
  int		major, minor;		// HTTP version numbers


  DEBUG_printf("_httpUpdate(http=%p, status=%p), state=%s", (void *)http, (void *)status, httpStateString(http->state));

 /*
  * Grab a single line from the connection...
  */

  if (!httpGets(http, line, sizeof(line)))
  {
    *status = HTTP_STATUS_ERROR;
    return (0);
  }

  DEBUG_printf("2_httpUpdate: Got \"%s\"", line);

  if (line[0] == '\0')
  {
   /*
    * Blank line means the start of the data section (if any).  Return
    * the result code, too...
    *
    * If we get status 100 (HTTP_STATUS_CONTINUE), then we *don't* change
    * states.  Instead, we just return HTTP_STATUS_CONTINUE to the caller and
    * keep on tryin'...
    */

    if (http->status == HTTP_STATUS_CONTINUE)
    {
      *status = http->status;
      return (0);
    }

    if (http->status < HTTP_STATUS_BAD_REQUEST)
      http->digest_tries = 0;

#ifdef HAVE_TLS
    if (http->status == HTTP_STATUS_SWITCHING_PROTOCOLS && !http->tls)
    {
      if (!_httpTLSStart(http))
      {
        httpAddrClose(NULL, http->fd);
        http->fd = -1;

	*status = http->status = HTTP_STATUS_ERROR;
	return (0);
      }

      *status = HTTP_STATUS_CONTINUE;
      return (0);
    }
#endif // HAVE_TLS

    if (http_set_length(http) < 0)
    {
      DEBUG_puts("1_httpUpdate: Bad Content-Length.");
      http->error  = EINVAL;
      http->status = *status = HTTP_STATUS_ERROR;
      return (0);
    }

    switch (http->state)
    {
      case HTTP_STATE_COPY :
      case HTTP_STATE_DELETE :
      case HTTP_STATE_GET :
      case HTTP_STATE_LOCK :
      case HTTP_STATE_LOCK_RECV :
      case HTTP_STATE_POST :
      case HTTP_STATE_POST_RECV :
      case HTTP_STATE_PROPFIND :
      case HTTP_STATE_PROPFIND_RECV :
      case HTTP_STATE_PROPPATCH :
      case HTTP_STATE_PROPPATCH_RECV :
      case HTTP_STATE_PUT :
	  http->state ++;

	  DEBUG_printf("1_httpUpdate: Set state to %s.", httpStateString(http->state));

      case HTTP_STATE_CONNECT :
      case HTTP_STATE_HEAD :
      case HTTP_STATE_LOCK_SEND :
      case HTTP_STATE_MKCOL :
      case HTTP_STATE_POST_SEND :
      case HTTP_STATE_PROPFIND_SEND :
      case HTTP_STATE_PROPPATCH_SEND :
      case HTTP_STATE_TRACE :
      case HTTP_STATE_UNLOCK :
	  break;

      default :
	  http->state = HTTP_STATE_WAITING;

	  DEBUG_puts("1_httpUpdate: Reset state to HTTP_STATE_WAITING.");
	  break;
    }

    DEBUG_puts("1_httpUpdate: Calling http_content_coding_start.");
    http_content_coding_start(http,
                              httpGetField(http, HTTP_FIELD_CONTENT_ENCODING));

    *status = http->status;
    return (0);
  }
  else if (!strncmp(line, "HTTP/", 5) && http->mode == _HTTP_MODE_CLIENT)
  {
   /*
    * Got the beginning of a response...
    */

    int	intstatus;			// Status value as an integer

    if (sscanf(line, "HTTP/%d.%d%d", &major, &minor, &intstatus) != 3)
    {
      *status = http->status = HTTP_STATUS_ERROR;
      return (0);
    }

    httpClearFields(http);

    http->version = (http_version_t)(major * 100 + minor);
    *status       = http->status = (http_status_t)intstatus;
  }
  else if ((value = strchr(line, ':')) != NULL)
  {
   /*
    * Got a value...
    */

    *value++ = '\0';
    while (_cups_isspace(*value))
      value ++;

    DEBUG_printf("1_httpUpdate: Header %s: %s", line, value);

   /*
    * Be tolerants of servers that send unknown attribute fields...
    */

    if (!_cups_strcasecmp(line, "expect"))
    {
     /*
      * "Expect: 100-continue" or similar...
      */

      http->expect = (http_status_t)atoi(value);
    }
    else if (!_cups_strcasecmp(line, "cookie"))
    {
     /*
      * "Cookie: name=value[; name=value ...]" - replaces previous cookies...
      */

      httpSetCookie(http, value);
    }
    else if ((field = httpFieldValue(line)) != HTTP_FIELD_UNKNOWN)
    {
      http_add_field(http, field, value, true);

      if (field == HTTP_FIELD_AUTHENTICATION_INFO)
        httpGetSubField(http, HTTP_FIELD_AUTHENTICATION_INFO, "nextnonce", http->nextnonce, (int)sizeof(http->nextnonce));
    }
#ifdef DEBUG
    else
      DEBUG_printf("1_httpUpdate: unknown field %s seen!", line);
#endif // DEBUG
  }
  else
  {
    DEBUG_printf("1_httpUpdate: Bad response line \"%s\"!", line);
    http->error  = EINVAL;
    http->status = *status = HTTP_STATUS_ERROR;
    return (0);
  }

  return (1);
}


//
// 'httpUpdate()' - Update the current HTTP state for incoming data.
//

http_status_t				// O - HTTP status
httpUpdate(http_t *http)		// I - HTTP connection
{
  http_status_t	status;			// Request status


  DEBUG_printf("httpUpdate(http=%p), state=%s", (void *)http, httpStateString(http->state));

 /*
  * Flush pending data, if any...
  */

  if (http->wused)
  {
    DEBUG_puts("2httpUpdate: flushing buffer...");

    if (httpFlushWrite(http) < 0)
      return (HTTP_STATUS_ERROR);
  }

 /*
  * If we haven't issued any commands, then there is nothing to "update"...
  */

  if (http->state == HTTP_STATE_WAITING)
    return (HTTP_STATUS_CONTINUE);

 /*
  * Grab all of the lines we can from the connection...
  */

  while (_httpUpdate(http, &status));

 /*
  * See if there was an error...
  */

  if (http->error == EPIPE && http->status > HTTP_STATUS_CONTINUE)
  {
    DEBUG_printf("1httpUpdate: Returning status %d...", http->status);
    return (http->status);
  }

  if (http->error)
  {
    DEBUG_printf("1httpUpdate: socket error %d - %s", http->error, strerror(http->error));
    http->status = HTTP_STATUS_ERROR;
    return (HTTP_STATUS_ERROR);
  }

 /*
  * Return the current status...
  */

  return (status);
}


//
// '_httpWait()' - Wait for data available on a connection (no flush).
//

bool					// O - `true` if data is available, `false` otherwise
_httpWait(http_t *http,			// I - HTTP connection
          int    msec,			// I - Milliseconds to wait
	  bool   usessl)		// I - Use SSL context?
{
  struct pollfd		pfd;		// Polled file descriptor
  int			nfds;		// Result from select()/poll()


  DEBUG_printf("4_httpWait(http=%p, msec=%d, usessl=%d)", (void *)http, msec, usessl);

  if (http->fd < 0)
  {
    DEBUG_printf("5_httpWait: Returning 0 since fd=%d", http->fd);
    return (false);
  }

 /*
  * Check the SSL/TLS buffers for data first...
  */

#ifdef HAVE_TLS
  if (http->tls && _httpTLSPending(http))
  {
    DEBUG_puts("5_httpWait: Return 1 since there is pending TLS data.");
    return (true);
  }
#endif // HAVE_TLS

 /*
  * Then try doing a select() or poll() to poll the socket...
  */

  pfd.fd     = http->fd;
  pfd.events = POLLIN;

  do
  {
    nfds = poll(&pfd, 1, msec);
  }
#ifdef _WIN32
  while (nfds < 0 && (WSAGetLastError() == WSAEINTR || WSAGetLastError() == WSAEWOULDBLOCK));
#else
  while (nfds < 0 && (errno == EINTR || errno == EAGAIN));
#endif // _WIN32

  DEBUG_printf("5_httpWait: returning with nfds=%d, errno=%d...", nfds, errno);

  return (nfds > 0);
}


//
// 'httpWait()' - Wait for data available on a connection.
//

bool					// O - `true` when data is available, `false` otherwise
httpWait(http_t *http,			// I - HTTP connection
         int    msec)			// I - Milliseconds to wait
{
  // First see if there is data in the buffer...
  DEBUG_printf("2httpWait(http=%p, msec=%d)", (void *)http, msec);

  if (http == NULL)
    return (false);

  if (http->used)
  {
    DEBUG_puts("3httpWait: Returning 1 since there is buffered data ready.");
    return (true);
  }

  if (http->coding >= _HTTP_CODING_GUNZIP && ((z_stream *)http->stream)->avail_in > 0)
  {
    DEBUG_puts("3httpWait: Returning 1 since there is buffered data ready.");
    return (true);
  }

 /*
  * Flush pending data, if any...
  */

  if (http->wused)
  {
    DEBUG_puts("3httpWait: Flushing write buffer.");

    if (httpFlushWrite(http) < 0)
      return (false);
  }

 /*
  * If not, check the SSL/TLS buffers and do a select() on the connection...
  */

  return (_httpWait(http, msec, 1));
}


//
// 'httpWrite()' - Write data to a HTTP connection.
//
//
//

ssize_t					// O - Number of bytes written
httpWrite(http_t     *http,		// I - HTTP connection
          const char *buffer,		// I - Buffer for data
	  size_t     length)		// I - Number of bytes to write
{
  ssize_t	bytes;			// Bytes written


  DEBUG_printf("httpWrite(http=%p, buffer=%p, length=" CUPS_LLFMT ")", (void *)http, (void *)buffer, CUPS_LLCAST length);

 /*
  * Range check input...
  */

  if (!http || !buffer)
  {
    DEBUG_puts("1httpWrite: Returning -1 due to bad input.");
    return (-1);
  }

 /*
  * Mark activity on the connection...
  */

  http->activity = time(NULL);

 /*
  * Buffer small writes for better performance...
  */

  if (http->coding == _HTTP_CODING_GZIP || http->coding == _HTTP_CODING_DEFLATE)
  {
    DEBUG_printf("1httpWrite: http->coding=%d", http->coding);

    if (length == 0)
    {
      http_content_coding_finish(http);
      bytes = 0;
    }
    else
    {
      size_t	slen;			// Bytes to write
      ssize_t	sret;			// Bytes written

      ((z_stream *)http->stream)->next_in   = (Bytef *)buffer;
      ((z_stream *)http->stream)->avail_in  = (uInt)length;

      while (deflate((z_stream *)http->stream, Z_NO_FLUSH) == Z_OK)
      {
        DEBUG_printf("1httpWrite: avail_out=%d", ((z_stream *)http->stream)->avail_out);

        if (((z_stream *)http->stream)->avail_out > 0)
	  continue;

	slen = _HTTP_MAX_SBUFFER - ((z_stream *)http->stream)->avail_out;

        DEBUG_printf("1httpWrite: Writing intermediate chunk, len=%d", (int)slen);

	if (slen > 0 && http->data_encoding == HTTP_ENCODING_CHUNKED)
	  sret = http_write_chunk(http, (char *)http->sbuffer, slen);
	else if (slen > 0)
	  sret = http_write(http, (char *)http->sbuffer, slen);
	else
	  sret = 0;

        if (sret < 0)
	{
	  DEBUG_puts("1httpWrite: Unable to write, returning -1.");
	  return (-1);
	}

	((z_stream *)http->stream)->next_out  = (Bytef *)http->sbuffer;
	((z_stream *)http->stream)->avail_out = (uInt)_HTTP_MAX_SBUFFER;
      }

      bytes = (ssize_t)length;
    }
  }
  else if (length > 0)
  {
    if (http->wused && (length + (size_t)http->wused) > sizeof(http->wbuffer))
    {
      DEBUG_printf("2httpWrite: Flushing buffer (wused=%d, length=" CUPS_LLFMT ")", http->wused, CUPS_LLCAST length);

      httpFlushWrite(http);
    }

    if ((length + (size_t)http->wused) <= sizeof(http->wbuffer) && length < sizeof(http->wbuffer))
    {
     /*
      * Write to buffer...
      */

      DEBUG_printf("2httpWrite: Copying " CUPS_LLFMT " bytes to wbuffer...", CUPS_LLCAST length);

      memcpy(http->wbuffer + http->wused, buffer, length);
      http->wused += (int)length;
      bytes = (ssize_t)length;
    }
    else
    {
     /*
      * Otherwise write the data directly...
      */

      DEBUG_printf("2httpWrite: Writing " CUPS_LLFMT " bytes to socket...", CUPS_LLCAST length);

      if (http->data_encoding == HTTP_ENCODING_CHUNKED)
	bytes = (ssize_t)http_write_chunk(http, buffer, length);
      else
	bytes = (ssize_t)http_write(http, buffer, length);

      DEBUG_printf("2httpWrite: Wrote " CUPS_LLFMT " bytes...", CUPS_LLCAST bytes);
    }

    if (http->data_encoding == HTTP_ENCODING_LENGTH)
      http->data_remaining -= bytes;
  }
  else
    bytes = 0;

 /*
  * Handle end-of-request processing...
  */

  if ((http->data_encoding == HTTP_ENCODING_CHUNKED && length == 0) ||
      (http->data_encoding == HTTP_ENCODING_LENGTH && http->data_remaining == 0))
  {
   /*
    * Finished with the transfer; unless we are sending POST or PUT
    * data, go idle...
    */

    if (http->coding == _HTTP_CODING_GZIP || http->coding == _HTTP_CODING_DEFLATE)
      http_content_coding_finish(http);

    if (http->wused)
    {
      if (httpFlushWrite(http) < 0)
        return (-1);
    }

    if (http->data_encoding == HTTP_ENCODING_CHUNKED)
    {
     /*
      * Send a 0-length chunk at the end of the request...
      */

      http_write(http, "0\r\n\r\n", 5);

     /*
      * Reset the data state...
      */

      http->data_encoding  = HTTP_ENCODING_FIELDS;
      http->data_remaining = 0;
    }

    if (http->state == HTTP_STATE_LOCK_RECV || http->state == HTTP_STATE_POST_RECV || http->state == HTTP_STATE_PROPFIND_RECV || http->state == HTTP_STATE_PROPPATCH_RECV)
      http->state ++;
    else if (http->state == HTTP_STATE_COPY_SEND || http->state == HTTP_STATE_DELETE_SEND || http->state == HTTP_STATE_GET_SEND || http->state ==  HTTP_STATE_LOCK_SEND || http->state == HTTP_STATE_MOVE_SEND || http->state == HTTP_STATE_POST_SEND || http->state == HTTP_STATE_PROPFIND_SEND || http->state == HTTP_STATE_PROPPATCH_SEND)
      http->state = HTTP_STATE_WAITING;
    else
      http->state = HTTP_STATE_STATUS;

    DEBUG_printf("2httpWrite: Changed state to %s.", httpStateString(http->state));
  }

  DEBUG_printf("1httpWrite: Returning " CUPS_LLFMT ".", CUPS_LLCAST bytes);

  return (bytes);
}


//
// 'httpWriteRequest()' - Write a HTTP request.
//
// This function writes a HTTP request to the specified connection.  The
// "method" parameter specifies the HTTP method ("GET", "POST", "PUT", etc)
// while the "uri" parameter specifies the request URI.  All fields must be
// set using the @link httpSetCookie@, @link httpSetField@, and
// @link httpSetLength@ functions prior to writing the request.
//

bool					// O - `true` on success, `false` on failure
httpWriteRequest(http_t     *http,	// I - HTTP connection
                 const char *method,	// I - Request method ("GET", "POST", "PUT", etc.)
                 const char *uri)	// I - Request URI
{
  if (!strcasecmp(method, "COPY"))
    return (http_send(http, HTTP_STATE_COPY, uri));
  else if (!strcasecmp(method, "DELETE"))
    return (http_send(http, HTTP_STATE_DELETE, uri));
  else if (!strcasecmp(method, "GET"))
    return (http_send(http, HTTP_STATE_GET, uri));
  else if (!strcasecmp(method, "HEAD"))
    return (http_send(http, HTTP_STATE_HEAD, uri));
  else if (!strcasecmp(method, "LOCK"))
    return (http_send(http, HTTP_STATE_LOCK, uri));
  else if (!strcasecmp(method, "MKCOL"))
    return (http_send(http, HTTP_STATE_MKCOL, uri));
  else if (!strcasecmp(method, "MOVE"))
    return (http_send(http, HTTP_STATE_MOVE, uri));
  else if (!strcasecmp(method, "OPTIONS"))
    return (http_send(http, HTTP_STATE_OPTIONS, uri));
  else if (!strcasecmp(method, "POST"))
    return (http_send(http, HTTP_STATE_POST, uri));
  else if (!strcasecmp(method, "PROPFIND"))
    return (http_send(http, HTTP_STATE_PROPFIND, uri));
  else if (!strcasecmp(method, "PROPPATCH"))
    return (http_send(http, HTTP_STATE_PROPPATCH, uri));
  else if (!strcasecmp(method, "PUT"))
    return (http_send(http, HTTP_STATE_PUT, uri));
  else if (!strcasecmp(method, "TRACE"))
    return (http_send(http, HTTP_STATE_TRACE, uri));
  else if (!strcasecmp(method, "UNLOCK"))
    return (http_send(http, HTTP_STATE_UNLOCK, uri));
  else
    return (false);
}


//
// 'httpWriteResponse()' - Write a HTTP response to a client connection.
//

bool					// O - `true` on success, `false` on error
httpWriteResponse(http_t        *http,	// I - HTTP connection
		  http_status_t status)	// I - Status code
{
  http_encoding_t	old_encoding;	// Old data_encoding value
  off_t			old_remaining;	// Old data_remaining value
  cups_lang_t		*lang;		// Response language


  // Range check input...
  DEBUG_printf("httpWriteResponse(http=%p, status=%d)", (void *)http, status);

  if (!http || status < HTTP_STATUS_CONTINUE)
  {
    DEBUG_puts("1httpWriteResponse: Bad input.");
    return (false);
  }

  // Set the various standard fields if they aren't already...
  if (!http->fields[HTTP_FIELD_DATE])
  {
    char	buffer[256];		// Date value

    httpSetField(http, HTTP_FIELD_DATE, httpGetDateString(time(NULL), buffer, sizeof(buffer)));
  }

  if (status >= HTTP_STATUS_BAD_REQUEST && http->keep_alive)
  {
    http->keep_alive = HTTP_KEEPALIVE_OFF;
    httpSetField(http, HTTP_FIELD_KEEP_ALIVE, "");
  }

  if (http->version == HTTP_VERSION_1_1)
  {
    if (!http->fields[HTTP_FIELD_CONNECTION])
    {
      if (http->keep_alive)
	httpSetField(http, HTTP_FIELD_CONNECTION, "Keep-Alive");
      else
	httpSetField(http, HTTP_FIELD_CONNECTION, "close");
    }

    if (http->keep_alive && !http->fields[HTTP_FIELD_KEEP_ALIVE])
      httpSetField(http, HTTP_FIELD_KEEP_ALIVE, "timeout=10");
  }

#ifdef HAVE_TLS
  if (status == HTTP_STATUS_UPGRADE_REQUIRED || status == HTTP_STATUS_SWITCHING_PROTOCOLS)
  {
    if (!http->fields[HTTP_FIELD_CONNECTION])
      httpSetField(http, HTTP_FIELD_CONNECTION, "Upgrade");

    if (!http->fields[HTTP_FIELD_UPGRADE])
      httpSetField(http, HTTP_FIELD_UPGRADE, "TLS/1.3,TLS/1.2");

    if (!http->fields[HTTP_FIELD_CONTENT_LENGTH])
      httpSetField(http, HTTP_FIELD_CONTENT_LENGTH, "0");
  }
#endif // HAVE_TLS

  if (!http->fields[HTTP_FIELD_SERVER])
    httpSetField(http, HTTP_FIELD_SERVER, http->default_fields[HTTP_FIELD_SERVER] ? http->default_fields[HTTP_FIELD_SERVER] : "CUPS/" LIBCUPS_VERSION " IPP/2.0");

  // Set the Accept-Encoding field if it isn't already...
  if (!http->fields[HTTP_FIELD_ACCEPT_ENCODING])
    httpSetField(http, HTTP_FIELD_ACCEPT_ENCODING, http->default_fields[HTTP_FIELD_ACCEPT_ENCODING] ? http->default_fields[HTTP_FIELD_ACCEPT_ENCODING] : "gzip, deflate, identity");

  // Get the response language, if any...
  lang = cupsLangFind(http->fields[HTTP_FIELD_CONTENT_LANGUAGE]);

  // Send the response header...
  old_encoding        = http->data_encoding;
  old_remaining       = http->data_remaining;
  http->data_encoding = HTTP_ENCODING_FIELDS;

  if (httpPrintf(http, "HTTP/%d.%d %d %s\r\n", http->version / 100, http->version % 100, (int)status, _httpStatusString(lang, status)) < 0)
  {
    http->status = HTTP_STATUS_ERROR;
    return (false);
  }

  if (status != HTTP_STATUS_CONTINUE)
  {
    // 100 Continue doesn't have the rest of the response headers...
    int		i;			// Looping var
    const char	*value;			// Field value

    for (i = 0; i < HTTP_FIELD_MAX; i ++)
    {
      if ((value = httpGetField(http, i)) != NULL && *value)
      {
	if (httpPrintf(http, "%s: %s\r\n", http_fields[i], value) < 1)
	{
	  http->status = HTTP_STATUS_ERROR;
	  return (false);
	}
      }
    }

    if (http->cookie)
    {
      if (strchr(http->cookie, ';'))
      {
        if (httpPrintf(http, "Set-Cookie: %s\r\n", http->cookie) < 1)
	{
	  http->status = HTTP_STATUS_ERROR;
	  return (false);
	}
      }
      else if (httpPrintf(http, "Set-Cookie: %s; path=/; httponly;%s\r\n", http->cookie, http->tls ? " secure;" : "") < 1)
      {
	http->status = HTTP_STATUS_ERROR;
	return (false);
      }
    }

    // "Click-jacking" defense...
    if (httpPrintf(http, "X-Frame-Options: DENY\r\nContent-Security-Policy: frame-ancestors 'none'\r\n") < 1)
    {
      http->status = HTTP_STATUS_ERROR;
      return (false);
    }
  }

  if (httpWrite(http, "\r\n", 2) < 2)
  {
    http->status = HTTP_STATUS_ERROR;
    return (false);
  }

  if (httpFlushWrite(http) < 0)
  {
    http->status = HTTP_STATUS_ERROR;
    return (false);
  }

  if (status == HTTP_STATUS_CONTINUE || status == HTTP_STATUS_SWITCHING_PROTOCOLS)
  {
    // Restore the old data_encoding and data_length values...
    http->data_encoding  = old_encoding;
    http->data_remaining = old_remaining;
  }
  else if (http->state == HTTP_STATE_OPTIONS || http->state == HTTP_STATE_HEAD || http->state == HTTP_STATE_PUT || http->state == HTTP_STATE_TRACE || http->state == HTTP_STATE_CONNECT || http->state == HTTP_STATE_STATUS)
  {
    DEBUG_printf("1httpWriteResponse: Resetting state to HTTP_STATE_WAITING, was %s.", httpStateString(http->state));
    http->state = HTTP_STATE_WAITING;
  }
  else
  {
    // Force data_encoding and data_length to be set according to the response
    // headers...
    http_set_length(http);

    if (http->data_encoding == HTTP_ENCODING_LENGTH && http->data_remaining == 0)
    {
      DEBUG_printf("1httpWriteResponse: Resetting state to HTTP_STATE_WAITING, was %s.", httpStateString(http->state));
      http->state = HTTP_STATE_WAITING;
      return (true);
    }

    if (http->state == HTTP_STATE_COPY || http->state == HTTP_STATE_DELETE || http->state == HTTP_STATE_GET || http->state == HTTP_STATE_LOCK_RECV || http->state == HTTP_STATE_MOVE || http->state == HTTP_STATE_POST_RECV || http->state == HTTP_STATE_PROPFIND_RECV || http->state == HTTP_STATE_PROPPATCH_RECV)
      http->state ++;

    // Then start any content encoding...
    DEBUG_puts("1httpWriteResponse: Calling http_content_coding_start.");
    http_content_coding_start(http, httpGetField(http, HTTP_FIELD_CONTENT_ENCODING));
  }

  return (true);
}


//
// 'http_add_field()' - Add a value for a HTTP field, appending if needed.
//

static void
http_add_field(http_t       *http,	// I - HTTP connection
               http_field_t field,	// I - HTTP field
               const char   *value,	// I - Value string
               bool         append)	// I - Append value?
{
  char		temp[1024];		// Temporary value string
  size_t	fieldlen,		// Length of existing value
		valuelen,		// Length of value string
		total;			// Total length of string


  if (field == HTTP_FIELD_HOST)
  {
   /*
    * Special-case for Host: as we don't want a trailing "." on the hostname and
    * need to bracket IPv6 numeric addresses.
    */

    char *ptr = strchr(value, ':');

    if (value[0] != '[' && ptr && strchr(ptr + 1, ':'))
    {
     /*
      * Bracket IPv6 numeric addresses...
      *
      * This is slightly inefficient (basically copying twice), but is an edge
      * case and not worth optimizing...
      */

      snprintf(temp, sizeof(temp), "[%s]", value);
      value = temp;
    }
    else if (*value)
    {
     /*
      * Check for a trailing dot on the hostname...
      */

      cupsCopyString(temp, value, sizeof(temp));
      value = temp;
      ptr   = temp + strlen(temp) - 1;

      if (*ptr == '.')
	*ptr = '\0';
    }
  }

  if (append && field != HTTP_FIELD_ACCEPT_ENCODING && field != HTTP_FIELD_ACCEPT_LANGUAGE && field != HTTP_FIELD_ACCEPT_RANGES && field != HTTP_FIELD_ALLOW && field != HTTP_FIELD_LINK && field != HTTP_FIELD_TRANSFER_ENCODING && field != HTTP_FIELD_UPGRADE && field != HTTP_FIELD_WWW_AUTHENTICATE)
    append = false;

  if (!append && http->fields[field])
  {
    free(http->fields[field]);

    http->fields[field] = NULL;
  }

  valuelen = strlen(value);

  if (!valuelen)
    return;

  if (http->fields[field])
  {
    fieldlen = strlen(http->fields[field]);
    total    = fieldlen + 2 + valuelen;
  }
  else
  {
    fieldlen = 0;
    total    = valuelen;
  }

  if (fieldlen)
  {
   /*
    * Expand the field value...
    */

    char *mcombined;			// New value string

    if ((mcombined = realloc(http->fields[field], total + 1)) != NULL)
    {
      http->fields[field] = mcombined;
      cupsConcatString(mcombined, ", ", total + 1);
      cupsConcatString(mcombined, value, total + 1);
    }
  }
  else
  {
   /*
    * Allocate the field value...
    */

    http->fields[field] = strdup(value);
  }

  DEBUG_printf("1http_add_field: append=%s, field=%d(%s), value=\"%s\".", append ? "true" : "false", field, http_fields[field], http->fields[field]);

  if (field == HTTP_FIELD_CONTENT_ENCODING && http->data_encoding != HTTP_ENCODING_FIELDS)
  {
    DEBUG_puts("1http_add_field: Calling http_content_coding_start.");
    http_content_coding_start(http, value);
  }
}


//
// 'http_content_coding_finish()' - Finish doing any content encoding.
//

static void
http_content_coding_finish(
    http_t *http)			// I - HTTP connection
{
  int		zerr;			// Compression status
  Byte		dummy[1];		// Dummy read buffer
  size_t	bytes;			// Number of bytes to write


  DEBUG_printf("http_content_coding_finish(http=%p)", (void *)http);
  DEBUG_printf("1http_content_coding_finishing: http->coding=%d", http->coding);

  switch (http->coding)
  {
    case _HTTP_CODING_DEFLATE :
    case _HTTP_CODING_GZIP :
        ((z_stream *)http->stream)->next_in  = dummy;
        ((z_stream *)http->stream)->avail_in = 0;

        do
        {
          zerr  = deflate((z_stream *)http->stream, Z_FINISH);
	  bytes = _HTTP_MAX_SBUFFER - ((z_stream *)http->stream)->avail_out;

          if (bytes > 0)
	  {
	    DEBUG_printf("1http_content_coding_finish: Writing trailing chunk, len=%d", (int)bytes);

	    if (http->data_encoding == HTTP_ENCODING_CHUNKED)
	      http_write_chunk(http, (char *)http->sbuffer, bytes);
	    else
	      http_write(http, (char *)http->sbuffer, bytes);
          }

          ((z_stream *)http->stream)->next_out  = (Bytef *)http->sbuffer;
          ((z_stream *)http->stream)->avail_out = (uInt)_HTTP_MAX_SBUFFER;
	}
        while (zerr == Z_OK);

        deflateEnd((z_stream *)http->stream);

        free(http->sbuffer);
        free(http->stream);

        http->sbuffer = NULL;
        http->stream  = NULL;

        if (http->wused)
          httpFlushWrite(http);
        break;

    case _HTTP_CODING_INFLATE :
    case _HTTP_CODING_GUNZIP :
        inflateEnd((z_stream *)http->stream);

        free(http->sbuffer);
        free(http->stream);

        http->sbuffer = NULL;
        http->stream  = NULL;
        break;

    default :
        break;
  }

  http->coding = _HTTP_CODING_IDENTITY;
}


//
// 'http_content_coding_start()' - Start doing content encoding.
//

static void
http_content_coding_start(
    http_t     *http,			// I - HTTP connection
    const char *value)			// I - Value of Content-Encoding
{
  int			zerr;		// Error/status
  _http_coding_t	coding;		// Content coding value


  DEBUG_printf("http_content_coding_start(http=%p, value=\"%s\")", (void *)http, value);

  if (http->coding != _HTTP_CODING_IDENTITY)
  {
    DEBUG_printf("1http_content_coding_start: http->coding already %d.", http->coding);
    return;
  }
  else if (!strcmp(value, "x-gzip") || !strcmp(value, "gzip"))
  {
    if (http->state == HTTP_STATE_GET_SEND ||
        http->state == HTTP_STATE_POST_SEND)
      coding = http->mode == _HTTP_MODE_SERVER ? _HTTP_CODING_GZIP :
                                                 _HTTP_CODING_GUNZIP;
    else if (http->state == HTTP_STATE_POST_RECV ||
             http->state == HTTP_STATE_PUT_RECV)
      coding = http->mode == _HTTP_MODE_CLIENT ? _HTTP_CODING_GZIP :
                                                 _HTTP_CODING_GUNZIP;
    else
    {
      DEBUG_puts("1http_content_coding_start: Not doing content coding.");
      return;
    }
  }
  else if (!strcmp(value, "x-deflate") || !strcmp(value, "deflate"))
  {
    if (http->state == HTTP_STATE_GET_SEND ||
        http->state == HTTP_STATE_POST_SEND)
      coding = http->mode == _HTTP_MODE_SERVER ? _HTTP_CODING_DEFLATE :
                                                 _HTTP_CODING_INFLATE;
    else if (http->state == HTTP_STATE_POST_RECV ||
             http->state == HTTP_STATE_PUT_RECV)
      coding = http->mode == _HTTP_MODE_CLIENT ? _HTTP_CODING_DEFLATE :
                                                 _HTTP_CODING_INFLATE;
    else
    {
      DEBUG_puts("1http_content_coding_start: Not doing content coding.");
      return;
    }
  }
  else
  {
    DEBUG_puts("1http_content_coding_start: Not doing content coding.");
    return;
  }

  switch (coding)
  {
    case _HTTP_CODING_DEFLATE :
    case _HTTP_CODING_GZIP :
        if (http->wused)
          httpFlushWrite(http);

        if ((http->sbuffer = malloc(_HTTP_MAX_SBUFFER)) == NULL)
        {
          http->status = HTTP_STATUS_ERROR;
          http->error  = errno;
          return;
        }

       /*
        * Window size for compression is 11 bits - optimal based on PWG Raster
        * sample files on pwg.org.  -11 is raw deflate, 27 is gzip, per ZLIB
        * documentation.
        */

	if ((http->stream = calloc(1, sizeof(z_stream))) == NULL)
	{
          free(http->sbuffer);

          http->sbuffer = NULL;
          http->status  = HTTP_STATUS_ERROR;
          http->error   = errno;
          return;
	}

        if ((zerr = deflateInit2((z_stream *)http->stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, coding == _HTTP_CODING_DEFLATE ? -11 : 27, 7, Z_DEFAULT_STRATEGY)) < Z_OK)
        {
          free(http->sbuffer);
          free(http->stream);

          http->sbuffer = NULL;
          http->stream  = NULL;
          http->status  = HTTP_STATUS_ERROR;
          http->error   = zerr == Z_MEM_ERROR ? ENOMEM : EINVAL;
          return;
        }

	((z_stream *)http->stream)->next_out  = (Bytef *)http->sbuffer;
	((z_stream *)http->stream)->avail_out = (uInt)_HTTP_MAX_SBUFFER;
        break;

    case _HTTP_CODING_INFLATE :
    case _HTTP_CODING_GUNZIP :
        if ((http->sbuffer = malloc(_HTTP_MAX_SBUFFER)) == NULL)
        {
          http->status = HTTP_STATUS_ERROR;
          http->error  = errno;
          return;
        }

       /*
        * Window size for decompression is up to 15 bits (maximum supported).
        * -15 is raw inflate, 31 is gunzip, per ZLIB documentation.
        */

	if ((http->stream = calloc(1, sizeof(z_stream))) == NULL)
	{
          free(http->sbuffer);

          http->sbuffer = NULL;
          http->status  = HTTP_STATUS_ERROR;
          http->error   = errno;
          return;
	}

        if ((zerr = inflateInit2((z_stream *)http->stream, coding == _HTTP_CODING_INFLATE ? -15 : 31)) < Z_OK)
        {
          free(http->sbuffer);
          free(http->stream);

          http->sbuffer = NULL;
          http->stream  = NULL;
          http->status  = HTTP_STATUS_ERROR;
          http->error   = zerr == Z_MEM_ERROR ? ENOMEM : EINVAL;
          return;
        }

        ((z_stream *)http->stream)->avail_in = 0;
        ((z_stream *)http->stream)->next_in  = http->sbuffer;
        break;

    default :
        break;
  }

  http->coding = coding;

  DEBUG_printf("1http_content_coding_start: http->coding now %d.", http->coding);
}


//
// 'http_create()' - Create an unconnected HTTP connection.
//

static http_t *				// O - HTTP connection
http_create(
    const char        *host,		// I - Hostname
    int               port,		// I - Port number
    http_addrlist_t   *addrlist,	// I - Address list or `NULL`
    int               family,		// I - Address family or AF_UNSPEC
    http_encryption_t encryption,	// I - Encryption to use
    bool              blocking,		// I - `true` for blocking mode
    _http_mode_t      mode)		// I - _HTTP_MODE_CLIENT or _SERVER
{
  http_t	*http;			// New HTTP connection
  char		service[255];		// Service name
  http_addrlist_t *myaddrlist = NULL;	// My address list


  DEBUG_printf("4http_create(host=\"%s\", port=%d, addrlist=%p, family=%d, encryption=%d, blocking=%s, mode=%d)", host, port, (void *)addrlist, family, encryption, blocking ? "true" : "false", mode);

  if (!host && mode == _HTTP_MODE_CLIENT)
    return (NULL);

  httpInitialize();

 /*
  * Lookup the host...
  */

  if (addrlist)
  {
    myaddrlist = httpAddrCopyList(addrlist);
  }
  else
  {
    snprintf(service, sizeof(service), "%d", port);

    myaddrlist = httpAddrGetList(host, family, service);
  }

  if (!myaddrlist)
    return (NULL);

 /*
  * Allocate memory for the structure...
  */

  if ((http = calloc(sizeof(http_t), 1)) == NULL)
  {
    _cupsSetError(IPP_STATUS_ERROR_INTERNAL, strerror(errno), 0);
    httpAddrFreeList(myaddrlist);
    return (NULL);
  }

 /*
  * Initialize the HTTP data...
  */

  http->mode     = mode;
  http->activity = time(NULL);
  http->hostlist = myaddrlist;
  http->blocking = blocking;
  http->fd       = -1;
  http->status   = HTTP_STATUS_CONTINUE;
  http->version  = HTTP_VERSION_1_1;

  if (host)
    cupsCopyString(http->hostname, host, sizeof(http->hostname));

  if (port == 443)			// Always use encryption for https
    http->encryption = HTTP_ENCRYPTION_ALWAYS;
  else
    http->encryption = encryption;

  http_set_wait(http);

 /*
  * Return the new structure...
  */

  return (http);
}


#ifdef DEBUG
//
// 'http_debug_hex()' - Do a hex dump of a buffer.
//

static void
http_debug_hex(const char *prefix,	// I - Prefix for line
               const char *buffer,	// I - Buffer to dump
               int        bytes)	// I - Bytes to dump
{
  int	i, j,				// Looping vars
	ch;				// Current character
  char	line[255],			// Line buffer
	*start,				// Start of line after prefix
	*ptr;				// Pointer into line


  if (_cups_debug_fd < 0 || _cups_debug_level < 6)
    return;

  DEBUG_printf("9%s: %d bytes:", prefix, bytes);

  snprintf(line, sizeof(line), "9%s: ", prefix);
  start = line + strlen(line);

  for (i = 0; i < bytes; i += 16)
  {
    for (j = 0, ptr = start; j < 16 && (i + j) < bytes; j ++, ptr += 2)
      snprintf(ptr, 3, "%02X", buffer[i + j] & 255);

    while (j < 16)
    {
      memcpy(ptr, "  ", 3);
      ptr += 2;
      j ++;
    }

    memcpy(ptr, "  ", 3);
    ptr += 2;

    for (j = 0; j < 16 && (i + j) < bytes; j ++)
    {
      ch = buffer[i + j] & 255;

      if (ch < ' ' || ch >= 127)
	ch = '.';

      *ptr++ = (char)ch;
    }

    *ptr = '\0';
    DEBUG_puts(line);
  }
}
#endif // DEBUG


//
// 'http_free_credential()' - Free a single credential.
//

static void
http_free_credential(
    http_credential_t *c)		// I - Credential
{
  free((void *)c->data);
  free(c);
}


//
// 'http_read()' - Read a buffer from a HTTP connection.
//
// This function does the low-level read from the socket, retrying and timing
// out as needed.
//

static ssize_t				// O - Number of bytes read or -1 on error
http_read(http_t *http,			// I - HTTP connection
          char   *buffer,		// I - Buffer
          size_t length)		// I - Maximum bytes to read
{
  ssize_t	bytes;			// Bytes read


  DEBUG_printf("7http_read(http=%p, buffer=%p, length=" CUPS_LLFMT ")", (void *)http, (void *)buffer, CUPS_LLCAST length);

  if (!http->blocking || http->timeout_value > 0.0)
  {
    while (!httpWait(http, http->wait_value))
    {
      if (http->timeout_cb && (*http->timeout_cb)(http, http->timeout_data))
	continue;

      DEBUG_puts("8http_read: Timeout.");
      return (0);
    }
  }

  DEBUG_printf("8http_read: Reading %d bytes into buffer.", (int)length);

  do
  {
#ifdef HAVE_TLS
    if (http->tls)
      bytes = _httpTLSRead(http, buffer, (int)length);
    else
#endif // HAVE_TLS
    bytes = recv(http->fd, buffer, length, 0);

    if (bytes < 0)
    {
#ifdef _WIN32
      if (WSAGetLastError() != WSAEINTR)
      {
	http->error = WSAGetLastError();
	return (-1);
      }
      else if (WSAGetLastError() == WSAEWOULDBLOCK)
      {
	if (!http->timeout_cb ||
	    !(*http->timeout_cb)(http, http->timeout_data))
	{
	  http->error = WSAEWOULDBLOCK;
	  return (-1);
	}
      }
#else
      DEBUG_printf("8http_read: %s", strerror(errno));

      if (errno == EWOULDBLOCK || errno == EAGAIN)
      {
	if (http->timeout_cb && !(*http->timeout_cb)(http, http->timeout_data))
	{
	  http->error = errno;
	  return (-1);
	}
	else if (!http->timeout_cb && errno != EAGAIN)
	{
	  http->error = errno;
	  return (-1);
	}
      }
      else if (errno != EINTR)
      {
	http->error = errno;
	return (-1);
      }
#endif // _WIN32
    }
  }
  while (bytes < 0);

  DEBUG_printf("8http_read: Read " CUPS_LLFMT " bytes into buffer.", CUPS_LLCAST bytes);
#ifdef DEBUG
  if (bytes > 0)
    http_debug_hex("http_read", buffer, (int)bytes);
  else
#endif // DEBUG
  if (bytes == 0)
  {
    http->error = EPIPE;
    return (0);
  }

  return (bytes);
}


//
// 'http_read_buffered()' - Do a buffered read from a HTTP connection.
//
// This function reads data from the HTTP buffer or from the socket, as needed.
//

static ssize_t				// O - Number of bytes read or -1 on error
http_read_buffered(http_t *http,	// I - HTTP connection
                   char   *buffer,	// I - Buffer
                   size_t length)	// I - Maximum bytes to read
{
  ssize_t	bytes;			// Bytes read


  DEBUG_printf("7http_read_buffered(http=%p, buffer=%p, length=" CUPS_LLFMT ") used=%d", (void *)http, (void *)buffer, CUPS_LLCAST length, http->used);

  if (http->used > 0)
  {
    if (length > (size_t)http->used)
      bytes = (ssize_t)http->used;
    else
      bytes = (ssize_t)length;

    DEBUG_printf("8http_read: Grabbing %d bytes from input buffer.", (int)bytes);

    memcpy(buffer, http->buffer, (size_t)bytes);
    http->used -= (int)bytes;

    if (http->used > 0)
      memmove(http->buffer, http->buffer + bytes, (size_t)http->used);
  }
  else
    bytes = http_read(http, buffer, length);

  return (bytes);
}


//
// 'http_read_chunk()' - Read a chunk from a HTTP connection.
//
// This function reads and validates the chunk length, then does a buffered read
// returning the number of bytes placed in the buffer.
//

static ssize_t				// O - Number of bytes read or -1 on error
http_read_chunk(http_t *http,		// I - HTTP connection
		char   *buffer,		// I - Buffer
		size_t length)		// I - Maximum bytes to read
{
  DEBUG_printf("7http_read_chunk(http=%p, buffer=%p, length=" CUPS_LLFMT ")", (void *)http, (void *)buffer, CUPS_LLCAST length);

  if (http->data_remaining <= 0)
  {
    char	len[32];		// Length string

    if (!httpGets(http, len, sizeof(len)))
    {
      DEBUG_puts("8http_read_chunk: Could not get chunk length.");
      return (0);
    }

    if (!len[0])
    {
      DEBUG_puts("8http_read_chunk: Blank chunk length, trying again...");
      if (!httpGets(http, len, sizeof(len)))
      {
	DEBUG_puts("8http_read_chunk: Could not get chunk length.");
	return (0);
      }
    }

    http->data_remaining = strtoll(len, NULL, 16);

    if (http->data_remaining < 0)
    {
      DEBUG_printf("8http_read_chunk: Negative chunk length \"%s\" (" CUPS_LLFMT ")", len, CUPS_LLCAST http->data_remaining);
      return (0);
    }

    DEBUG_printf("8http_read_chunk: Got chunk length \"%s\" (" CUPS_LLFMT ")", len, CUPS_LLCAST http->data_remaining);

    if (http->data_remaining == 0)
    {
     /*
      * 0-length chunk, grab trailing blank line...
      */

      httpGets(http, len, sizeof(len));
    }
  }

  DEBUG_printf("8http_read_chunk: data_remaining=" CUPS_LLFMT, CUPS_LLCAST http->data_remaining);

  if (http->data_remaining <= 0)
    return (0);
  else if (length > (size_t)http->data_remaining)
    length = (size_t)http->data_remaining;

  return (http_read_buffered(http, buffer, length));
}


//
// 'http_send()' - Send a request with all fields and the trailing blank line.
//

static bool				// O - `true` on success, `false` on error
http_send(http_t       *http,		// I - HTTP connection
          http_state_t request,		// I - Request code
	  const char   *uri)		// I - URI
{
  int		i;			// Looping var
  char		buf[1024];		// Encoded URI buffer
  const char	*value;			// Field value
  static const char * const codes[HTTP_STATE_MAX] =
  {					// Request code strings
    NULL,	// WAITING
    "CONNECT",
    "COPY",
    NULL,
    "DELETE",
    NULL,
    "GET",
    NULL,
    "HEAD",
    "LOCK",
    NULL,
    NULL,
    "MKCOL",
    "MOVE",
    NULL,
    "OPTIONS",
    "POST",
    NULL,
    NULL,
    "PROPFIND",
    NULL,
    NULL,
    "PROPPATCH",
    NULL,
    NULL,
    "PUT",
    NULL,
    "TRACE",
    "UNLOCK",
    NULL,
    NULL,
    NULL
  };


  DEBUG_printf("4http_send(http=%p, request=HTTP_%s, uri=\"%s\")", (void *)http, codes[request], uri);

  if (http == NULL || uri == NULL)
    return (false);

 /*
  * Set the User-Agent field if it isn't already...
  */

  if (!http->fields[HTTP_FIELD_USER_AGENT])
  {
    if (http->default_fields[HTTP_FIELD_USER_AGENT])
      httpSetField(http, HTTP_FIELD_USER_AGENT, http->default_fields[HTTP_FIELD_USER_AGENT]);
    else
      httpSetField(http, HTTP_FIELD_USER_AGENT, cupsGetUserAgent());
  }

 /*
  * Set the Accept-Encoding field if it isn't already...
  */

  if (!http->fields[HTTP_FIELD_ACCEPT_ENCODING] && http->default_fields[HTTP_FIELD_ACCEPT_ENCODING])
    httpSetField(http, HTTP_FIELD_ACCEPT_ENCODING, http->default_fields[HTTP_FIELD_ACCEPT_ENCODING]);

 /*
  * Encode the URI as needed...
  */

  _httpEncodeURI(buf, uri, sizeof(buf));

 /*
  * See if we had an error the last time around; if so, reconnect...
  */

  if (http->fd < 0 || http->status == HTTP_STATUS_ERROR || http->status >= HTTP_STATUS_BAD_REQUEST)
  {
    DEBUG_printf("5http_send: Reconnecting, fd=%d, status=%d, tls_upgrade=%d", http->fd, http->status, http->tls_upgrade);

    if (!httpReconnect(http, 30000, NULL))
      return (false);
  }

 /*
  * Flush any written data that is pending...
  */

  if (http->wused)
  {
    if (httpFlushWrite(http) < 0)
    {
      if (!httpReconnect(http, 30000, NULL))
        return (false);
    }
  }

 /*
  * Send the request header...
  */

  http->state         = request;
  http->data_encoding = HTTP_ENCODING_FIELDS;

  if (request == HTTP_STATE_LOCK || request == HTTP_STATE_POST || request == HTTP_STATE_PROPFIND || request == HTTP_STATE_PROPPATCH || request == HTTP_STATE_PUT)
    http->state ++;

  http->status = HTTP_STATUS_CONTINUE;

#ifdef HAVE_TLS
  if (http->encryption == HTTP_ENCRYPTION_REQUIRED && !http->tls)
  {
    httpSetField(http, HTTP_FIELD_CONNECTION, "Upgrade");
    httpSetField(http, HTTP_FIELD_UPGRADE, "TLS/1.2,TLS/1.1,TLS/1.0");
  }
#endif // HAVE_TLS

  if (httpPrintf(http, "%s %s HTTP/1.1\r\n", codes[request], buf) < 1)
  {
    http->status = HTTP_STATUS_ERROR;
    return (false);
  }

  for (i = 0; i < HTTP_FIELD_MAX; i ++)
  {
    if ((value = httpGetField(http, i)) != NULL && *value)
    {
      DEBUG_printf("5http_send: %s: %s", http_fields[i], value);

      if (i == HTTP_FIELD_HOST)
      {
        // Issue #185: Use "localhost" for the loopback addresses to work
        // around an Avahi bug...
        if (httpAddrIsLocalhost(http->hostaddr))
          value = "localhost";

	if (httpPrintf(http, "Host: %s:%d\r\n", value, httpAddrGetPort(http->hostaddr)) < 1)
	{
	  http->status = HTTP_STATUS_ERROR;
	  return (false);
	}
      }
      else if (httpPrintf(http, "%s: %s\r\n", http_fields[i], value) < 1)
      {
	http->status = HTTP_STATUS_ERROR;
	return (false);
      }
    }
  }

  if (http->cookie)
  {
    if (httpPrintf(http, "Cookie: $Version=0; %s\r\n", http->cookie) < 1)
    {
      http->status = HTTP_STATUS_ERROR;
      return (false);
    }
  }

  DEBUG_printf("5http_send: expect=%d, mode=%d, state=%d", http->expect, http->mode, http->state);

  if (http->expect == HTTP_STATUS_CONTINUE && http->mode == _HTTP_MODE_CLIENT && (http->state == HTTP_STATE_LOCK_RECV || http->state == HTTP_STATE_POST_RECV || http->state == HTTP_STATE_PROPFIND_RECV || http->state == HTTP_STATE_PROPPATCH_RECV || http->state == HTTP_STATE_PUT_RECV))
  {
    if (httpPrintf(http, "Expect: 100-continue\r\n") < 1)
    {
      http->status = HTTP_STATUS_ERROR;
      return (false);
    }
  }

  if (httpPrintf(http, "\r\n") < 1)
  {
    http->status = HTTP_STATUS_ERROR;
    return (false);
  }

  if (httpFlushWrite(http) < 0)
    return (false);

  http_set_length(http);
  httpClearFields(http);

 /*
  * Some authentication strings can only be used once...
  */

  if (http->fields[HTTP_FIELD_AUTHORIZATION] && http->authstring)
  {
    free(http->authstring);
    http->authstring = NULL;
  }

  return (true);
}


//
// 'http_set_length()' - Set the data_encoding and data_remaining values.
//

static off_t				// O - Remainder or -1 on error
http_set_length(http_t *http)		// I - Connection
{
  off_t	remaining;			// Remainder


  DEBUG_printf("4http_set_length(http=%p) mode=%d state=%s", (void *)http, http->mode, httpStateString(http->state));

  if ((remaining = httpGetLength(http)) >= 0)
  {
    if (http->mode == _HTTP_MODE_SERVER && http->state != HTTP_STATE_COPY_SEND && http->state != HTTP_STATE_DELETE_SEND && http->state != HTTP_STATE_GET_SEND && http->state != HTTP_STATE_LOCK && http->state != HTTP_STATE_LOCK_SEND && http->state != HTTP_STATE_MOVE_SEND && http->state != HTTP_STATE_PUT && http->state != HTTP_STATE_POST && http->state != HTTP_STATE_POST_SEND && http->state != HTTP_STATE_PROPFIND && http->state != HTTP_STATE_PROPFIND_SEND && http->state != HTTP_STATE_PROPPATCH && http->state != HTTP_STATE_PROPPATCH_SEND)
    {
      DEBUG_puts("5http_set_length: Not setting data_encoding/remaining.");
      return (remaining);
    }

    if (!_cups_strcasecmp(httpGetField(http, HTTP_FIELD_TRANSFER_ENCODING), "chunked"))
    {
      DEBUG_puts("5http_set_length: Setting data_encoding to HTTP_ENCODING_CHUNKED.");
      http->data_encoding = HTTP_ENCODING_CHUNKED;
    }
    else
    {
      DEBUG_puts("5http_set_length: Setting data_encoding to HTTP_ENCODING_LENGTH.");
      http->data_encoding = HTTP_ENCODING_LENGTH;
    }

    DEBUG_printf("5http_set_length: Setting data_remaining to " CUPS_LLFMT ".", CUPS_LLCAST remaining);
    http->data_remaining = remaining;
  }

  return (remaining);
}

//
// 'http_set_timeout()' - Set the socket timeout values.
//

static void
http_set_timeout(int    fd,		// I - File descriptor
                 double timeout)	// I - Timeout in seconds
{
#ifdef _WIN32
  DWORD tv = (DWORD)(timeout * 1000);
				      // Timeout in milliseconds

  if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, CUPS_SOCAST &tv, sizeof(tv)))
    DEBUG_printf("http_set_timeout: setsockopt(SO_RCVTIMEO) failed - %s", strerror(errno));

  if (setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, CUPS_SOCAST &tv, sizeof(tv)))
    DEBUG_printf("http_set_timeout: setsockopt(SO_SNDTIMEO) failed - %s", strerror(errno));

#else
  struct timeval tv;			// Timeout in secs and usecs

  tv.tv_sec  = (int)timeout;
  tv.tv_usec = (int)(1000000 * fmod(timeout, 1.0));

  if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, CUPS_SOCAST &tv, sizeof(tv)))
    DEBUG_printf("http_set_timeout: setsockopt(SO_RCVTIMEO) failed - %s", strerror(errno));
  if (setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, CUPS_SOCAST &tv, sizeof(tv)))
    DEBUG_printf("http_set_timeout: setsockopt(SO_SNDTIMEO) failed - %s", strerror(errno));
#endif // _WIN32
}


//
// 'http_set_wait()' - Set the default wait value for reads.
//

static void
http_set_wait(http_t *http)		// I - HTTP connection
{
  if (http->blocking)
  {
    http->wait_value = (int)(http->timeout_value * 1000);

    if (http->wait_value <= 0)
      http->wait_value = 60000;
  }
  else
    http->wait_value = 10000;
}


#ifdef HAVE_TLS
//
// 'http_tls_upgrade()' - Force upgrade to TLS encryption.
//

static bool				// O - `true` on success, `false` on failure
http_tls_upgrade(http_t *http)		// I - HTTP connection
{
  bool		ret;			// Return value
  http_t	myhttp;			// Local copy of HTTP data


  DEBUG_printf("4http_tls_upgrade(%p)", (void *)http);

  // Flush the connection to make sure any previous "Upgrade" message
  // has been read.
  httpFlush(http);

  // Copy the HTTP data to a local variable so we can do the OPTIONS
  // request without interfering with the existing request data...
  memcpy(&myhttp, http, sizeof(myhttp));

  // Send an OPTIONS request to the server, requiring SSL or TLS
  // encryption on the link...
  http->tls_upgrade = true;
  memset(http->fields, 0, sizeof(http->fields));
  http->expect = (http_status_t)0;

  if (http->hostname[0] == '/')
    httpSetField(http, HTTP_FIELD_HOST, "localhost");
  else
    httpSetField(http, HTTP_FIELD_HOST, http->hostname);

  httpSetField(http, HTTP_FIELD_CONNECTION, "upgrade");
  httpSetField(http, HTTP_FIELD_UPGRADE, "TLS/1.2,TLS/1.1,TLS/1.0");

  if ((ret = httpWriteRequest(http, "OPTIONS", "*")) == 0)
  {
    // Wait for the secure connection...
    while (httpUpdate(http) == HTTP_STATUS_CONTINUE)
      ;
  }

  // Restore the HTTP request data...
  memcpy(http->fields, myhttp.fields, sizeof(http->fields));

  http->data_encoding   = myhttp.data_encoding;
  http->data_remaining  = myhttp.data_remaining;
  http->expect          = myhttp.expect;
  http->digest_tries    = myhttp.digest_tries;
  http->tls_upgrade     = false;

  // See if we actually went secure...
  if (!http->tls)
  {
    // Server does not support HTTP upgrade...
    DEBUG_puts("5http_tls_upgrade: Server does not support HTTP upgrade!");

    _cupsSetError(IPP_STATUS_ERROR_CUPS_PKI, _("Encryption is not supported."), 1);
    httpAddrClose(NULL, http->fd);

    http->fd = -1;

    return (false);
  }
  else
    return (ret);
}
#endif // HAVE_TLS


//
// 'http_write()' - Write a buffer to a HTTP connection.
//

static ssize_t				// O - Number of bytes written
http_write(http_t     *http,		// I - HTTP connection
           const char *buffer,		// I - Buffer for data
	   size_t     length)		// I - Number of bytes to write
{
  ssize_t	tbytes,			// Total bytes sent
		bytes;			// Bytes sent


  DEBUG_printf("7http_write(http=%p, buffer=%p, length=" CUPS_LLFMT ")", (void *)http, (void *)buffer, CUPS_LLCAST length);
  http->error = 0;
  tbytes      = 0;

  while (length > 0)
  {
    DEBUG_printf("8http_write: About to write %d bytes.", (int)length);

    if (http->timeout_value > 0.0)
    {
      struct pollfd	pfd;		// Polled file descriptor
      int		nfds;		// Result from select()/poll()

      do
      {
	pfd.fd     = http->fd;
	pfd.events = POLLOUT;

	while ((nfds = poll(&pfd, 1, http->wait_value)) < 0)
	{
#ifdef _WIN32
          if (WSAGetLastError() == WSAEINTR || WSAGetLastError() == WSAEWOULDBLOCK)
            break;
#else
	  if (errno == EINTR || errno == EAGAIN)
	    break;
#endif // _WIN32
        }

        if (nfds < 0)
	{
	  http->error = errno;
	  return (-1);
	}
	else if (nfds == 0 && (!http->timeout_cb || !(*http->timeout_cb)(http, http->timeout_data)))
	{
#ifdef _WIN32
	  http->error = WSAEWOULDBLOCK;
#else
	  http->error = EWOULDBLOCK;
#endif // _WIN32
	  return (-1);
	}
      }
      while (nfds <= 0);
    }

#ifdef HAVE_TLS
    if (http->tls)
      bytes = _httpTLSWrite(http, buffer, (int)length);
    else
#endif // HAVE_TLS
    bytes = send(http->fd, buffer, length, 0);

    DEBUG_printf("8http_write: Write of " CUPS_LLFMT " bytes returned " CUPS_LLFMT ".", CUPS_LLCAST length, CUPS_LLCAST bytes);

    if (bytes < 0)
    {
#ifdef _WIN32
      if (WSAGetLastError() == WSAEINTR)
        continue;
      else if (WSAGetLastError() == WSAEWOULDBLOCK)
      {
        if (http->timeout_cb && (*http->timeout_cb)(http, http->timeout_data))
          continue;

        http->error = WSAGetLastError();
      }
      else if (WSAGetLastError() != http->error &&
               WSAGetLastError() != WSAECONNRESET)
      {
        http->error = WSAGetLastError();
	continue;
      }

#else
      if (errno == EINTR)
        continue;
      else if (errno == EWOULDBLOCK || errno == EAGAIN)
      {
	if (http->timeout_cb && (*http->timeout_cb)(http, http->timeout_data))
          continue;
        else if (!http->timeout_cb && errno == EAGAIN)
	  continue;

        http->error = errno;
      }
      else if (errno != http->error && errno != ECONNRESET)
      {
        http->error = errno;
	continue;
      }
#endif // _WIN32

      DEBUG_printf("8http_write: error writing data (%s).", strerror(http->error));

      return (-1);
    }

    buffer += bytes;
    tbytes += bytes;
    length -= (size_t)bytes;
  }

#ifdef DEBUG
  http_debug_hex("http_write", buffer - tbytes, (int)tbytes);
#endif // DEBUG

  DEBUG_printf("8http_write: Returning " CUPS_LLFMT ".", CUPS_LLCAST tbytes);

  return (tbytes);
}


//
// 'http_write_chunk()' - Write a chunked buffer.
//

static ssize_t				// O - Number bytes written
http_write_chunk(http_t     *http,	// I - HTTP connection
                 const char *buffer,	// I - Buffer to write
		 size_t        length)	// I - Length of buffer
{
  char		header[16];		// Chunk header
  ssize_t	bytes;			// Bytes written


  DEBUG_printf("7http_write_chunk(http=%p, buffer=%p, length=" CUPS_LLFMT ")", (void *)http, (void *)buffer, CUPS_LLCAST length);

 /*
  * Write the chunk header, data, and trailer.
  */

  snprintf(header, sizeof(header), "%x\r\n", (unsigned)length);
  if (http_write(http, header, strlen(header)) < 0)
  {
    DEBUG_puts("8http_write_chunk: http_write of length failed.");
    return (-1);
  }

  if ((bytes = http_write(http, buffer, length)) < 0)
  {
    DEBUG_puts("8http_write_chunk: http_write of buffer failed.");
    return (-1);
  }

  if (http_write(http, "\r\n", 2) < 0)
  {
    DEBUG_puts("8http_write_chunk: http_write of CR LF failed.");
    return (-1);
  }

  return (bytes);
}
