//
// HTTP address routines for CUPS.
//
// Copyright © 2021-2024 by OpenPrinting.
// Copyright © 2007-2021 by Apple Inc.
// Copyright © 1997-2006 by Easy Software Products, all rights reserved.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#include "cups-private.h"
#include <sys/stat.h>
#ifdef HAVE_RESOLV_H
#  include <resolv.h>
#endif // HAVE_RESOLV_H
#ifdef __APPLE__
#  include <CoreFoundation/CoreFoundation.h>
#  ifdef HAVE_SCDYNAMICSTORECOPYCOMPUTERNAME
#    include <SystemConfiguration/SystemConfiguration.h>
#  endif // HAVE_SCDYNAMICSTORECOPYCOMPUTERNAME
#endif // __APPLE__


//
// 'httpAddrClose()' - Close a socket created by @link httpAddrConnect@ or
//                     @link httpAddrListen@.
//
// Pass `NULL` for sockets created with @link httpAddrConnect@ and the
// listen address for sockets created with @link httpAddrListen@.  This function
// ensures that domain sockets are removed when closed.
//

bool					// O - `true` on success, `false` on failure
httpAddrClose(http_addr_t *addr,	// I - Listen address or `NULL`
              int         fd)		// I - Socket file descriptor
{
#ifdef _WIN32
  if (closesocket(fd))
#else
  if (close(fd))
#endif // _WIN32
    return (false);

#ifdef AF_LOCAL
  if (addr && addr->addr.sa_family == AF_LOCAL)
    return (!unlink(addr->un.sun_path));
#endif // AF_LOCAL

  return (true);
}


//
// 'httpGetAddress()' - Get the address of the connected peer of a connection.
//
// For connections created with @link httpConnect2@, the address is for the
// server.  For connections created with @link httpAccept@, the address is for
// the client.
//
// Returns `NULL` if the socket is currently unconnected.
//

http_addr_t *				// O - Connected address or `NULL`
httpGetAddress(http_t *http)		// I - HTTP connection
{
  return (http ? http->hostaddr : NULL);
}


//
// 'httpAddrGetFamily()' - Get the address family of an address.
//

int					// O - Address family
httpAddrGetFamily(http_addr_t *addr)	// I - Address
{
  if (addr)
    return (addr->addr.sa_family);
  else
    return (0);
}


//
// 'httpAddrGetLength()' - Return the length of the address in bytes.
//

size_t					// O - Length in bytes
httpAddrGetLength(
    const http_addr_t *addr)		// I - Address
{
  if (!addr)
    return (0);

#ifdef AF_INET6
  if (addr->addr.sa_family == AF_INET6)
    return (sizeof(addr->ipv6));
  else
#endif // AF_INET6
#ifdef AF_LOCAL
  if (addr->addr.sa_family == AF_LOCAL)
    return (offsetof(struct sockaddr_un, sun_path) + strlen(addr->un.sun_path) + 1);
  else
#endif // AF_LOCAL
  if (addr->addr.sa_family == AF_INET)
    return (sizeof(addr->ipv4));
  else
    return (0);

}


//
// 'httpAddrGetPort()' - Get the port number associated with an address.
//

int					// O - Port number
httpAddrGetPort(http_addr_t *addr)	// I - Address
{
  if (!addr)
    return (-1);
#ifdef AF_INET6
  else if (addr->addr.sa_family == AF_INET6)
    return (ntohs(addr->ipv6.sin6_port));
#endif // AF_INET6
  else if (addr->addr.sa_family == AF_INET)
    return (ntohs(addr->ipv4.sin_port));
  else
    return (0);
}


//
// 'httpAddrGetString()' - Convert an address to a numeric string.
//

char *					// O - Numeric address string
httpAddrGetString(
    const http_addr_t *addr,		// I - Address to convert
    char              *s,		// I - String buffer
    size_t            slen)		// I - Length of string buffer
{
  DEBUG_printf("httpAddrGetString(addr=%p, s=%p, slen=%u)", (void *)addr, (void *)s, (unsigned)slen);

  // Range check input...
  if (!addr || !s || slen <= 2)
  {
    if (s && slen >= 1)
      *s = '\0';

    return (NULL);
  }

#ifdef AF_LOCAL
  if (addr->addr.sa_family == AF_LOCAL)
  {
    if (addr->un.sun_path[0] == '/')
      cupsCopyString(s, addr->un.sun_path, (size_t)slen);
    else
      cupsCopyString(s, "localhost", (size_t)slen);
  }
  else
#endif // AF_LOCAL
  if (addr->addr.sa_family == AF_INET)
  {
    unsigned temp;			// Temporary address

    temp = ntohl(addr->ipv4.sin_addr.s_addr);

    snprintf(s, (size_t)slen, "%d.%d.%d.%d", (temp >> 24) & 255, (temp >> 16) & 255, (temp >> 8) & 255, temp & 255);
  }
#ifdef AF_INET6
  else if (addr->addr.sa_family == AF_INET6)
  {
    char	*sptr,			// Pointer into string
		temps[64];		// Temporary string for address

    if (getnameinfo(&addr->addr, (socklen_t)httpAddrGetLength(addr), temps, sizeof(temps), NULL, 0, NI_NUMERICHOST))
    {
      // If we get an error back, then the address type is not supported
      // and we should zero out the buffer...
      s[0] = '\0';

      return (NULL);
    }
    else if ((sptr = strchr(temps, '%')) != NULL)
    {
      // Convert "%zone" to "+zone" to match URI form...
      *sptr = '+';
    }

    // Add "[v1." and "]" around IPv6 address to convert to URI form.
    snprintf(s, (size_t)slen, "[v1.%s]", temps);
  }
#endif // AF_INET6
  else
  {
    cupsCopyString(s, "UNKNOWN", (size_t)slen);
  }

  DEBUG_printf("2httpAddrGetString: returning \"%s\"...", s);

  return (s);
}


//
// 'httpAddrIsAny()' - Check for the "any" address.
//

bool					// O - `true` if "any", `false` otherwise
httpAddrIsAny(const http_addr_t *addr)	// I - Address to check
{
  if (!addr)
    return (false);

#ifdef AF_INET6
  if (addr->addr.sa_family == AF_INET6 && IN6_IS_ADDR_UNSPECIFIED(&(addr->ipv6.sin6_addr)))
    return (true);
#endif // AF_INET6

  if (addr->addr.sa_family == AF_INET && ntohl(addr->ipv4.sin_addr.s_addr) == 0x00000000)
    return (true);

  return (false);
}


//
// 'httpAddrIsEqual()' - Compare two addresses.
//

bool					// O - `true` if equal, `false` if not
httpAddrIsEqual(
    const http_addr_t *addr1,		// I - First address
    const http_addr_t *addr2)		// I - Second address
{
  if (!addr1 && !addr2)
    return (true);

  if (!addr1 || !addr2)
    return (false);

  if (addr1->addr.sa_family != addr2->addr.sa_family)
    return (false);

#ifdef AF_LOCAL
  if (addr1->addr.sa_family == AF_LOCAL)
    return (!strcmp(addr1->un.sun_path, addr2->un.sun_path));
#endif // AF_LOCAL

#ifdef AF_INET6
  if (addr1->addr.sa_family == AF_INET6)
    return (!memcmp(&(addr1->ipv6.sin6_addr), &(addr2->ipv6.sin6_addr), 16));
#endif // AF_INET6

  return (addr1->ipv4.sin_addr.s_addr == addr2->ipv4.sin_addr.s_addr);
}


//
// 'httpAddrIsLocalhost()' - Check for the local loopback address.
//

bool					// O - `true` if local host, `false` otherwise
httpAddrIsLocalhost(
    const http_addr_t *addr)		// I - Address to check
{
  if (!addr)
    return (true);

#ifdef AF_INET6
  if (addr->addr.sa_family == AF_INET6 && IN6_IS_ADDR_LOOPBACK(&(addr->ipv6.sin6_addr)))
    return (true);
#endif // AF_INET6

#ifdef AF_LOCAL
  if (addr->addr.sa_family == AF_LOCAL)
    return (true);
#endif // AF_LOCAL

  if (addr->addr.sa_family == AF_INET && (ntohl(addr->ipv4.sin_addr.s_addr) & 0xff000000) == 0x7f000000)
    return (true);

  return (false);
}


//
// 'httpAddrListen()' - Create a listening socket bound to the specified address and port.
//

int					// O - Socket or `-1` on error
httpAddrListen(http_addr_t *addr,	// I - Address to bind to
               int         port)	// I - Port number to bind to
{
  int		fd = -1,		// Socket
		val,			// Socket value
                status;			// Bind status


  // Range check input...
  if (!addr || port < 0)
    return (-1);

  // Make sure the network stack is initialized...
  httpInitialize();

  // Create the socket and set options...
  if ((fd = socket(addr->addr.sa_family, SOCK_STREAM, 0)) < 0)
  {
    _cupsSetError(IPP_STATUS_ERROR_INTERNAL, strerror(errno), false);
    return (-1);
  }

  val = 1;
  if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, CUPS_SOCAST &val, sizeof(val)))
    DEBUG_printf("2httpAddrListen: setsockopt(SO_REUSEADDR) failed: %s", strerror(errno));

#ifdef IPV6_V6ONLY
  if (addr->addr.sa_family == AF_INET6)
  {
    if (setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, CUPS_SOCAST &val, sizeof(val)))
      DEBUG_printf("2httpAddrListen: setsockopt(IPv6_V6ONLY) failed: %s", strerror(errno));
  }
#endif // IPV6_V6ONLY

  // Bind the socket...
#ifdef AF_LOCAL
  if (addr->addr.sa_family == AF_LOCAL)
  {
    mode_t	mask;			// Umask setting

    // Remove any existing domain socket file...
    if ((status = unlink(addr->un.sun_path)) < 0)
    {
      if (errno == ENOENT)
        status = 0;
      else
        DEBUG_printf("2httpAddrListen: Unable to unlink '%s': %s",
addr->un.sun_path, strerror(errno));
    }

    if (!status)
    {
      // Save the current umask and set it to 0 so that all users can access
      // the domain socket...
      mask = umask(0);

      // Bind the domain socket...
      if ((status = bind(fd, (struct sockaddr *)addr, (socklen_t)httpAddrGetLength(addr))) < 0)
	DEBUG_printf("1httpAddrListen: Unable to bind domain socket \"%s\": %s", addr->un.sun_path, strerror(errno));
 
      // Restore the umask and fix permissions...
      umask(mask);
    }
  }
  else
#endif // AF_LOCAL
  {
    httpAddrSetPort(addr, port);

    if ((status = bind(fd, (struct sockaddr *)addr, (socklen_t)httpAddrGetLength(addr))) < 0)
      DEBUG_printf("1httpAddrListen: Unable to bind network socket: %s", strerror(errno));
  }

  if (status)
  {
    _cupsSetError(IPP_STATUS_ERROR_INTERNAL, strerror(errno), false);

    close(fd);

    return (-1);
  }

  // Listen...
  if (listen(fd, INT_MAX))
  {
    DEBUG_printf("1httpAddrListen: Unable to listen on socket: %s", strerror(errno));
    _cupsSetError(IPP_STATUS_ERROR_INTERNAL, strerror(errno), false);

    close(fd);

    return (-1);
  }

  // Close on exec...
#ifndef _WIN32
  if (fcntl(fd, F_SETFD, fcntl(fd, F_GETFD) | FD_CLOEXEC))
    DEBUG_printf("2httpAddrListen: fcntl(F_SETFD, FD_CLOEXEC) failed: %s", strerror(errno));
#endif // !_WIN32

#ifdef SO_NOSIGPIPE
  // Disable SIGPIPE for this socket.
  val = 1;
  if (setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, CUPS_SOCAST &val, sizeof(val)))
    DEBUG_printf("2httpAddrListen: setsockopt(SO_NOSIGPIPE) failed: %s", strerror(errno));
#endif // SO_NOSIGPIPE

  return (fd);
}


//
// 'httpAddrLookup()' - Lookup the hostname associated with the address.
//

char *					// O - Host name
httpAddrLookup(
    const http_addr_t *addr,		// I - Address to lookup
    char              *name,		// I - Host name buffer
    size_t            namelen)		// I - Size of name buffer
{
  _cups_globals_t	*cg = _cupsGlobals();
					// Global data


  DEBUG_printf("httpAddrLookup(addr=%p, name=%p, namelen=%u)", (void *)addr, (void *)name, (unsigned)namelen);

  // Range check input...
  if (!addr || !name || namelen <= 2)
  {
    if (name && namelen >= 1)
      *name = '\0';

    return (NULL);
  }

#ifdef AF_LOCAL
  if (addr->addr.sa_family == AF_LOCAL)
  {
    cupsCopyString(name, addr->un.sun_path, (size_t)namelen);
    return (name);
  }
#endif // AF_LOCAL

  // Optimize lookups for localhost/loopback addresses...
  if (httpAddrIsLocalhost(addr))
  {
    cupsCopyString(name, "localhost", (size_t)namelen);
    return (name);
  }

#ifdef HAVE_RES_INIT
  // If the previous lookup failed, re-initialize the resolver to prevent
  // temporary network errors from persisting.  This *should* be handled by
  // the resolver libraries, but apparently the glibc folks do not agree.
  //
  // We set a flag at the end of this function if we encounter an error that
  // requires reinitialization of the resolver functions.  We then call
  // res_init() if the flag is set on the next call here or in httpAddrLookup().
  if (cg->need_res_init)
  {
    res_init();

    cg->need_res_init = 0;
  }
#endif // HAVE_RES_INIT

  // Fall back on httpAddrGetString if getnameinfo fails...
  int error = getnameinfo(&addr->addr, (socklen_t)httpAddrGetLength(addr), name, (socklen_t)namelen, NULL, 0, 0);

  if (error)
  {
    if (error == EAI_FAIL)
      cg->need_res_init = 1;

    return (httpAddrGetString(addr, name, namelen));
  }

  DEBUG_printf("2httpAddrLookup: returning \"%s\"...", name);

  return (name);
}


//
// 'httpGetHostname()' - Get the FQDN for the connection or local system.
//
// When "http" points to a connected socket, return the hostname or address that
// was used in the call to @link httpConnect@ or the address of the client for
// the connection from @link httpAcceptConnection@.
//
// When "http" is `NULL`, return the FQDN for the local system.
//

const char *				// O - FQDN for connection or system
httpGetHostname(http_t *http,		// I - HTTP connection or NULL
                char   *s,		// I - String buffer for name
                size_t slen)		// I - Size of buffer
{
  if (http)
  {
    // Return the connection address...
    if (!s || slen <= 1)
    {
      if (http->hostname[0] == '/')
	return ("localhost");
      else
	return (http->hostname);
    }
    else if (http->hostname[0] == '/')
    {
      cupsCopyString(s, "localhost", (size_t)slen);
    }
    else
    {
      cupsCopyString(s, http->hostname, (size_t)slen);
    }
  }
  else
  {
    // Return the hostname...
    if (!s || slen <= 1)
      return (NULL);

    if (gethostname(s, (size_t)slen) < 0)
      cupsCopyString(s, "localhost", (size_t)slen);

    DEBUG_printf("1httpGetHostname: gethostname() returned \"%s\".", s);

    if (!strchr(s, '.'))
    {
#ifdef HAVE_SCDYNAMICSTORECOPYCOMPUTERNAME
      // The hostname is not a FQDN, so use the local hostname from the
      // SystemConfiguration framework...
      SCDynamicStoreRef	sc = SCDynamicStoreCreate(kCFAllocatorDefault, CFSTR("libcups"), NULL, NULL);
					// System configuration data
      CFStringRef	local = sc ? SCDynamicStoreCopyLocalHostName(sc) : NULL;
					// Local host name
      char		localStr[1024];	// Local host name C string

      if (local && CFStringGetCString(local, localStr, sizeof(localStr), kCFStringEncodingUTF8))
      {
        // Append ".local." to the hostname we get...
        snprintf(s, (size_t)slen, "%s.local.", localStr);
        DEBUG_printf("1httpGetHostname: SCDynamicStoreCopyLocalHostName() returned \"%s\".", s);
      }

      if (local)
        CFRelease(local);
      if (sc)
        CFRelease(sc);

#else
      // The hostname is not a FQDN, so look it up...
      struct hostent	*host;		// Host entry to get FQDN

      if ((host = gethostbyname(s)) != NULL && host->h_name)
      {
        // Use the resolved hostname...
	cupsCopyString(s, host->h_name, (size_t)slen);
        DEBUG_printf("1httpGetHostname: gethostbyname() returned \"%s\".", s);
      }
#endif // HAVE_SCDYNAMICSTORECOPYCOMPUTERNAME
    }

    // Make sure .local hostnames end with a period...
    if (strlen(s) > 6 && !strcmp(s + strlen(s) - 6, ".local"))
      cupsConcatString(s, ".", (size_t)slen);
  }

  // Convert the hostname to lowercase as needed...
  if (s[0] != '/')
  {
    char	*ptr;			// Pointer into string

    for (ptr = s; *ptr; ptr ++)
      *ptr = (char)_cups_tolower((int)*ptr);
  }

  // Return the hostname with as much domain info as we have...
  return (s);
}


//
// 'httpResolveHostname()' - Resolve the hostname of the HTTP connection
//                           address.
//

const char *				// O - Resolved hostname or `NULL`
httpResolveHostname(http_t *http,	// I - HTTP connection
                    char   *buffer,	// I - Hostname buffer or `NULL` to use HTTP buffer
                    size_t bufsize)	// I - Size of buffer
{
  if (!http)
    return (NULL);

  if (isdigit(http->hostname[0] & 255) || http->hostname[0] == '[')
  {
    char	temp[1024];		// Temporary string

    if (httpAddrLookup(http->hostaddr, temp, sizeof(temp)))
      cupsCopyString(http->hostname, temp, sizeof(http->hostname));
    else
      return (NULL);
  }

  if (buffer)
  {
    if (http->hostname[0] == '/')
      cupsCopyString(buffer, "localhost", bufsize);
    else
      cupsCopyString(buffer, http->hostname, bufsize);

    return (buffer);
  }
  else if (http->hostname[0] == '/')
  {
    return ("localhost");
  }
  else
  {
    return (http->hostname);
  }
}


//
// 'httpAddrSetPort()' - Set the port number associated with an address.
//

void
httpAddrSetPort(http_addr_t *addr,	// I - Address
                int         port)	// I - Port
{
  if (!addr || port <= 0)
    return;

#ifdef AF_INET6
  if (addr->addr.sa_family == AF_INET6)
    addr->ipv6.sin6_port = htons(port);
  else
#endif // AF_INET6
  if (addr->addr.sa_family == AF_INET)
    addr->ipv4.sin_port = htons(port);
}
