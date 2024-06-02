//
// OAuth API implementation for CUPS.
//
// Copyright © 2024 by OpenPrinting.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#include "cups-private.h"
#include "oauth.h"
#include <sys/stat.h>


//
// Local types...
//

typedef enum _cups_otype_e
{
  _CUPS_OTYPE_AUTH,
  _CUPS_OTYPE_METADATA,
  _CUPS_OTYPE_REFRESH
} _cups_otype_t;


//
// Local functions...
//

static char *oauth_make_path(char *buffer, size_t bufsize, const char *auth_server, const char *res_server, _cups_otype_t otype);


//
// 'cupsOAuthClearTokens()' - Clear any cached authorization or refresh tokens.
//

void
cupsOAuthClearTokens(
    const char *auth_server,		// I - Authorization server FQDN
    const char *res_server)		// I - Resource server FQDN
{
  char	filename[1024];			// Token filename


  if (oauth_make_path(filename, sizeof(filename), auth_server, res_server, _CUPS_OTYPE_AUTH))
    unlink(filename);

  if (oauth_make_path(filename, sizeof(filename), auth_server, res_server, _CUPS_OTYPE_REFRESH))
    unlink(filename);
}


//
// 'cupsOAuthCopyAuthToken()' - Get a cached authorization token.
//
// This function makes a copy of a cached authorization token and any
// associated expiration time for the given authorization and resource
// server combination.  The returned authorization token must be freed
// using the `free` function.  `NULL` is returned if no token is cached.
//

char *					// O - Authorization token
cupsOAuthGetAuthToken(
    const char *auth_server,		// I - Authorization server FQDN
    const char *res_server,		// I - Resource server FQDN
    time_t     *auth_expires)		// O - Expiration time of token or `NULL` for don't care
{
  char		filename[1024],		// Token filename
		buffer[1024],		// Token buffer
		*bufptr,		// Pointer into token buffer
		*auth_token = NULL;	// Authorization token
  int		fd;			// File descriptor
  ssize_t	bytes;			// Bytes read


  // Range check input...
  if (auth_expires)
    *auth_expires = 0;
 
  // See if we have a token file...
  if (oauth_make_path(filename, sizeof(filename), auth_server, res_server, _CUPS_OTYPE_AUTH) && (fd = open(filename, O_RDONLY)) >= 0)
  {
    if ((bytes = read(fd, buffer, sizeof(buffer) - 1)) < 0)
      bytes = 0;

    close(fd);

    buffer[bytes] = '\0';
    if ((bufptr = strchr(buffer, '\n')) != NULL)
    {
      *bufptr++ = '\0';

      if (auth_expires)
        *auth_expires = strtol(bufptr, NULL, 10);
    }

    auth_token = strdup(buffer);
  }
 
  return (auth_token);
}


//
// 'cupsOAuthCopyMetadata()' - Get the metadata for an authorization server.
//

cups_json_t *				// O - JSON metadata or `NULL` on error
cupsOAuthCopyMetadata(
    const char *auth_server)		// I - Authorization server URI
{
  char		filename[1024];		// Local metadata filename
  struct stat	fileinfo;		// Local metadata file info
  char		filedate[256],		// Local metadata modification date
		scheme[32],		// URI scheme
		userpass[256],		// Username:password data (not used)
		host[256],		// Hostname
		resource[256];		// Resource path
  int		port;			// Port to use
  http_encryption_t encryption;		// Type of encryption to use
  http_t	*http;			// Connection to server
  http_status_t	status;			// Request status
  size_t	i;			// Looping var
  static const char * const paths[] =	// Metadata paths
  {
    "/.well-known/oauth-authorization-server",
    "/.well-known/openid-configuration"
  };


  // Get existing metadata...
  if (!oauth_make_path(filename, sizeof(filename), auth_server, /*resource_server*/NULL, _CUPS_OTYPE_METADATA))
    return (NULL);

  if (stat(filename, &fileinfo))
    memset(&fileinfo, 0, sizeof(fileinfo));

  if (fileinfo.st_mtime)
    httpGetDateString(fileinfo.st_mtime, filedate, sizeof(filedate));
  else
    filedate[0] = '\0';

  // Don't bother connecting if the metadata was updated recently...
  if ((time(NULL) - fileinfo.st_mtime) <= 60)
    goto load_metadata;

  // Try getting the metadata...
  if (httpSeparateURI(HTTP_URI_CODING_ALL, auth_server, scheme, sizeof(scheme), userpass, sizeof(userpass), host, sizeof(host), &port, resource, sizeof(resource)) < HTTP_URI_STATUS_OK)
  {
    _cupsSetError(IPP_STATUS_ERROR_INTERNAL, _("Bad authorization server URI."), true);
    return (NULL);
  }

  if (!strcmp(scheme, "https") || port == 443)
    encryption = HTTP_ENCRYPTION_ALWAYS;
  else
    encryption = HTTP_ENCRYPTION_IF_REQUESTED;

  if ((http = httpConnect(host, port, /*addrlist*/NULL, AF_UNSPEC, encryption, /*blocking*/true, /*msec*/30000, /*cancel*/NULL)) == NULL)
    return (NULL);

  for (i = 0; i < (sizeof(paths) / sizeof(paths[0])); i ++)
  {
    cupsCopyString(resource, paths[i], sizeof(resource));

    do
    {
      if (!_cups_strcasecmp(httpGetField(http, HTTP_FIELD_CONNECTION), "close"))
      {
        httpClearFields(http);
        if (!httpReconnect(http, /*msec*/30000, /*cancel*/NULL))
        {
	  status = HTTP_STATUS_ERROR;
	  break;
        }
      }

      httpClearFields(http);

      httpSetField(http, HTTP_FIELD_IF_MODIFIED_SINCE, filedate);
      if (!httpWriteRequest(http, "GET", resource))
      {
        if (httpReconnect(http, 30000, NULL))
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

      while ((status = httpUpdate(http)) == HTTP_STATUS_CONTINUE)
        ;

      if (status >= HTTP_STATUS_MULTIPLE_CHOICES && status <= HTTP_STATUS_SEE_OTHER)
      {
        // Redirect
	char	lscheme[32],		// Location scheme
		luserpass[256],		// Location user:password (not used)
		lhost[256],		// Location hostname
		lresource[256];		// Location resource path
	int	lport;			// Location port

        if (httpSeparateURI(HTTP_URI_CODING_ALL, httpGetField(http, HTTP_FIELD_LOCATION), lscheme, sizeof(lscheme), luserpass, sizeof(luserpass), lhost, sizeof(lhost), &lport, lresource, sizeof(lresource)) < HTTP_URI_STATUS_OK)
	  break;			// Don't redirect to an invalid URI

        if (strcmp(scheme, lscheme) || _cups_strcasecmp(host, lhost) || port != lport)
	  break;			// Don't redirect off this host

        // Redirect to a local resource...
        cupsCopyString(resource, lresource, sizeof(resource));
      }
    }
    while (status >= HTTP_STATUS_MULTIPLE_CHOICES && status <= HTTP_STATUS_SEE_OTHER);

    if (status == HTTP_STATUS_NOT_MODIFIED)
    {
      // Metadata isn't changed, stop now...
      break;
    }
    else if (status == HTTP_STATUS_OK)
    {
      // Copy the metadata to the file...
      int	fd;			// Local metadata file
      char	buffer[8192];		// Copy buffer
      size_t	bytes;			// Bytes read

      if ((fd = open(filename, O_CREAT | O_TRUNC | O_WRONLY | O_NOFOLLOW, 0600)) < 0)
      {
        httpFlush(http);
        break;
      }

      while ((bytes = httpRead(http, buffer, sizeof(buffer))) > 0)
        write(fd, buffer, (size_t)bytes);

      close(fd);
      break;
    }
  }

  if (status != HTTP_STATUS_OK && status != HTTP_STATUS_NOT_MODIFIED)
  {
    // Remove old cached data...
    unlink(filename);
  }

  httpClose(http);

  // Return the cached metadata, if any...
  load_metadata:

  return (cupsJSONImportFile(filename));
}


//
// 'cupsOAuthCopyRefreshToken()' - Get a cached refresh token.
//
// This function makes a copy of a cached refresh token for the given
// authorization and resource server combination.  The returned refresh
// token must be freed using the `free` function.  `NULL` is returned
// if no token is cached.
//

char *					// O - Refresh token
cupsOAuthCopyRefreshToken(
    const char *auth_server,		// I - Authorization server FQDN
    const char *res_server)		// I - Resource server FQDN
{
  char		filename[1024],		// Token filename
		buffer[1024],		// Token buffer
		*bufptr,		// Pointer into token buffer
		*refresh_token = NULL;	// Refresh token
  int		fd;			// File descriptor
  ssize_t	bytes;			// Bytes read


  // See if we have a token file...
  if (oauth_make_path(filename, sizeof(filename), auth_server, res_server, _CUPS_OTYPE_REFRESH) && (fd = open(filename, O_RDONLY)) >= 0)
  {
    if ((bytes = read(fd, buffer, sizeof(buffer) - 1)) < 0)
      bytes = 0;

    close(fd);

    buffer[bytes] = '\0';
    if ((bufptr = strchr(buffer, '\n')) != NULL)
      *bufptr++ = '\0';

    refresh_token = strdup(buffer);
  }
 
  return (refresh_token);
}


//
// 'cupsOAuthSetTokens()' - Save authorization and refresh tokens.
//

void
cupsOAuthSetTokens(
    const char *auth_server,		// I - Authorization server FQDN
    const char *res_server,		// I - Resource server FQDN
    const char *auth_token,		// I - Authorization token
    time_t     auth_expires,		// I - Expiration time for authorization token
    const char *refresh_token)		// I - Refresh token
{
  char		filename[1024],		// Token filename
		temp[256];		// Temporary string
  int		fd;			// File descriptor


  if (oauth_make_path(filename, sizeof(filename), auth_server, res_server, _CUPS_OTYPE_AUTH))
  {
    if (auth_token)
    {
      // Save authorization token...
      if ((fd = open(filename, O_CREAT | O_TRUNC | O_WRONLY | O_NOFOLLOW, 0600)) >= 0)
      {
        write(fd, auth_token, strlen(auth_token));
	snprintf(temp, sizeof(temp), "\n%ld\n", (long)auth_expires);
        write(fd, temp, strlen(temp));
	close(fd);
      }
    }
    else
    {
      // Clear authorization token...
      unlink(filename);
    }
  }

  if (oauth_make_path(filename, sizeof(filename), auth_server, res_server, _CUPS_OTYPE_REFRESH))
  {
    if (refresh_token)
    {
      // Save refresh token...
      if ((fd = open(filename, O_CREAT | O_TRUNC | O_WRONLY | O_NOFOLLOW, 0600)) >= 0)
      {
        write(fd, refresh_token, strlen(refresh_token));
	close(fd);
      }
    }
    else
    {
      // Clear refresh token...
      unlink(filename);
    }
  }
}


//
// 'oauth_make_path()' - Make an OAuth token path.
//

static char *				// O - Filename
oauth_make_path(
    char          *buffer,		// I - Filename buffer
    size_t        bufsize,		// I - Size of filename buffer
    const char    *auth_server,		// I - Authorization server FQDN
    const char    *res_server,		// I - Resource server FQDN
    _cups_otype_t otype)		// I - Type (_CUPS_OTYPE_AUTH or _CUPS_OTYPE_REFRESH)
{
  _cups_globals_t	*cg = _cupsGlobals();
					// Global data
  static const char * const otypes[] =	// Filename extensions for each type
  {
    "auth",
    "meta",
    "rfsh"
  };


  // Range check input...
  if (!auth_server || (!res_server && otype != _CUPS_OTYPE_METADATA))
  {
    *buffer = '\0';
    return (NULL);
  }

  // First make sure the "oauth" directory exists...
  if (mkdir(cg->userconfig, 0700) && errno != EEXIST)
  {
    *buffer = '\0';
    return (NULL);
  }

  snprintf(buffer, bufsize, "%s/oauth", cg->userconfig);
  if (mkdir(buffer, 0700) && errno != EEXIST)
  {
    *buffer = '\0';
    return (NULL);
  }

  if (otype == _CUPS_OTYPE_METADATA)
    snprintf(buffer, bufsize, "%s/oauth/%s.%s", cg->userconfig, auth_server, otypes[otype]);
  else
    snprintf(buffer, bufsize, "%s/oauth/%s-%s.%s", cg->userconfig, auth_server, res_server, otypes[otype]);

  return (buffer);
}
