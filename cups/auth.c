/*
 * Authentication functions for CUPS.
 *
 * Copyright © 2021 by OpenPrinting.
 * Copyright © 2007-2019 by Apple Inc.
 * Copyright © 1997-2007 by Easy Software Products.
 *
 * Licensed under Apache License v2.0.  See the file "LICENSE" for more
 * information.
 */

/*
 * Include necessary headers...
 */

#include "cups-private.h"
#include "debug-internal.h"
#include <fcntl.h>
#include <sys/stat.h>
#if defined(_WIN32) || defined(__EMX__)
#  include <io.h>
#else
#  include <unistd.h>
#endif /* _WIN32 || __EMX__ */

#if defined(SO_PEERCRED) && defined(AF_LOCAL)
#  include <pwd.h>
#endif /* SO_PEERCRED && AF_LOCAL */


/*
 * Local functions...
 */

static const char	*cups_auth_find(const char *www_authenticate, const char *scheme);
static const char	*cups_auth_param(const char *scheme, const char *name, char *value, size_t valsize);
static const char	*cups_auth_scheme(const char *www_authenticate, char *scheme, size_t schemesize);

static int	cups_is_local_connection(http_t *http);
static int	cups_local_auth(http_t *http);


/*
 * 'cupsDoAuthentication()' - Authenticate a request.
 *
 * This function should be called in response to a @code HTTP_STATUS_UNAUTHORIZED@
 * status, prior to resubmitting your request.
 *
 * @since CUPS 1.1.20@
 */

int					/* O - 0 on success, -1 on error */
cupsDoAuthentication(
    http_t     *http,			/* I - Connection to server or @code CUPS_HTTP_DEFAULT@ */
    const char *method,			/* I - Request method ("GET", "POST", "PUT") */
    const char *resource)		/* I - Resource path */
{
  const char	*password,		/* Password string */
		*www_auth,		/* WWW-Authenticate header */
		*schemedata;		/* Scheme-specific data */
  char		scheme[256],		/* Scheme name */
		prompt[1024];		/* Prompt for user */
  int		localauth;		/* Local authentication result */
  _cups_globals_t *cg = _cupsGlobals();	/* Global data */


  DEBUG_printf(("cupsDoAuthentication(http=%p, method=\"%s\", resource=\"%s\")", (void *)http, method, resource));

  if (!http)
    http = _cupsConnect();

  if (!http || !method || !resource)
    return (-1);

  DEBUG_printf(("2cupsDoAuthentication: digest_tries=%d, userpass=\"%s\"",
                http->digest_tries, http->userpass));
  DEBUG_printf(("2cupsDoAuthentication: WWW-Authenticate=\"%s\"",
                httpGetField(http, HTTP_FIELD_WWW_AUTHENTICATE)));

 /*
  * Clear the current authentication string...
  */

  httpSetAuthString(http, NULL, NULL);

 /*
  * See if we can do local authentication...
  */

  if (http->digest_tries < 3)
  {
    if ((localauth = cups_local_auth(http)) == 0)
    {
      DEBUG_printf(("2cupsDoAuthentication: authstring=\"%s\"",
                    http->authstring));

      if (http->status == HTTP_STATUS_UNAUTHORIZED)
	http->digest_tries ++;

      return (0);
    }
    else if (localauth == -1)
    {
      http->status = HTTP_STATUS_CUPS_AUTHORIZATION_CANCELED;
      return (-1);			/* Error or canceled */
    }
  }

 /*
  * Nope, loop through the authentication schemes to find the first we support.
  */

  www_auth = httpGetField(http, HTTP_FIELD_WWW_AUTHENTICATE);

  for (schemedata = cups_auth_scheme(www_auth, scheme, sizeof(scheme)); schemedata; schemedata = cups_auth_scheme(schemedata + strlen(scheme), scheme, sizeof(scheme)))
  {
   /*
    * Check the scheme name...
    */

    DEBUG_printf(("2cupsDoAuthentication: Trying scheme \"%s\"...", scheme));

    if (!_cups_strcasecmp(scheme, "Bearer"))
    {
      // OAuth 2.0 (Bearer) authentication...
      const char	*bearer = NULL;	/* Bearer token string, if any */

      if (cg->oauth_cb)
      {
        // Try callback...
	char	scope[HTTP_MAX_VALUE];	/* scope="xyz" string */

	cups_auth_param(schemedata, "realm", http->realm, sizeof(http->realm));

	if (cups_auth_param(schemedata, "scope", scope, sizeof(scope)))
	  bearer = (cg->oauth_cb)(http, http->realm, scope, resource, cg->oauth_data);
	else
	  bearer = (cg->oauth_cb)(http, http->realm, NULL, resource, cg->oauth_data);
      }

      if (bearer)
      {
        // Use this access token...
        httpSetAuthString(http, "Bearer", bearer);
        break;
      }
      else
      {
        // No access token, try the next scheme...
        DEBUG_puts("2cupsDoAuthentication: No OAuth access token to provide.");
        continue;
      }
    }
    else if (_cups_strcasecmp(scheme, "Basic") && _cups_strcasecmp(scheme, "Digest") && _cups_strcasecmp(scheme, "Negotiate"))
    {
     /*
      * Other schemes not yet supported...
      */

      DEBUG_printf(("2cupsDoAuthentication: Scheme \"%s\" not yet supported.", scheme));
      continue;
    }

   /*
    * See if we should retry the current username:password...
    */

    if (http->digest_tries > 1 || !http->userpass[0])
    {
     /*
      * Nope - get a new password from the user...
      */

      char default_username[HTTP_MAX_VALUE];
					/* Default username */

      if (!cg->lang_default)
	cg->lang_default = cupsLangDefault();

      if (cups_auth_param(schemedata, "username", default_username, sizeof(default_username)))
	cupsSetUser(default_username);

      snprintf(prompt, sizeof(prompt), _cupsLangString(cg->lang_default, _("Password for %s on %s? ")), cupsGetUser(), http->hostname[0] == '/' ? "localhost" : http->hostname);

      http->digest_tries  = _cups_strncasecmp(scheme, "Digest", 6) != 0;
      http->userpass[0]   = '\0';

      if ((password = cupsGetPassword(prompt, http, method, resource)) == NULL)
      {
        DEBUG_puts("1cupsDoAuthentication: User canceled password request.");
	http->status = HTTP_STATUS_CUPS_AUTHORIZATION_CANCELED;
	return (-1);
      }

      snprintf(http->userpass, sizeof(http->userpass), "%s:%s", cupsGetUser(), password);
    }
    else if (http->status == HTTP_STATUS_UNAUTHORIZED)
      http->digest_tries ++;

    if (http->status == HTTP_STATUS_UNAUTHORIZED && http->digest_tries >= 3)
    {
      DEBUG_printf(("1cupsDoAuthentication: Too many authentication tries (%d)", http->digest_tries));

      http->status = HTTP_STATUS_CUPS_AUTHORIZATION_CANCELED;
      return (-1);
    }

   /*
    * Got a password; encode it for the server...
    */

    if (!_cups_strcasecmp(scheme, "Basic"))
    {
     /*
      * Basic authentication...
      */

      char	encode[256];		/* Base64 buffer */

      DEBUG_puts("2cupsDoAuthentication: Using Basic.");
      httpEncode64(encode, sizeof(encode), http->userpass, strlen(http->userpass));
      httpSetAuthString(http, "Basic", encode);
      break;
    }
    else if (!_cups_strcasecmp(scheme, "Digest"))
    {
     /*
      * Digest authentication...
      */

      char nonce[HTTP_MAX_VALUE];	/* nonce="xyz" string */

      cups_auth_param(schemedata, "algorithm", http->algorithm, sizeof(http->algorithm));
      cups_auth_param(schemedata, "opaque", http->opaque, sizeof(http->opaque));
      cups_auth_param(schemedata, "nonce", nonce, sizeof(nonce));
      cups_auth_param(schemedata, "realm", http->realm, sizeof(http->realm));

      if (_httpSetDigestAuthString(http, nonce, method, resource))
      {
	DEBUG_puts("2cupsDoAuthentication: Using Digest.");
        break;
      }
    }
  }

  if (http->authstring && http->authstring[0])
  {
    DEBUG_printf(("1cupsDoAuthentication: authstring=\"%s\".", http->authstring));

    return (0);
  }
  else
  {
    DEBUG_puts("1cupsDoAuthentication: No supported schemes.");
    http->status = HTTP_STATUS_CUPS_AUTHORIZATION_CANCELED;

    return (-1);
  }
}


/*
 * 'cups_auth_find()' - Find the named WWW-Authenticate scheme.
 *
 * The "www_authenticate" parameter points to the current position in the header.
 *
 * Returns @code NULL@ if the auth scheme is not present.
 */

static const char *				/* O - Start of matching scheme or @code NULL@ if not found */
cups_auth_find(const char *www_authenticate,	/* I - Pointer into WWW-Authenticate header */
               const char *scheme)		/* I - Authentication scheme */
{
  size_t	schemelen = strlen(scheme);	/* Length of scheme */


  DEBUG_printf(("8cups_auth_find(www_authenticate=\"%s\", scheme=\"%s\"(%d))", www_authenticate, scheme, (int)schemelen));

  while (*www_authenticate)
  {
   /*
    * Skip leading whitespace and commas...
    */

    DEBUG_printf(("9cups_auth_find: Before whitespace: \"%s\"", www_authenticate));
    while (isspace(*www_authenticate & 255) || *www_authenticate == ',')
      www_authenticate ++;
    DEBUG_printf(("9cups_auth_find: After whitespace: \"%s\"", www_authenticate));

   /*
    * See if this is "Scheme" followed by whitespace or the end of the string.
    */

    if (!strncmp(www_authenticate, scheme, schemelen) && (isspace(www_authenticate[schemelen] & 255) || www_authenticate[schemelen] == ',' || !www_authenticate[schemelen]))
    {
     /*
      * Yes, this is the start of the scheme-specific information...
      */

      DEBUG_printf(("9cups_auth_find: Returning \"%s\".", www_authenticate));

      return (www_authenticate);
    }

   /*
    * Skip the scheme name or param="value" string...
    */

    while (!isspace(*www_authenticate & 255) && *www_authenticate)
    {
      if (*www_authenticate == '\"')
      {
       /*
        * Skip quoted value...
        */

        www_authenticate ++;
        while (*www_authenticate && *www_authenticate != '\"')
          www_authenticate ++;

        DEBUG_printf(("9cups_auth_find: After quoted: \"%s\"", www_authenticate));
      }

      www_authenticate ++;
    }

    DEBUG_printf(("9cups_auth_find: After skip: \"%s\"", www_authenticate));
  }

  DEBUG_puts("9cups_auth_find: Returning NULL.");

  return (NULL);
}


/*
 * 'cups_auth_param()' - Copy the value for the named authentication parameter,
 *                       if present.
 */

static const char *				/* O - Parameter value or @code NULL@ if not present */
cups_auth_param(const char *scheme,		/* I - Pointer to auth data */
                const char *name,		/* I - Name of parameter */
                char       *value,		/* I - Value buffer */
                size_t     valsize)		/* I - Size of value buffer */
{
  char		*valptr = value,		/* Pointer into value buffer */
      		*valend = value + valsize - 1;	/* Pointer to end of buffer */
  size_t	namelen = strlen(name);		/* Name length */
  int		param;				/* Is this a parameter? */


  DEBUG_printf(("8cups_auth_param(scheme=\"%s\", name=\"%s\", value=%p, valsize=%d)", scheme, name, (void *)value, (int)valsize));

  while (!isspace(*scheme & 255) && *scheme)
    scheme ++;

  while (*scheme)
  {
    while (isspace(*scheme & 255) || *scheme == ',')
      scheme ++;

    if (!strncmp(scheme, name, namelen) && scheme[namelen] == '=')
    {
     /*
      * Found the parameter, copy the value...
      */

      scheme += namelen + 1;
      if (*scheme == '\"')
      {
        scheme ++;

	while (*scheme && *scheme != '\"')
	{
	  if (valptr < valend)
	    *valptr++ = *scheme;

	  scheme ++;
	}
      }
      else
      {
	while (*scheme && strchr("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~+/=", *scheme))
	{
	  if (valptr < valend)
	    *valptr++ = *scheme;

	  scheme ++;
	}
      }

      *valptr = '\0';

      DEBUG_printf(("9cups_auth_param: Returning \"%s\".", value));

      return (value);
    }

   /*
    * Skip the param=value string...
    */

    param = 0;

    while (!isspace(*scheme & 255) && *scheme)
    {
      if (*scheme == '=')
        param = 1;
      else if (*scheme == '\"')
      {
       /*
        * Skip quoted value...
        */

        scheme ++;
        while (*scheme && *scheme != '\"')
          scheme ++;
      }

      scheme ++;
    }

   /*
    * If this wasn't a parameter, we are at the end of this scheme's
    * parameters...
    */

    if (!param)
      break;
  }

  *value = '\0';

  DEBUG_puts("9cups_auth_param: Returning NULL.");

  return (NULL);
}


/*
 * 'cups_auth_scheme()' - Get the (next) WWW-Authenticate scheme.
 *
 * The "www_authenticate" parameter points to the current position in the header.
 *
 * Returns @code NULL@ if there are no (more) auth schemes present.
 */

static const char *				/* O - Start of scheme or @code NULL@ if not found */
cups_auth_scheme(const char *www_authenticate,	/* I - Pointer into WWW-Authenticate header */
		 char       *scheme,		/* I - Scheme name buffer */
		 size_t     schemesize)		/* I - Size of buffer */
{
  const char	*start;				/* Start of scheme data */
  char		*sptr = scheme,			/* Pointer into scheme buffer */
		*send = scheme + schemesize - 1;/* End of scheme buffer */
  int		param;				/* Is this a parameter? */


  DEBUG_printf(("8cups_auth_scheme(www_authenticate=\"%s\", scheme=%p, schemesize=%d)", www_authenticate, (void *)scheme, (int)schemesize));

  while (*www_authenticate)
  {
   /*
    * Skip leading whitespace and commas...
    */

    while (isspace(*www_authenticate & 255) || *www_authenticate == ',')
      www_authenticate ++;

   /*
    * Parse the scheme name or param="value" string...
    */

    for (sptr = scheme, start = www_authenticate, param = 0; *www_authenticate && *www_authenticate != ',' && !isspace(*www_authenticate & 255); www_authenticate ++)
    {
      if (*www_authenticate == '=')
        param = 1;
      else if (!param && sptr < send)
        *sptr++ = *www_authenticate;
      else if (*www_authenticate == '\"')
      {
       /*
        * Skip quoted value...
        */


        do
          www_authenticate ++;
        while (*www_authenticate && *www_authenticate != '\"');
      }
    }

    if (sptr > scheme && !param)
    {
      *sptr = '\0';

      DEBUG_printf(("9cups_auth_scheme: Returning \"%s\".", start));

      return (start);
    }
  }

  *scheme = '\0';

  DEBUG_puts("9cups_auth_scheme: Returning NULL.");

  return (NULL);
}


//
// 'cups_is_local_connection()' - Determine whether a connection is on a local connection.
//

static int				/* O - 0 if not a local connection */
					/*     1  if local connection */
cups_is_local_connection(http_t *http)	/* I - HTTP connection to server */
{
  if (!httpAddrLocalhost(http->hostaddr) && _cups_strcasecmp(http->hostname, "localhost") != 0)
    return 0;
  return 1;
}


/*
 * 'cups_local_auth()' - Get the local authorization certificate if
 *                       available/applicable.
 */

static int				/* O - 0 if available */
					/*     1 if not available */
					/*    -1 error */
cups_local_auth(http_t *http)		/* I - HTTP connection to server */
{
#if defined(_WIN32) || defined(__EMX__)
 /*
  * Currently _WIN32 and OS-2 do not support the CUPS server...
  */

  return (1);
#else
  int			pid;		/* Current process ID */
  FILE			*fp;		/* Certificate file */
  char			trc[16],	/* Try Root Certificate parameter */
			filename[1024];	/* Certificate filename */
  const char		*www_auth,	/* WWW-Authenticate header */
			*schemedata;	/* Data for the named auth scheme */
  _cups_globals_t *cg = _cupsGlobals();	/* Global data */


  DEBUG_printf(("7cups_local_auth(http=%p) hostaddr=%s, hostname=\"%s\"", (void *)http, httpAddrString(http->hostaddr, filename, sizeof(filename)), http->hostname));

 /*
  * See if we are accessing localhost...
  */

  if (!cups_is_local_connection(http))
  {
    DEBUG_puts("8cups_local_auth: Not a local connection!");
    return (1);
  }

  www_auth = httpGetField(http, HTTP_FIELD_WWW_AUTHENTICATE);

#  if defined(SO_PEERCRED) && defined(AF_LOCAL)
 /*
  * See if we can authenticate using the peer credentials provided over a
  * domain socket; if so, specify "PeerCred username" as the authentication
  * information...
  */

  if (http->hostaddr->addr.sa_family == AF_LOCAL &&
      !getenv("GATEWAY_INTERFACE") &&	/* Not via CGI programs... */
      cups_auth_find(www_auth, "PeerCred"))
  {
   /*
    * Verify that the current cupsGetUser() matches the current UID...
    */

    struct passwd	pwd;		/* Password information */
    struct passwd	*result;	/* Auxiliary pointer */
    const char		*username;	/* Current username */

    username = cupsGetUser();

    getpwnam_r(username, &pwd, cg->pw_buf, PW_BUF_SIZE, &result);
    if (result && pwd.pw_uid == getuid())
    {
      httpSetAuthString(http, "PeerCred", username);

      DEBUG_printf(("8cups_local_auth: Returning authstring=\"%s\"",
		    http->authstring));

      return (0);
    }
  }
#  endif /* SO_PEERCRED && AF_LOCAL */

  if ((schemedata = cups_auth_find(www_auth, "Local")) == NULL)
    return (1);

 /*
  * Try opening a certificate file for this PID.  If that fails,
  * try the root certificate...
  */

  pid = getpid();
  snprintf(filename, sizeof(filename), "%s/certs/%d", cg->cups_statedir, pid);
  if ((fp = fopen(filename, "r")) == NULL && pid > 0)
  {
   /*
    * No certificate for this PID; see if we can get the root certificate...
    */

    DEBUG_printf(("9cups_local_auth: Unable to open file \"%s\": %s", filename, strerror(errno)));

    if (!cups_auth_param(schemedata, "trc", trc, sizeof(trc)))
    {
     /*
      * Scheduler doesn't want us to use the root certificate...
      */

      return (1);
    }

    snprintf(filename, sizeof(filename), "%s/certs/0", cg->cups_statedir);
    if ((fp = fopen(filename, "r")) == NULL)
      DEBUG_printf(("9cups_local_auth: Unable to open file \"%s\": %s", filename, strerror(errno)));
  }

  if (fp)
  {
   /*
    * Read the certificate from the file...
    */

    char	certificate[33],	/* Certificate string */
		*certptr;		/* Pointer to certificate string */

    certptr = fgets(certificate, sizeof(certificate), fp);
    fclose(fp);

    if (certptr)
    {
     /*
      * Set the authorization string and return...
      */

      httpSetAuthString(http, "Local", certificate);

      DEBUG_printf(("8cups_local_auth: Returning authstring=\"%s\"",
		    http->authstring));

      return (0);
    }
  }

  return (1);
#endif /* _WIN32 || __EMX__ */
}
