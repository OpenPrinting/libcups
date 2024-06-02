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
    "rfsh"
  };


  // Range check input...
  if (!auth_server || !res_server)
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

  snprintf(buffer, bufsize, "%s/oauth/%s-%s.%s", cg->userconfig, auth_server, res_server, otypes[otype]);

  return (buffer);
}
