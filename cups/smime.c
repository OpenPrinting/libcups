//
// S/MIME API implementation for CUPS.
//
// Copyright © 2023 by OpenPrinting.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#include "cups-private.h"
#include "smime.h"
#ifdef HAVE_OPENSSL
#  include <openssl/ecdsa.h>
#  include <openssl/evp.h>
#  include <openssl/rsa.h>
#else
#  include <gnutls/gnutls.h>
#  include <gnutls/abstract.h>
#  include <gnutls/crypto.h>
#endif // HAVE_OPENSSL


//
// Private types...
//

struct _cups_smime_s			// S/MIME read/write stream
{
  cups_smime_cb_t	cb;		// Input/output callback
  void			*context;	// Callback context
};


//
// 'cupsSMIMEClose()' - Close an S/MIME container.
//

bool					// O - `true` on success, `false` on error
cupsSMIMEClose(cups_smime_t *smime)	// I - S/MIME read/write stream
{
  (void)smime;

  return (false);
}


//
// 'cupsSMIMEOpen()' - Open an S/MIME container for reading or writing through a file.
//

cups_smime_t *				// O - 	S/MIME read/write stream
cupsSMIMEOpen(
    const char *filename,		// I - Filename
    const char *mode,			// I - "r" to read, "w" to write
    const char *credentials,		// I - X.509 certificate and public key string
    const char *key,			// I - X.509 private key string for reading or `NULL` for writing
    const char *password)		// I - Additional password, if any
{
  (void)filename;
  (void)mode;
  (void)credentials;
  (void)key;
  (void)password;

  return (NULL);
}


//
// 'cupsSMIMEOpenIO()' - Open an S/MIME container for reading or writing through a callback.
//

cups_smime_t *				// O - S/MIME read/write stream
cupsSMIMEOpenIO(
    void            *context,		// I - I/O context
    cups_smime_cb_t cb,			// I - I/O callback
    const char      *mode,		// I - "r" to read, "w" to write
    const char      *credentials,	// I - X.509 certificate and public key string
    const char      *key,		// I - X.509 private key string for reading or `NULL` for writing
    const char      *password)		// I - Additional password, if any
{
  (void)context;
  (void)cb;
  (void)mode;
  (void)credentials;
  (void)key;
  (void)password;

  return (NULL);
}


//
// 'cupsSMIMERead()' - Read from an S/MIME container.
//

ssize_t					// O - Number of bytes read or `-1` on error
cupsSMIMERead(cups_smime_t *smime,	// I - S/MIME read stream
              char         *buffer,	// I - Read buffer
              size_t       bytes)	// I - Number of bytes to read
{
  (void)smime;
  (void)buffer;
  (void)bytes;

  return (-1);
}


//
// 'cupsSMIMEWrite()' - Write to an S/MIME container.
//

ssize_t					// O - Number of bytes written or `-1` on error
cupsSMIMEWrite(cups_smime_t *smime,	// I - S/MIME write stream
               const char   *buffer,	// I - Write buffer
               size_t       bytes)	// I - Number of bytes to write
{
  (void)smime;
  (void)buffer;
  (void)bytes;

  return (-1);
}
