//
// JSON Web Token API implementation for CUPS.
//
// Copyright © 2023 by OpenPrinting.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#include "cups-private.h"
#include "jwt.h"
#include "json-private.h"
#ifdef HAVE_OPENSSL
#  include <openssl/hmac.h>
#else
#  include <gnutls/gnutls.h>
#endif // HAVE_OPENSSL


//
// Constants...
//

#define _CUPS_JWT_HMAC_B	64	// Block size for HMAC
#define _CUPS_JWT_MAX_SIGNATURE	32	// 256-bit hash


//
// Private types...
//

struct _cups_jwt_s			// JWT object
{
  cups_json_t	*jose;			// JOSE header
  cups_json_t	*claims;		// JWT claims
  cups_jwa_t	sigalg;			// Signature algorithm
  size_t	sigsize;		// Size of signature
  unsigned char	*signature;		// Signature
};


//
// Local globals...
//

static const char * const cups_jwa_strings[CUPS_JWA_MAX] =
{
  "none",				// No algorithm
  "HS256",				// HMAC using SHA-256
  "HS384",				// HMAC using SHA-384
  "HS512",				// HMAC using SHA-512
  "RS256",				// RSASSA-PKCS1-v1_5 using SHA-256
  "RS384",				// RSASSA-PKCS1-v1_5 using SHA-384
  "RS512",				// RSASSA-PKCS1-v1_5 using SHA-512
  "ES256",				// ECDSA using P-256 and SHA-256
  "ES384",				// ECDSA using P-384 and SHA-384
  "ES512"				// ECDSA using P-521 and SHA-512
};


//
// Local functions...
//

static bool	make_signature(cups_jwt_t *jwt, cups_jwa_t alg, cups_json_t *jwk, unsigned char *signature, size_t *sigsize);
static char	*make_string(cups_jwt_t *jwt, bool with_signature);


//
// 'cupsJWTDelete()' - Free the memory used for a JSON Web Token.
//

void
cupsJWTDelete(cups_jwt_t *jwt)		// I - JWT object
{
  if (jwt)
  {
    cupsJSONDelete(jwt->jose);
    cupsJSONDelete(jwt->claims);
    free(jwt->signature);
    free(jwt);
  }
}


//
// 'cupsJWTExportString()' - Export a JWT with the JWS Compact Serialization format.
//

char *					// O - JWT/JWS Compact Serialization string
cupsJWTExportString(cups_jwt_t *jwt)	// I - JWT object
{
  if (jwt)
    return (make_string(jwt, true));
  else
    return (NULL);
}


//
// 'cupsJWTGetAlgorithm()' - Get the signature algorithm used by a JSON Web Token.
//

cups_jwa_t				// O - Signature algorithm
cupsJWTGetAlgorithm(cups_jwt_t *jwt)	// I - JWT object
{
  return (jwt ? jwt->sigalg : CUPS_JWA_NONE);
}


//
// 'cupsJWTGetClaimNumber()' - Get the number value of a claim.
//

double					// O - Number value
cupsJWTGetClaimNumber(cups_jwt_t *jwt,	// I - JWT object
                      const char *claim)// I - Claim name
{
  cups_json_t	*node;			// Value node


  if (jwt && (node = cupsJSONFind(jwt->claims, claim)) != NULL)
    return (cupsJSONGetNumber(node));
  else
    return (0.0);
}


//
// 'cupsJWTGetClaimString()' - Get the string value of a claim.
//

const char *				// O - String value
cupsJWTGetClaimString(cups_jwt_t *jwt,	// I - JWT object
                      const char *claim)// I - Claim name
{
  cups_json_t	*node;			// Value node


  if (jwt && (node = cupsJSONFind(jwt->claims, claim)) != NULL)
    return (cupsJSONGetString(node));
  else
    return (NULL);
}


//
// 'cupsJWTGetClaimType()' - Get the value type of a claim.
//

cups_jtype_t				// O - JSON value type
cupsJWTGetClaimType(cups_jwt_t *jwt,	// I - JWT object
                    const char *claim)	// I - Claim name
{
  cups_json_t	*node;			// Value node


  if (jwt && (node = cupsJSONFind(jwt->claims, claim)) != NULL)
    return (cupsJSONGetType(node));
  else
    return (CUPS_JTYPE_NULL);
}


//
// 'cupsJWTGetClaimValue()' - Get the value node of a claim.
//

cups_json_t *				// O - JSON value node
cupsJWTGetClaimValue(cups_jwt_t *jwt,	// I - JWT object
                     const char *claim)	// I - Claim name
{
  if (jwt)
    return (cupsJSONFind(jwt->claims, claim));
  else
    return (NULL);
}


//
// 'cupsJWTGetClaims()' - Get the JWT claims as a JSON object.
//

cups_json_t *				// O - JSON object
cupsJWTGetClaims(cups_jwt_t *jwt)	// I - JWT object
{
  return (jwt ? jwt->claims : NULL);
}


//
// 'cupsJWTHasValidSignature()' - Determine whether the JWT has a valid signature.
//

bool					// O - `true` if value, `false` otherwise
cupsJWTHasValidSignature(
    cups_jwt_t  *jwt,			// I - JWT object
    cups_json_t *jwk)			// I - JWK key set
{
  unsigned char	signature[_CUPS_JWT_MAX_SIGNATURE];
					// Signature
  size_t	sigsize = _CUPS_JWT_MAX_SIGNATURE;
					// Size of signature


  // Range check input...
  if (!jwt || !jwt->signature || !jwk)
    return (false);

  // Calculate signature with keys...
  if (!make_signature(jwt, jwt->sigalg, jwk, signature, &sigsize))
    return (false);

  fprintf(stderr, "orig sig(%u) = %02X%02X%02X%02X...%02X%02X%02X%02X\n", (unsigned)jwt->sigsize, jwt->signature[0], jwt->signature[1], jwt->signature[2], jwt->signature[3], jwt->signature[jwt->sigsize - 4], jwt->signature[jwt->sigsize - 3], jwt->signature[jwt->sigsize - 2], jwt->signature[jwt->sigsize - 1]);
  fprintf(stderr, "calc sig(%u) = %02X%02X%02X%02X...%02X%02X%02X%02X\n", (unsigned)sigsize, signature[0], signature[1], signature[2], signature[3], signature[sigsize - 4], signature[sigsize - 3], signature[sigsize - 2], signature[sigsize - 1]);

  // Compae and return the result...
  return (jwt->sigsize == sigsize && !memcmp(jwt->signature, signature, sigsize));
}


//
// 'cupsJWTImportString()' - Import a JSON Web Token or JSON Web Signature.
//

cups_jwt_t *				// O - JWT object
cupsJWTImportString(const char *token)	// I - JWS Compact Serialization string
{
  cups_jwt_t	*jwt;			// JWT object
  const char	*tokptr;		// Pointer into the token
  size_t	datalen;		// Size of data
  char		data[65536];		// Data
  const char	*alg;			// Signature algorithm, if any


  // Allocate a JWT...
  if ((jwt = calloc(1, sizeof(cups_jwt_t))) == NULL)
    return (NULL);

  // Extract the JOSE header...
  datalen = sizeof(data) - 1;
  if (!httpDecode64(data, &datalen, token, &tokptr))
    goto import_error;

  if (!tokptr || *tokptr != '.')
    goto import_error;

  tokptr ++;
  data[datalen] = '\0';
  if ((jwt->jose = cupsJSONImportString(data)) == NULL)
    goto import_error;

  // Extract the JWT claims...
  datalen = sizeof(data) - 1;
  if (!httpDecode64(data, &datalen, tokptr, &tokptr))
    goto import_error;

  if (!tokptr || *tokptr != '.')
    goto import_error;

  tokptr ++;
  data[datalen] = '\0';
  if ((jwt->claims = cupsJSONImportString(data)) == NULL)
    goto import_error;

  // Extract the signature, if any...
  datalen = sizeof(data);
  if (!httpDecode64(data, &datalen, tokptr, &tokptr))
    goto import_error;

  if (!tokptr || *tokptr)
    goto import_error;

  if (datalen > 0)
  {
    if ((jwt->signature = malloc(datalen)) == NULL)
      goto import_error;

    memcpy(jwt->signature, data, datalen);
    jwt->sigsize = datalen;
  }

  if ((alg = cupsJSONGetString(cupsJSONFind(jwt->jose, "alg"))) != NULL)
  {
    cups_jwa_t	sigalg;			// Signing algorithm

    for (sigalg = CUPS_JWA_NONE; sigalg < CUPS_JWA_MAX; sigalg ++)
    {
      if (!strcmp(alg, cups_jwa_strings[sigalg]))
      {
        jwt->sigalg = sigalg;
        break;
      }
    }
  }

  // Can't have signature with none or no signature for !none...
  if ((jwt->sigalg == CUPS_JWA_NONE) != (jwt->sigsize == 0))
    goto import_error;

  // Return the new JWT...
  return (jwt);

  import_error:

  _cupsSetError(IPP_STATUS_ERROR_INTERNAL, _("Invalid JSON web token."), 1);
  cupsJWTDelete(jwt);

  return (NULL);
}


//
// 'cupsJWTNew()' - Create a new, empty JSON Web Token.
//

cups_jwt_t *				// O - JWT object
cupsJWTNew(const char *type)		// I - JWT type or `NULL` for default ("JWT")
{
  cups_jwt_t	*jwt;			// JWT object


  if ((jwt = calloc(1, sizeof(cups_jwt_t))) != NULL)
  {
    if ((jwt->jose = cupsJSONNew(NULL, NULL, CUPS_JTYPE_OBJECT)) != NULL)
    {
      cupsJSONNewString(jwt->jose, cupsJSONNewKey(jwt->jose, NULL, "typ"), type ? type : "JWT");

      if ((jwt->claims = cupsJSONNew(NULL, NULL, CUPS_JTYPE_OBJECT)) != NULL)
        return (jwt);
    }
  }

  cupsJWTDelete(jwt);
  return (NULL);
}


//
// 'cupsJWTSetClaimNumber()' - Set a claim number.
//

void
cupsJWTSetClaimNumber(cups_jwt_t *jwt,	// I - JWT object
                      const char *claim,// I - Claim name
                      double     value)	// I - Number value
{
  // Range check input
  if (!jwt || !claim)
    return;

  // Remove existing claim, if any...
  _cupsJSONDelete(jwt->claims, claim);

  // Add claim...
  cupsJSONNewNumber(jwt->claims, cupsJSONNewKey(jwt->claims, NULL, claim), value);
}


//
// 'cupsJWTSetClaimString()' - Set a claim string.
//

void
cupsJWTSetClaimString(cups_jwt_t *jwt,	// I - JWT object
                      const char *claim,// I - Claim name
                      const char *value)// I - String value
{
  // Range check input
  if (!jwt || !claim || !value)
    return;

  // Remove existing claim, if any...
  _cupsJSONDelete(jwt->claims, claim);

  // Add claim...
  cupsJSONNewString(jwt->claims, cupsJSONNewKey(jwt->claims, NULL, claim), value);
}


//
// 'cupsJWTSetClaimValue()' - Set a claim value.
//

void
cupsJWTSetClaimValue(
    cups_jwt_t  *jwt,			// I - JWT object
    const char  *claim,			// I - Claim name
    cups_json_t *value)			// I - JSON value node
{
  // Range check input
  if (!jwt || !claim)
    return;

  // Remove existing claim, if any...
  _cupsJSONDelete(jwt->claims, claim);

  // Add claim...
  _cupsJSONAdd(jwt->claims, cupsJSONNewKey(jwt->claims, NULL, claim), value);
}


//
// 'cupsJWTSign()' - Sign a JSON Web Token, creating a JSON Web Signature.
//

bool					// O - `true` on success, `false` on error
cupsJWTSign(cups_jwt_t  *jwt,		// I - JWT object
            cups_jwa_t  alg,		// I - Signing algorithm
            cups_json_t *jwk)		// I - JWK key set
{
  unsigned char	signature[_CUPS_JWT_MAX_SIGNATURE];
					// Signature
  size_t	sigsize = _CUPS_JWT_MAX_SIGNATURE;
					// Size of signature


  // Range check input...
  if (!jwt || alg <= CUPS_JWA_NONE || alg >= CUPS_JWA_MAX || !jwk)
  {
    _cupsSetError(IPP_STATUS_ERROR_INTERNAL, strerror(EINVAL), 0);
    return (false);
  }

  // Clear existing signature...
  free(jwt->signature);
  jwt->signature = NULL;
  jwt->sigsize   = 0;
  jwt->sigalg    = CUPS_JWA_NONE;

  _cupsJSONDelete(jwt->jose, "alg");

  // Create new signature...
  if (!make_signature(jwt, alg, jwk, signature, &sigsize))
    return (false);

  if ((jwt->signature = malloc(sigsize)) == NULL)
    return (false);

  memcpy(jwt->signature, signature, sigsize);
  jwt->sigalg  = alg;
  jwt->sigsize = sigsize;

  cupsJSONNewString(jwt->jose, cupsJSONNewKey(jwt->jose, NULL, "alg"), cups_jwa_strings[alg]);

  return (true);
}


//
// 'make_signature()' - Make a signature.
//

static bool				// O  - `true` on success, `false` on failure
make_signature(cups_jwt_t    *jwt,	// I  - JWT
               cups_jwa_t    alg,	// I  - Algorithm
               cups_json_t   *jwk,	// I  - JSON Web Key Set
               unsigned char *signature,// I  - Signature buffer
               size_t        *sigsize)	// IO - Signature size
{
  const char	*k;			// "k" value
  char		key[256];		// Key value
  size_t	key_idx,		// Index into key
		key_len;		// Length of key
  char		*text;			// JWS Signing Input
  size_t	text_len;		// Lengtb of signing input
  unsigned char	buffer[65536],		// Buffer to hash
		hash[32];		// SHA2-256 hash
  size_t	buffer_len;		// Length of buffer


  *sigsize = 0;

  memset(key, 0, sizeof(key));
  k       = cupsJSONGetString(cupsJSONFind(jwk, "k"));
  key_len = sizeof(key);
  httpDecode64(key, &key_len, k, NULL);

  if (key_len > 64)
  {
    cupsHashData("sha2-256", key, key_len, hash, sizeof(hash));
    memcpy(key, hash, 32);
    memset(key + 32, 0, 32);
    key_len = 64;
  }

  fprintf(stderr, "alg=%u(%s)\n", alg, cups_jwa_strings[alg]);
  fprintf(stderr, "key(%u): [%d", (unsigned)key_len, key[0] & 255);
  for (size_t i = 1; i < key_len; i ++)
    fprintf(stderr, ", %d", key[i] & 255);
  fputs("]\n", stderr);

  text     = make_string(jwt, false);
  text_len = strlen(text);

  fprintf(stderr, "text (%u): %s\n", (unsigned)text_len, text);
  fprintf(stderr, "text (%u): [%d", (unsigned)text_len, *text);
  for (char *tptr = text + 1; *tptr; tptr ++)
    fprintf(stderr, ", %d", *tptr);
  fputs("]\n", stderr);

  // H1 = H(K XOR ipad,text)
  for (key_idx = 0; key_idx < _CUPS_JWT_HMAC_B; key_idx ++)
    buffer[key_idx] = key[key_idx] ^ 0x36;
  memcpy(buffer + _CUPS_JWT_HMAC_B, text, text_len);
  buffer_len = _CUPS_JWT_HMAC_B + text_len;

  cupsHashData("sha2-256", buffer, buffer_len, hash, sizeof(hash));

  // H(K XOR opad,H1)
  for (key_idx = 0; key_idx < _CUPS_JWT_HMAC_B; key_idx ++)
    buffer[key_idx] = key[key_idx] ^ 0x5c;
  memcpy(buffer + _CUPS_JWT_HMAC_B, hash, sizeof(hash));
  buffer_len = _CUPS_JWT_HMAC_B + sizeof(hash);
  cupsHashData("sha2-256", buffer, buffer_len, signature, sizeof(hash));

  free(text);

  *sigsize = sizeof(hash);

  fprintf(stderr, "HMAC: %s\n", cupsHashString(signature, 32, key, sizeof(key)));

#if 0
#ifdef HAVE_OPENSSL
  const EVP_MD	*md;			// Message digest
  unsigned	mdlen;			// Length of digest
  const char	*k = NULL;		// "k" value
  char		key[512];		// Key value
  size_t	key_len;		// Length of key
  char		*text;			// JWS Signing Input


  *sigsize = 0;

  switch (alg)
  {
    case CUPS_JWA_HS256 : // HMAC using SHA-256
        k  = cupsJSONGetString(cupsJSONFind(jwk, "k"));
        md = EVP_sha256();
        break;

    case CUPS_JWA_HS384 : // HMAC using SHA-384
        k  = cupsJSONGetString(cupsJSONFind(jwk, "k"));
        md = EVP_sha384();
        break;

    case CUPS_JWA_HS512 : // HMAC using SHA-512
        k  = cupsJSONGetString(cupsJSONFind(jwk, "k"));
        md = EVP_sha512();
        break;

    default :
        return (false);

#  if 0
    case CUPS_JWA_RS256 : // RSASSA-PKCS1-v1_5 using SHA-256
        break;

    case CUPS_JWA_RS384 : // RSASSA-PKCS1-v1_5 using SHA-384
        break;

    case CUPS_JWA_RS512 : // RSASSA-PKCS1-v1_5 using SHA-512
        break;

    case CUPS_JWA_ES256 : // ECDSA using P-256 and SHA-256
        break;

    case CUPS_JWA_ES384 : // ECDSA using P-384 and SHA-384
        break;

    case CUPS_JWA_ES512 : // ECDSA using P-521 and SHA-512
        break;
#  endif // 0
  }

  if (!k)
    return (false);

  key_len = sizeof(key);
  httpDecode64(key, &key_len, k, NULL);

  fprintf(stderr, "alg=%u(%s)\n", alg, cups_jwa_strings[alg]);
  fprintf(stderr, "key(%u): [%d", (unsigned)key_len, key[0] & 255);
  for (size_t i = 1; i < key_len; i ++)
    fprintf(stderr, ", %d", key[i] & 255);
  fputs("]\n", stderr);

  text  = make_string(jwt, false);
  mdlen = (unsigned)*sigsize;

  fprintf(stderr, "text (%u): %s\n", (unsigned)strlen(text), text);
  fprintf(stderr, "text (%u): [%d", (unsigned)strlen(text), *text);
  for (char *tptr = text + 1; *tptr; tptr ++)
    fprintf(stderr, ", %d", *tptr);
  fputs("]\n", stderr);

  HMAC(md, key, (int)key_len, (unsigned char *)text, strlen(text), signature, &mdlen);

  free(text);
  *sigsize = mdlen;

#else
  *sigsize = 0;

  switch (alg)
  {
    case CUPS_JWA_HS256 : // HMAC using SHA-256
    case CUPS_JWA_HS384 : // HMAC using SHA-384
    case CUPS_JWA_HS512 : // HMAC using SHA-512
        break;

    case CUPS_JWA_RS256 : // RSASSA-PKCS1-v1_5 using SHA-256
    case CUPS_JWA_RS384 : // RSASSA-PKCS1-v1_5 using SHA-384
    case CUPS_JWA_RS512 : // RSASSA-PKCS1-v1_5 using SHA-512
        break;

    case CUPS_JWA_ES256 : // ECDSA using P-256 and SHA-256
    case CUPS_JWA_ES384 : // ECDSA using P-384 and SHA-384
    case CUPS_JWA_ES512 : // ECDSA using P-521 and SHA-512
        break;
  }
#endif // HAVE_OPENSSL
#endif // 0

  return (true);
}


//
// 'make_string()' - Make a JWT/JWS Compact Serialization string.
//

static char *				// O - JWT/JWS string
make_string(cups_jwt_t *jwt,		// I - JWT object
            bool       with_signature)	// I - Include signature field?
{
  char		*jose,			// JOSE header string
		*claims,		// Claims string
		*s = NULL,		// JWT/JWS string
		*ptr,			// Pointer into string
		*end;			// End of string
  size_t	jose_len,		// Length of JOSE header
		claims_len,		// Length of claims string
		len;			// Allocation length


  // Get the JOSE header and claims object strings...
  jose   = cupsJSONExportString(jwt->jose);
  claims = cupsJSONExportString(jwt->claims);

  if (!jose || !claims)
    goto done;

  jose_len   = strlen(jose);
  claims_len = strlen(claims);

  // Calculate the maximum Base64URL-encoded string length...
  len = ((jose_len + 2) * 4 / 3) + 1 + ((claims_len + 2) * 4 / 3) + 1 + ((_CUPS_JWT_MAX_SIGNATURE + 2) * 4 / 3) + 1;

  if ((s = malloc(len)) == NULL)
    goto done;

  ptr = s;
  end = s + len;

  httpEncode64(ptr, (size_t)(end - ptr), jose, jose_len, true);
  ptr += strlen(ptr);
  *ptr++ = '.';

  httpEncode64(ptr, (size_t)(end - ptr), claims, claims_len, true);
  ptr += strlen(ptr);

  if (with_signature)
  {
    *ptr++ = '.';

    if (jwt->sigsize)
      httpEncode64(ptr, (size_t)(end - ptr), (char *)jwt->signature, jwt->sigsize, true);
  }

  // Free temporary strings...
  done:

  free(jose);
  free(claims);

  return (s);
}
