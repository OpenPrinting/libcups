//
// TLS routines for CUPS.
//
// Copyright © 2021-2023 by OpenPrinting.
// Copyright @ 2007-2014 by Apple Inc.
// Copyright @ 1997-2007 by Easy Software Products, all rights reserved.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#include "cups-private.h"
#include "debug-internal.h"
#include <fcntl.h>
#include <math.h>
#include <sys/stat.h>
#ifdef _WIN32
#  include <tchar.h>
#else
#  include <poll.h>
#  include <signal.h>
#  include <sys/time.h>
#  include <sys/resource.h>
#endif // _WIN32


//
// Local globals...
//

static bool		tls_auto_create = false;
					// Auto-create self-signed certs?
static char		*tls_common_name = NULL;
					// Default common name
static char		*tls_keypath = NULL;
					// Certificate store path
static cups_mutex_t	tls_mutex = CUPS_MUTEX_INITIALIZER;
					// Mutex for certificates
static int		tls_options = -1,// Options for TLS connections
			tls_min_version = _HTTP_TLS_1_2,
			tls_max_version = _HTTP_TLS_MAX;


//
// Local functions...
//

static char		*http_copy_file(const char *path, const char *common_name, const char *ext);
static const char	*http_default_path(char *buffer, size_t bufsize);
static const char	*http_make_path(char *buffer, size_t bufsize, const char *dirname, const char *filename, const char *ext);
static bool		http_save_file(const char *path, const char *common_name, const char *ext, const char *value);


//
// Include platform-specific TLS code...
//

#ifdef HAVE_OPENSSL
#    include "tls-openssl.c"
#else // HAVE_GNUTLS
#  include "tls-gnutls.c"
#endif /* HAVE_OPENSSL */


//
// 'cupsCopyCredentials()' - Copy the X.509 certificate chain to a string.
//

char *
cupsCopyCredentials(
    const char *path,			// I - Directory path for certificate/key store or `NULL` for default
    const char *common_name)		// I - Common name
{
  return (http_copy_file(path, common_name, "crt"));
}


//
// 'cupsCopyCredentialsKey()' - Copy the private key to a string.
//

char *
cupsCopyCredentialsKey(
    const char *path,			// I - Directory path for certificate/key store or `NULL` for default
    const char *common_name)		// I - Common name
{
  return (http_copy_file(path, common_name, "key"));
}


//
// 'cupsCopyCredentialsRequest()' - Copy the X.509 certificate signing request to a string.
//

char *
cupsCopyCredentialsRequest(
    const char *path,			// I - Directory path for certificate/key store or `NULL` for default
    const char *common_name)		// I - Common name
{
  return (http_copy_file(path, common_name, "csr"));
}


//
// 'cupsSaveServerCredentials()' - Save the X.509 certificate chain associated
//                                 with a server.
//
// This function saves the the PEM-encoded X.509 certificate chain string to
// the directory "path" or, if "path" is `NULL`, in a per-user or system-wide
// (when running as root) certificate/key store.  The "common_name" value must
// match the value supplied when the @link cupsMakeServerRequest@ function was
// called to obtain the certificate signing request (CSR).  The saved
// certificate is paired with the private key that was generated for the CSR,
// allowing it to be used for encryption and signing.
//

extern bool				// O - `true` on success, `false` on failure
cupsSaveCredentials(
    const char *path,			// I - Directory path for certificate/key store or `NULL` for default
    const char *common_name,		// I - Common name for certificate
    const char *credentials,		// I - PEM-encoded certificate chain
    const char *key)			// I - PEM-encoded private key or `NULL` for none
{
  if (http_save_file(path, common_name, "crt", credentials))
  {
    if (key)
      return (http_save_file(path, common_name, "key", key));
    else
      return (true);
  }

  return (false);
}


//
// 'cupsSetServerCredentials()' - Set the default server credentials.
//
// Note: The server credentials are used by all threads in the running process.
// This function is threadsafe.
//

bool					// O - `true` on success, `false` on failure
cupsSetServerCredentials(
    const char *path,			// I - Directory path for certificate/key store or `NULL` for default
    const char *common_name,		// I - Default common name for server
    bool       auto_create)		// I - `true` = automatically create self-signed certificates
{
  char	temp[1024];			// Default path buffer


  DEBUG_printf(("cupsSetServerCredentials(path=\"%s\", common_name=\"%s\", auto_create=%d)", path, common_name, auto_create));

 /*
  * Use defaults as needed...
  */

  if (!path)
    path = http_default_path(temp, sizeof(temp));

 /*
  * Range check input...
  */

  if (!path || !common_name)
  {
    _cupsSetError(IPP_STATUS_ERROR_INTERNAL, strerror(EINVAL), 0);
    return (0);
  }

  cupsMutexLock(&tls_mutex);

 /*
  * Free old values...
  */

  if (tls_keypath)
    _cupsStrFree(tls_keypath);

  if (tls_common_name)
    _cupsStrFree(tls_common_name);

 /*
  * Save the new values...
  */

  tls_keypath     = _cupsStrAlloc(path);
  tls_auto_create = auto_create;
  tls_common_name = _cupsStrAlloc(common_name);

  cupsMutexUnlock(&tls_mutex);

  return (1);
}


//
// 'httpLoadCredentials()' - Load X.509 credentials from a keychain file.
//

bool					// O - `true` on success, `false` on error
httpLoadCredentials(
    const char   *path,			// I  - Keychain/PKCS#12 path
    cups_array_t **credentials,		// IO - Credentials
    const char   *common_name)		// I  - Common name for credentials
{
  cups_file_t		*fp;		// Certificate file
  char			filename[1024],	// filename.crt
			temp[1024],	// Temporary string
			line[256];	// Base64-encoded line
  unsigned char		*data = NULL;	// Buffer for cert data
  size_t		alloc_data = 0,	// Bytes allocated
			num_data = 0,	// Bytes used
			decoded;	// Bytes decoded
  bool			in_certificate = false;
					// In a certificate?


  if (credentials)
    *credentials = NULL;

  if (!credentials || !common_name)
    return (false);

  if (!path)
    path = http_default_path(temp, sizeof(temp));
  if (!path)
    return (false);

  http_make_path(filename, sizeof(filename), path, common_name, "crt");

  if ((fp = cupsFileOpen(filename, "r")) == NULL)
    return (false);

  while (cupsFileGets(fp, line, sizeof(line)))
  {
    if (!strcmp(line, "-----BEGIN CERTIFICATE-----"))
    {
      if (in_certificate)
      {
        // Missing END CERTIFICATE...
        httpFreeCredentials(*credentials);
	*credentials = NULL;
        break;
      }

      in_certificate = true;
    }
    else if (!strcmp(line, "-----END CERTIFICATE-----"))
    {
      if (!in_certificate || !num_data)
      {
        // Missing data...
        httpFreeCredentials(*credentials);
	*credentials = NULL;
        break;
      }

      if (!*credentials)
        *credentials = cupsArrayNew(NULL, NULL, NULL, 0, NULL, NULL);

      if (httpAddCredential(*credentials, data, num_data))
      {
        httpFreeCredentials(*credentials);
	*credentials = NULL;
        break;
      }

      num_data       = 0;
      in_certificate = false;
    }
    else if (in_certificate)
    {
      if (alloc_data == 0)
      {
        data       = malloc(2048);
	alloc_data = 2048;

        if (!data)
	  break;
      }
      else if ((num_data + strlen(line)) >= alloc_data)
      {
        unsigned char *tdata = realloc(data, alloc_data + 1024);
					// Expanded buffer

	if (!tdata)
	{
	  httpFreeCredentials(*credentials);
	  *credentials = NULL;
	  break;
	}

	data       = tdata;
        alloc_data += 1024;
      }

      decoded = alloc_data - num_data;
      httpDecode64((char *)data + num_data, &decoded, line, NULL);
      num_data += (size_t)decoded;
    }
  }

  cupsFileClose(fp);

  if (in_certificate)
  {
    // Missing END CERTIFICATE...
    httpFreeCredentials(*credentials);
    *credentials = NULL;
  }

  if (data)
    free(data);

  return (*credentials != NULL);
}


//
// 'httpSaveCredentials()' - Save X.509 credentials to a keychain file.
//

bool					// O - `true` on success, `false` on error
httpSaveCredentials(
    const char   *path,			// I - Keychain/PKCS#12 path
    cups_array_t *credentials,		// I - Credentials
    const char   *common_name)		// I - Common name for credentials
{
  cups_file_t		*fp;		// Certificate file
  char			filename[1024],	// filename.crt
			nfilename[1024],// filename.crt.N
			temp[1024],	// Temporary string
			line[256];	// Base64-encoded line
  const unsigned char	*ptr;		// Pointer into certificate
  ssize_t		remaining;	// Bytes left
  http_credential_t	*cred;		// Current credential


  if (!credentials || !common_name)
    return (false);

  if (!path)
    path = http_default_path(temp, sizeof(temp));
  if (!path)
    return (false);

  http_make_path(filename, sizeof(filename), path, common_name, "crt");
  snprintf(nfilename, sizeof(nfilename), "%s.N", filename);

  if ((fp = cupsFileOpen(nfilename, "w")) == NULL)
    return (false);

#ifndef _WIN32
  fchmod(cupsFileNumber(fp), 0600);
#endif // !_WIN32

  for (cred = (http_credential_t *)cupsArrayGetFirst(credentials); cred; cred = (http_credential_t *)cupsArrayGetNext(credentials))
  {
    cupsFilePuts(fp, "-----BEGIN CERTIFICATE-----\n");
    for (ptr = cred->data, remaining = (ssize_t)cred->datalen; remaining > 0; remaining -= 45, ptr += 45)
    {
      httpEncode64(line, sizeof(line), (char *)ptr, remaining > 45 ? 45 : (size_t)remaining, false);
      cupsFilePrintf(fp, "%s\n", line);
    }
    cupsFilePuts(fp, "-----END CERTIFICATE-----\n");
  }

  cupsFileClose(fp);

  return (!rename(nfilename, filename));
}


//
// '_httpTLSSetOptions()' - Set TLS protocol and cipher suite options.
//

void
_httpTLSSetOptions(int options,		// I - Options
                   int min_version,	// I - Minimum TLS version
                   int max_version)	// I - Maximum TLS version
{
  if (!(options & _HTTP_TLS_SET_DEFAULT) || tls_options < 0)
  {
    tls_options     = options;
    tls_min_version = min_version;
    tls_max_version = max_version;
  }
}


//
// 'http_copy_file()' - Copy the contents of a file to a string.
//

static char *
http_copy_file(const char *path,	// I - Directory
               const char *common_name,	// I - Common name
               const char *ext)		// I - Extension
{
  char		*s = NULL;		// String
  int		fd;			// File descriptor
  char		defpath[1024],		// Default path
		filename[1024];		// Filename
  struct stat	fileinfo;		// File information


  if (!common_name)
    return (NULL);

  if (!path)
    path = http_default_path(defpath, sizeof(defpath));

  if ((fd = open(http_make_path(filename, sizeof(filename), path, common_name, ext), O_RDONLY)) < 0)
    return (NULL);

  if (fstat(fd, &fileinfo))
    goto done;

  if (fileinfo.st_size > 65536)
  {
    close(fd);
    return (NULL);
  }

  if ((s = calloc(1, (size_t)fileinfo.st_size + 1)) == NULL)
  {
    close(fd);
    return (NULL);
  }

  if (read(fd, s, (size_t)fileinfo.st_size) < 0)
  {
    free(s);
    s = NULL;
  }

  done:

  close(fd);

  return (s);
}


//
// 'http_default_path()' - Get the default credential store path.
//

static const char *			// O - Path or NULL on error
http_default_path(
    char   *buffer,			// I - Path buffer
    size_t bufsize)			// I - Size of path buffer
{
  _cups_globals_t	*cg = _cupsGlobals();
					// Pointer to library globals


  if (cg->userconfig)
  {
    if (mkdir(cg->userconfig, 0755) && errno != EEXIST)
    {
      DEBUG_printf(("1http_default_path: Failed to make directory '%s': %s", cg->userconfig, strerror(errno)));
      return (NULL);
    }

    snprintf(buffer, bufsize, "%s/ssl", cg->userconfig);

    if (mkdir(buffer, 0700) && errno != EEXIST)
    {
      DEBUG_printf(("1http_default_path: Failed to make directory '%s': %s", buffer, strerror(errno)));
      return (NULL);
    }
  }
  else
  {
    if (mkdir(cg->sysconfig, 0755) && errno != EEXIST)
    {
      DEBUG_printf(("1http_default_path: Failed to make directory '%s': %s", cg->sysconfig, strerror(errno)));
      return (NULL);
    }

    snprintf(buffer, bufsize, "%s/ssl", cg->sysconfig);

    if (mkdir(buffer, 0700) && errno != EEXIST)
    {
      DEBUG_printf(("1http_default_path: Failed to make directory '%s': %s", buffer, strerror(errno)));
      return (NULL);
    }
  }

  DEBUG_printf(("1http_default_path: Using default path \"%s\".", buffer));

  return (buffer);
}


//
// 'http_make_path()' - Format a filename for a certificate or key file.
//

static const char *			// O - Filename
http_make_path(
    char       *buffer,			// I - Filename buffer
    size_t     bufsize,			// I - Size of buffer
    const char *dirname,		// I - Directory
    const char *filename,		// I - Filename (usually hostname)
    const char *ext)			// I - Extension
{
  char	*bufptr,			// Pointer into buffer
	*bufend = buffer + bufsize - 1;	// End of buffer


  snprintf(buffer, bufsize, "%s/", dirname);
  bufptr = buffer + strlen(buffer);

  while (*filename && bufptr < bufend)
  {
    if (_cups_isalnum(*filename) || *filename == '-' || *filename == '.')
      *bufptr++ = *filename;
    else
      *bufptr++ = '_';

    filename ++;
  }

  if (bufptr < bufend && filename[-1] != '.')
    *bufptr++ = '.';

  cupsCopyString(bufptr, ext, (size_t)(bufend - bufptr + 1));

  return (buffer);
}


//
// 'http_save_file()' - Save a string to a file.
//

static bool				// O - `true` on success, `false` on failure
http_save_file(const char *path,	// I - Directory path for certificate/key store or `NULL` for default
               const char *common_name,	// I - Common name
               const char *ext,		// I - Extension
	       const char *value)	// I - String value
{
  char	defpath[1024],			// Default path
	filename[1024];			// Output filename
  int	fd;				// File descriptor


  // Range check input...
  if (!common_name || !value)
    return (false);

  // Get default path as needed...
  if (!path)
    path = http_default_path(defpath, sizeof(defpath));

  if ((fd = open(http_make_path(filename, sizeof(filename), path, common_name, ext), O_CREAT | O_WRONLY | O_TRUNC, 0644)) < 0)
    return (false);

  if (write(fd, value, strlen(value)) < 0)
  {
    close(fd);
    unlink(filename);
    return (false);
  }

  close(fd);

  return (true);
}


