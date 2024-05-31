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
// Local functions...
//

static char *oauth_make_path(char *buffer, size_t bufsize, const char *auth_server, const char *res_server, const char *type);


//
// 'cupsOAuthClearTokens()' - Clear any cached authorization or refresh tokens.
//

void
cupsOAuthClearTokens(
    const char *auth_server,		// I - Authorization server FQDN
    const char *res_server)		// I - Resource server FQDN
{
  char	filename[1024];			// Token filename


  if (oauth_make_path(filename, sizeof(filename), auth_server, res_server, "auth"))
    unlink(filename);

  if (oauth_make_path(filename, sizeof(filename), auth_server, res_server, "rfsh"))
    unlink(filename);
}


//
// 'cupsOAuthGetAuthToken()' - Get a cached authorization token.
//

const char *				// O - Authorization token
cupsOAuthGetAuthToken(
    const char *auth_server,		// I - Authorization server FQDN
    const char *res_server,		// I - Resource server FQDN
    char       *token,			// I - Token buffer
    size_t     toksize)			// I - Size of token buffer
{
  char		filename[1024];		// Token filename
  int		fd;			// File descriptor
  ssize_t	bytes;			// Bytes read


  // Range check input...
  if (token)
    *token = '\0';
 
  if (!token || toksize < 256)
    return (NULL);
 
  // See if we have a token file...
  if (oauth_make_path(filename, sizeof(filename), auth_server, res_server, "auth") && (fd = open(filename, O_RDONLY)) >= 0)
  {
    if ((bytes = read(fd, token, toksize - 1)) < 0)
      bytes = 0;

    close(fd);
    token[bytes] = '\0';
  }
 
  return (*token ? token : NULL);
}


//
// 'cupsOAuthGetRefreshToken()' - Get a cached refresh token.
//

const char *				// O - Refresh token
cupsOAuthGetRefreshToken(
    const char *auth_server,		// I - Authorization server FQDN
    const char *res_server,		// I - Resource server FQDN
    char       *token,			// I - Token buffer
    size_t     toksize)			// I - Size of token buffer
{
  char		filename[1024];		// Token filename
  int		fd;			// File descriptor
  ssize_t	bytes;			// Bytes read


  // Range check input...
  if (token)
    *token = '\0';
 
  if (!token || toksize < 256)
    return (NULL);
 
  // See if we have a token file...
  if (oauth_make_path(filename, sizeof(filename), auth_server, res_server, "rfsh") && (fd = open(filename, O_RDONLY)) >= 0)
  {
    if ((bytes = read(fd, token, toksize - 1)) < 0)
      bytes = 0;

    close(fd);
    token[bytes] = '\0';
  }
 
  return (*token ? token : NULL);
}


//
// 'cupsOAuthSetTokens()' - Save authorization and refresh tokens.
//

void
cupsOAuthSetTokens(
    const char *auth_server,		// I - Authorization server FQDN
    const char *res_server,		// I - Resource server FQDN
    const char *auth_token,		// I - Authorization token
    const char *refresh_token)		// I - Refresh token
{
  char		filename[1024];		// Token filename
  int		fd;			// File descriptor


  if (oauth_make_path(filename, sizeof(filename), auth_server, res_server, "auth"))
  {
    if (auth_token)
    {
      // Save authorization token...
      if ((fd = open(filename, O_CREAT | O_TRUNC | O_WRONLY | O_NOFOLLOW, 0600)) >= 0)
      {
        write(fd, auth_token, strlen(auth_token));
	close(fd);
      }
    }
    else
    {
      // Clear authorization token...
      unlink(filename);
    }
  }

  if (oauth_make_path(filename, sizeof(filename), auth_server, res_server, "rfsh"))
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
    char       *buffer,			// I - Filename buffer
    size_t     bufsize,			// I - Size of filename buffer
    const char *auth_server,		// I - Authorization server FQDN
    const char *res_server,		// I - Resource server FQDN
    const char *type)			// I - Type ("auth" or "rfsh")
{
  _cups_globals_t	*cg = _cupsGlobals();
					// Global data


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

  snprintf(buffer, bufsize, "%s/oauth/%s-%s.%s", cg->userconfig, auth_server, res_server, type);

  return (buffer);
}
