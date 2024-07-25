//
// Get/put file functions for CUPS.
//
// Copyright © 2021-2024 by OpenPrinting.
// Copyright © 2007-2018 by Apple Inc.
// Copyright © 1997-2006 by Easy Software Products.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#include "cups-private.h"
#include <fcntl.h>
#include <sys/stat.h>
#if defined(_WIN32) || defined(__EMX__)
#  include <io.h>
#else
#  include <unistd.h>
#endif // _WIN32 || __EMX__


//
// 'cupsGetFd()' - Get a file from the server.
//
// This function returns @code HTTP_STATUS_OK@ when the file is successfully retrieved.
//

http_status_t				// O - HTTP status
cupsGetFd(http_t     *http,		// I - Connection to server or @code CUPS_HTTP_DEFAULT@
	  const char *resource,		// I - Resource name
	  int        fd)		// I - File descriptor
{
  ssize_t	bytes;			// Number of bytes read
  char		buffer[8192];		// Buffer for file
  http_status_t	status;			// HTTP status from server
  char		if_modified_since[HTTP_MAX_VALUE];
					// If-Modified-Since header
  int		new_auth = 0;		// Using new auth information?
  int		digest;			// Are we using Digest authentication?


  // Range check input...
  DEBUG_printf("cupsGetFd(http=%p, resource=\"%s\", fd=%d)", (void *)http, resource, fd);

  if (!resource || fd < 0)
  {
    if (http)
      http->error = EINVAL;

    return (HTTP_STATUS_ERROR);
  }

  if (!http)
  {
    if ((http = _cupsConnect()) == NULL)
      return (HTTP_STATUS_SERVICE_UNAVAILABLE);
  }

  // Then send GET requests to the HTTP server...
  cupsCopyString(if_modified_since, httpGetField(http, HTTP_FIELD_IF_MODIFIED_SINCE), sizeof(if_modified_since));

  do
  {
    if (!_cups_strcasecmp(httpGetField(http, HTTP_FIELD_CONNECTION), "close"))
    {
      httpClearFields(http);
      if (!httpConnectAgain(http, 30000, NULL))
      {
	status = HTTP_STATUS_ERROR;
	break;
      }
    }

    httpClearFields(http);
    httpSetField(http, HTTP_FIELD_IF_MODIFIED_SINCE, if_modified_since);

    digest = http->authstring && !strncmp(http->authstring, "Digest ", 7);

    if (digest && !new_auth)
    {
      // Update the Digest authentication string...
      _httpSetDigestAuthString(http, http->nextnonce, "GET", resource);
    }

    httpSetField(http, HTTP_FIELD_AUTHORIZATION, http->authstring);

    if (!httpWriteRequest(http, "GET", resource))
    {
      if (httpConnectAgain(http, 30000, NULL))
      {
        status = HTTP_STATUS_UNAUTHORIZED;
        continue;
      }
      else
      {
        status = HTTP_STATUS_ERROR;
	break;
      }
    }

    new_auth = 0;

    while ((status = httpUpdate(http)) == HTTP_STATUS_CONTINUE);

    if (status == HTTP_STATUS_UNAUTHORIZED)
    {
      // Flush any error message...
      httpFlush(http);

      // See if we can do authentication...
      new_auth = 1;

      if (!cupsDoAuthentication(http, "GET", resource))
      {
        status = HTTP_STATUS_CUPS_AUTHORIZATION_CANCELED;
        break;
      }

      if (!httpConnectAgain(http, 30000, NULL))
      {
        status = HTTP_STATUS_ERROR;
        break;
      }

      continue;
    }
    else if (status == HTTP_STATUS_UPGRADE_REQUIRED)
    {
      // Flush any error message...
      httpFlush(http);

      // Reconnect...
      if (!httpConnectAgain(http, 30000, NULL))
      {
        status = HTTP_STATUS_ERROR;
        break;
      }

      // Upgrade with encryption...
      httpSetEncryption(http, HTTP_ENCRYPTION_REQUIRED);

      // Try again, this time with encryption enabled...
      continue;
    }
  }
  while (status == HTTP_STATUS_UNAUTHORIZED || status == HTTP_STATUS_UPGRADE_REQUIRED);

  // See if we actually got the file or an error...
  if (status == HTTP_STATUS_OK)
  {
    // Yes, copy the file...
    while ((bytes = httpRead(http, buffer, sizeof(buffer))) > 0)
      write(fd, buffer, (size_t)bytes);
  }
  else
  {
    _cupsSetHTTPError(http, status);
    httpFlush(http);
  }

  // Return the request status...
  DEBUG_printf("1cupsGetFd: Returning %d...", status);

  return (status);
}


//
// 'cupsGetFile()' - Get a file from the server.
//
// This function returns @code HTTP_STATUS_OK@ when the file is successfully retrieved.
//

http_status_t				// O - HTTP status
cupsGetFile(http_t     *http,		// I - Connection to server or @code CUPS_HTTP_DEFAULT@
	    const char *resource,	// I - Resource name
	    const char *filename)	// I - Filename
{
  int		fd;			// File descriptor
  http_status_t	status;			// Status


  // Range check input...
  if (!http || !resource || !filename)
  {
    _cupsSetError(IPP_STATUS_ERROR_INTERNAL, strerror(EINVAL), false);

    if (http)
      http->error = EINVAL;

    return (HTTP_STATUS_ERROR);
  }

  // Create the file...
  if ((fd = open(filename, O_WRONLY | O_EXCL | O_TRUNC)) < 0)
  {
    // Couldn't open the file!
    _cupsSetError(IPP_STATUS_ERROR_INTERNAL, strerror(errno), false);

    http->error = errno;

    return (HTTP_STATUS_ERROR);
  }

  // Get the file...
  status = cupsGetFd(http, resource, fd);

  // If the file couldn't be gotten, then remove the file...
  close(fd);

  if (status != HTTP_STATUS_OK)
  {
    _cupsSetHTTPError(http, status);
    unlink(filename);
  }

  // Return the HTTP status code...
  return (status);
}


//
// 'cupsPutFd()' - Put a file on the server.
//
// This function returns @code HTTP_STATUS_CREATED@ when the file is stored
// successfully.
//

http_status_t				// O - HTTP status
cupsPutFd(http_t     *http,		// I - Connection to server or @code CUPS_HTTP_DEFAULT@
          const char *resource,		// I - Resource name
	  int        fd)		// I - File descriptor
{
  ssize_t	bytes;			// Number of bytes read
  int		retries;		// Number of retries
  char		buffer[8192];		// Buffer for file
  http_status_t	status;			// HTTP status from server
  int		new_auth = 0;		// Using new auth information?
  int		digest;			// Are we using Digest authentication?


  // Range check input...
  DEBUG_printf("cupsPutFd(http=%p, resource=\"%s\", fd=%d)", (void *)http, resource, fd);

  if (!resource || fd < 0)
  {
    _cupsSetError(IPP_STATUS_ERROR_INTERNAL, strerror(EINVAL), false);

    if (http)
      http->error = EINVAL;

    return (HTTP_STATUS_ERROR);
  }

  if (!http)
  {
    if ((http = _cupsConnect()) == NULL)
      return (HTTP_STATUS_SERVICE_UNAVAILABLE);
  }

  // Then send PUT requests to the HTTP server...
  retries = 0;

  do
  {
    if (!_cups_strcasecmp(httpGetField(http, HTTP_FIELD_CONNECTION), "close"))
    {
      httpClearFields(http);
      if (!httpConnectAgain(http, 30000, NULL))
      {
	status = HTTP_STATUS_ERROR;
	break;
      }
    }

    DEBUG_printf("2cupsPutFd: starting attempt, authstring=\"%s\"...", http->authstring);

    httpClearFields(http);
    httpSetField(http, HTTP_FIELD_TRANSFER_ENCODING, "chunked");
    httpSetExpect(http, HTTP_STATUS_CONTINUE);

    digest = http->authstring && !strncmp(http->authstring, "Digest ", 7);

    if (digest && !new_auth)
    {
      // Update the Digest authentication string...
      _httpSetDigestAuthString(http, http->nextnonce, "PUT", resource);
    }

    httpSetField(http, HTTP_FIELD_AUTHORIZATION, http->authstring);

    if (!httpWriteRequest(http, "PUT", resource))
    {
      if (httpConnectAgain(http, 30000, NULL))
      {
        status = HTTP_STATUS_UNAUTHORIZED;
        continue;
      }
      else
      {
        status = HTTP_STATUS_ERROR;
	break;
      }
    }

    // Wait up to 1 second for a 100-continue response...
    if (httpWait(http, 1000))
      status = httpUpdate(http);
    else
      status = HTTP_STATUS_CONTINUE;

    if (status == HTTP_STATUS_CONTINUE)
    {
      // Copy the file...
      lseek(fd, 0, SEEK_SET);

      while ((bytes = read(fd, buffer, sizeof(buffer))) > 0)
      {
	if (httpWait(http, 0))
	{
          if ((status = httpUpdate(http)) != HTTP_STATUS_CONTINUE)
            break;
	}
	else if (httpWrite(http, buffer, (size_t)bytes) < 0)
	{
	  status = HTTP_STATUS_ERROR;
	  break;
	}
      }
    }

    if (status == HTTP_STATUS_CONTINUE)
    {
      // Write 0-length chunk...
      if (httpWrite(http, buffer, 0) < 0)
      {
        status = HTTP_STATUS_ERROR;
      }
      else
      {
        // Wait for response...
        while ((status = httpUpdate(http)) == HTTP_STATUS_CONTINUE);
      }
    }

    if (status == HTTP_STATUS_ERROR && !retries)
    {
      DEBUG_printf("2cupsPutFd: retry on status %d", status);

      retries ++;
      status = HTTP_STATUS_NONE;

      // Flush any error message...
      httpFlush(http);

      // Reconnect...
      if (!httpConnectAgain(http, 30000, NULL))
      {
        status = HTTP_STATUS_ERROR;
        break;
      }

      // Try again...
      continue;
    }

    DEBUG_printf("2cupsPutFd: status=%d", status);

    new_auth = 0;

    if (status == HTTP_STATUS_UNAUTHORIZED)
    {
      // Flush any error message...
      httpFlush(http);

      // See if we can do authentication...
      new_auth = 1;

      if (!cupsDoAuthentication(http, "PUT", resource))
      {
        status = HTTP_STATUS_CUPS_AUTHORIZATION_CANCELED;
        break;
      }

      if (!httpConnectAgain(http, 30000, NULL))
      {
        status = HTTP_STATUS_ERROR;
        break;
      }

      continue;
    }
    else if (status == HTTP_STATUS_UPGRADE_REQUIRED)
    {
      // Flush any error message...
      httpFlush(http);

      // Reconnect...
      if (!httpConnectAgain(http, 30000, NULL))
      {
        status = HTTP_STATUS_ERROR;
        break;
      }

      // Upgrade with encryption...
      httpSetEncryption(http, HTTP_ENCRYPTION_REQUIRED);

      // Try again, this time with encryption enabled...
      continue;
    }
  }
  while (status == HTTP_STATUS_UNAUTHORIZED || status == HTTP_STATUS_UPGRADE_REQUIRED || status == HTTP_STATUS_NONE);

  // See if we actually put the file or an error...
  if (status != HTTP_STATUS_CREATED)
  {
    _cupsSetHTTPError(http, status);
    httpFlush(http);
  }

  DEBUG_printf("1cupsPutFd: Returning %d...", status);

  return (status);
}


//
// 'cupsPutFile()' - Put a file on the server.
//
// This function returns @code HTTP_CREATED@ when the file is stored
// successfully.
//

http_status_t				// O - HTTP status
cupsPutFile(http_t     *http,		// I - Connection to server or @code CUPS_HTTP_DEFAULT@
            const char *resource,	// I - Resource name
	    const char *filename)	// I - Filename
{
  int		fd;			// File descriptor
  http_status_t	status;			// Status


  // Range check input...
  if (!http || !resource || !filename)
  {
    _cupsSetError(IPP_STATUS_ERROR_INTERNAL, strerror(EINVAL), false);

    if (http)
      http->error = EINVAL;

    return (HTTP_STATUS_ERROR);
  }

  // Open the local file...
  if ((fd = open(filename, O_RDONLY)) < 0)
  {
    // Couldn't open the file!
    _cupsSetError(IPP_STATUS_ERROR_INTERNAL, strerror(errno), false);

    http->error = errno;

    return (HTTP_STATUS_ERROR);
  }

  // Put the file...
  status = cupsPutFd(http, resource, fd);

  close(fd);

  _cupsSetHTTPError(http, status);

  return (status);
}
