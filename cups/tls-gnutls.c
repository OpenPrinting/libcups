//
// TLS support code for CUPS using GNU TLS.
//
// Copyright © 2020-2023 by OpenPrinting
// Copyright © 2007-2019 by Apple Inc.
// Copyright © 1997-2007 by Easy Software Products, all rights reserved.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

//// This file is included from tls.c


//
// Local functions...
//

static gnutls_x509_crt_t gnutls_create_credential(http_credential_t *credential);
static gnutls_x509_privkey_t gnutls_create_key(cups_credtype_t type);
static ssize_t		gnutls_http_read(gnutls_transport_ptr_t ptr, void *data, size_t length);
static ssize_t		gnutls_http_write(gnutls_transport_ptr_t ptr, const void *data, size_t length);
static void		gnutls_load_crl(void);


//
// Local globals...
//

static gnutls_x509_crl_t tls_crl = NULL;/* Certificate revocation list */


//
// 'cupsCreateCredentials()' - Make an X.509 certificate and private key pair.
//
// This function creates an X.509 certificate and private key pair.  The
// certificate and key are stored in the directory "path" or, if "path" is
// `NULL`, in a per-user or system-wide (when running as root) certificate/key
// store.  The generated certificate is signed by the named root certificate or,
// if "root_name" is `NULL`, a site-wide default root certificate.  When
// "root_name" is `NULL` and there is no site-wide default root certificate, a
// self-signed certificate is generated instead.
//

bool					// O - `true` on success, `false` on error
cupsCreateCredentials(
    const char         *path,		// I - Directory path for certificate/key store or `NULL` for default
    bool               ca_cert,		// I - `true` to create a CA certificate, `false` for a client/server certificate
    cups_credpurpose_t purpose,		// I - Credential purposes
    cups_credtype_t    type,		// I - Credential type
    cups_credusage_t   usage,		// I - Credential usages
    const char         *organization,	// I - Organization or `NULL` to use common name
    const char         *org_unit,	// I - Organizational unit or `NULL` for none
    const char         *locality,	// I - City/town or `NULL` for "Unknown"
    const char         *state_province,	// I - State/province or `NULL` for "Unknown"
    const char         *country,	// I - Country or `NULL` for locale-based default
    const char         *common_name,	// I - Common name
    size_t             num_alt_names,	// I - Number of subject alternate names
    const char * const *alt_names,	// I - Subject Alternate Names
    const char         *root_name,	// I - Root certificate/domain name or `NULL` for site/self-signed
    time_t             expiration_date)	// I - Expiration date
{
  gnutls_x509_crt_t	crt;		// New certificate
  gnutls_x509_privkey_t	key;		// Encryption private key
  gnutls_x509_crt_t	root_crt = NULL;// Root certificate
  gnutls_x509_privkey_t	root_key = NULL;// Root private key
  char			temp[1024],	// Temporary directory name
 			crtfile[1024],	// Certificate filename
			keyfile[1024],	// Private key filename
 			*root_crtdata,	// Root certificate data
			*root_keydata;	// Root private key data
  unsigned		gnutls_usage = 0;// GNU TLS keyUsage bits
  cups_file_t		*fp;		// Key/cert file
  unsigned char		buffer[65536];	// Buffer for x509 data
  size_t		bytes;		// Number of bytes of data
  unsigned char		serial[4];	// Serial number buffer
  time_t		curtime;	// Current time
  int			result;		// Result of GNU TLS calls


  DEBUG_printf(("cupsCreateCredentials(path=\"%s\", ca_cert=%s, purpose=0x%x, type=%d, usage=0x%x, organization=\"%s\", org_unit=\"%s\", locality=\"%s\", state_province=\"%s\", country=\"%s\", common_name=\"%s\", num_alt_names=%u, alt_names=%p, root_name=\"%s\", expiration_date=%ld)", path, ca_cert ? "true" : "false", purpose, type, usage, organization, org_unit, locality, state_province, country, common_name, (unsigned)num_alt_names, alt_names, root_name, (long)expiration_date));

  // Filenames...
  if (!path)
    path = http_default_path(temp, sizeof(temp));

  if (!path || !common_name)
  {
    _cupsSetError(IPP_STATUS_ERROR_INTERNAL, strerror(EINVAL), 0);
    return (false);
  }

  http_make_path(crtfile, sizeof(crtfile), path, common_name, "crt");
  http_make_path(keyfile, sizeof(keyfile), path, common_name, "key");

  // Create the encryption key...
  DEBUG_puts("1cupsCreateCredentials: Creating key pair.");

  key = gnutls_create_key(type);

  DEBUG_puts("1cupsCreateCredentials: Key pair created.");

  // Save it...
  bytes = sizeof(buffer);

  if ((result = gnutls_x509_privkey_export(key, GNUTLS_X509_FMT_PEM, buffer, &bytes)) < 0)
  {
    DEBUG_printf(("1cupsCreateCredentials: Unable to export private key: %s", gnutls_strerror(result)));
    _cupsSetError(IPP_STATUS_ERROR_INTERNAL, gnutls_strerror(result), 0);
    gnutls_x509_privkey_deinit(key);
    return (false);
  }
  else if ((fp = cupsFileOpen(keyfile, "w")) != NULL)
  {
    DEBUG_printf(("1cupsCreateCredentials: Writing private key to \"%s\".", keyfile));
    cupsFileWrite(fp, (char *)buffer, bytes);
    cupsFileClose(fp);
  }
  else
  {
    DEBUG_printf(("1cupsCreateCredentials: Unable to create private key file \"%s\": %s", keyfile, strerror(errno)));
    _cupsSetError(IPP_STATUS_ERROR_INTERNAL, strerror(errno), 0);
    gnutls_x509_privkey_deinit(key);
    return (false);
  }

  // Create the certificate...
  DEBUG_puts("1cupsCreateCredentials: Generating X.509 certificate.");

  curtime   = time(NULL);
  serial[0] = (unsigned char)(curtime >> 24);
  serial[1] = (unsigned char)(curtime >> 16);
  serial[2] = (unsigned char)(curtime >> 8);
  serial[3] = (unsigned char)(curtime);

  if (!organization)
    organization = common_name;
  if (!org_unit)
    org_unit = "";
  if (!locality)
    locality = "Unknown";
  if (!state_province)
    state_province = "Unknown";
  if (!country)
    country = "US";

  gnutls_x509_crt_init(&crt);
  gnutls_x509_crt_set_dn_by_oid(crt, GNUTLS_OID_X520_COUNTRY_NAME, 0, "US", 2);
  gnutls_x509_crt_set_dn_by_oid(crt, GNUTLS_OID_X520_COMMON_NAME, 0, common_name, strlen(common_name));
  gnutls_x509_crt_set_dn_by_oid(crt, GNUTLS_OID_X520_ORGANIZATION_NAME, 0, organization, strlen(organization));
  gnutls_x509_crt_set_dn_by_oid(crt, GNUTLS_OID_X520_ORGANIZATIONAL_UNIT_NAME, 0, org_unit, strlen(org_unit));
  gnutls_x509_crt_set_dn_by_oid(crt, GNUTLS_OID_X520_STATE_OR_PROVINCE_NAME, 0, state_province, strlen(state_province));
  gnutls_x509_crt_set_dn_by_oid(crt, GNUTLS_OID_X520_LOCALITY_NAME, 0, locality, strlen(locality));
  gnutls_x509_crt_set_key(crt, key);
  gnutls_x509_crt_set_serial(crt, serial, sizeof(serial));
  gnutls_x509_crt_set_activation_time(crt, curtime);
  gnutls_x509_crt_set_expiration_time(crt, expiration_date);
  gnutls_x509_crt_set_ca_status(crt, ca_cert ? 1 : 0);
  gnutls_x509_crt_set_subject_alt_name(crt, GNUTLS_SAN_DNSNAME, common_name, (unsigned)strlen(common_name), GNUTLS_FSAN_SET);
  if (!strchr(common_name, '.'))
  {
    // Add common_name.local to the list, too...
    char localname[256];                // hostname.local

    snprintf(localname, sizeof(localname), "%s.local", common_name);
    gnutls_x509_crt_set_subject_alt_name(crt, GNUTLS_SAN_DNSNAME, localname, (unsigned)strlen(localname), GNUTLS_FSAN_APPEND);
  }
  gnutls_x509_crt_set_subject_alt_name(crt, GNUTLS_SAN_DNSNAME, "localhost", 9, GNUTLS_FSAN_APPEND);
  if (num_alt_names > 0)
  {
    size_t i;				// Looping var

    for (i = 0; i < num_alt_names; i ++)
    {
      if (strcmp(alt_names[i], "localhost"))
      {
        gnutls_x509_crt_set_subject_alt_name(crt, GNUTLS_SAN_DNSNAME, alt_names[i], (unsigned)strlen(alt_names[i]), GNUTLS_FSAN_APPEND);
      }
    }
  }

  if (purpose & CUPS_CREDPURPOSE_SERVER_AUTH)
    gnutls_x509_crt_set_key_purpose_oid(crt, GNUTLS_KP_TLS_WWW_SERVER, 0);
  if (purpose & CUPS_CREDPURPOSE_CLIENT_AUTH)
    gnutls_x509_crt_set_key_purpose_oid(crt, GNUTLS_KP_TLS_WWW_CLIENT, 0);
  if (purpose & CUPS_CREDPURPOSE_CODE_SIGNING)
    gnutls_x509_crt_set_key_purpose_oid(crt, GNUTLS_KP_CODE_SIGNING, 0);
  if (purpose & CUPS_CREDPURPOSE_EMAIL_PROTECTION)
    gnutls_x509_crt_set_key_purpose_oid(crt, GNUTLS_KP_EMAIL_PROTECTION, 0);
  if (purpose & CUPS_CREDPURPOSE_OCSP_SIGNING)
    gnutls_x509_crt_set_key_purpose_oid(crt, GNUTLS_KP_OCSP_SIGNING, 0);

  if (usage & CUPS_CREDUSAGE_DIGITAL_SIGNATURE)
    gnutls_usage |= GNUTLS_KEY_DIGITAL_SIGNATURE;
  if (usage & CUPS_CREDUSAGE_NON_REPUDIATION)
    gnutls_usage |= GNUTLS_KEY_NON_REPUDIATION;
  if (usage & CUPS_CREDUSAGE_KEY_ENCIPHERMENT)
    gnutls_usage |= GNUTLS_KEY_KEY_ENCIPHERMENT;
  if (usage & CUPS_CREDUSAGE_DATA_ENCIPHERMENT)
    gnutls_usage |= GNUTLS_KEY_DATA_ENCIPHERMENT;
  if (usage & CUPS_CREDUSAGE_KEY_AGREEMENT)
    gnutls_usage |= GNUTLS_KEY_KEY_AGREEMENT;
  if (usage & CUPS_CREDUSAGE_KEY_CERT_SIGN)
    gnutls_usage |= GNUTLS_KEY_KEY_CERT_SIGN;
  if (usage & CUPS_CREDUSAGE_CRL_SIGN)
    gnutls_usage |= GNUTLS_KEY_CRL_SIGN;
  if (usage & CUPS_CREDUSAGE_ENCIPHER_ONLY)
    gnutls_usage |= GNUTLS_KEY_ENCIPHER_ONLY;
  if (usage & CUPS_CREDUSAGE_DECIPHER_ONLY)
    gnutls_usage |= GNUTLS_KEY_DECIPHER_ONLY;
 
  gnutls_x509_crt_set_key_usage(crt, gnutls_usage);
  gnutls_x509_crt_set_version(crt, 3);

  bytes = sizeof(buffer);
  if (gnutls_x509_crt_get_key_id(crt, 0, buffer, &bytes) >= 0)
    gnutls_x509_crt_set_subject_key_id(crt, buffer, bytes);

  // Try loading a root certificate...
  if (!ca_cert)
  {
    root_crtdata = cupsCopyCredentials(path, root_name ? root_name : "_site_");
    root_keydata = cupsCopyCredentialsKey(path, root_name ? root_name : "_site_");

    if (root_crtdata && root_keydata)
    {
      // Load root certificate...
      gnutls_datum_t    datum;          // Datum for cert/key

      datum.data = (unsigned char *)root_crtdata;
      datum.size = strlen(root_crtdata);

      gnutls_x509_crt_init(&root_crt);
      if (gnutls_x509_crt_import(root_crt, &datum, GNUTLS_X509_FMT_PEM) < 0)
      {
        // No good, clear it...
        gnutls_x509_crt_deinit(root_crt);
        root_crt = NULL;
      }
      else
      {
        // Load root private key...
        datum.data = (unsigned char *)root_keydata;
        datum.size = strlen(root_keydata);

        gnutls_x509_privkey_init(&root_key);
        if (gnutls_x509_privkey_import(root_key, &datum, GNUTLS_X509_FMT_PEM) < 0)
        {
          // No food, clear them...
          gnutls_x509_privkey_deinit(root_key);
          root_key = NULL;

          gnutls_x509_crt_deinit(root_crt);
          root_crt = NULL;
        }
      }
    }

    free(root_crtdata);
    free(root_keydata);
  }

  if (root_crt && root_key)
  {
    gnutls_x509_crt_sign(crt, root_crt, root_key);
    gnutls_x509_crt_deinit(root_crt);
    gnutls_x509_privkey_deinit(root_key);
  }
  else
  {
    gnutls_x509_crt_sign(crt, crt, key);
  }

  // Save it...
  bytes = sizeof(buffer);
  if ((result = gnutls_x509_crt_export(crt, GNUTLS_X509_FMT_PEM, buffer, &bytes)) < 0)
  {
    DEBUG_printf(("1cupsCreateCredentials: Unable to export public key and X.509 certificate: %s", gnutls_strerror(result)));
    _cupsSetError(IPP_STATUS_ERROR_INTERNAL, gnutls_strerror(result), 0);
    gnutls_x509_crt_deinit(crt);
    gnutls_x509_privkey_deinit(key);
    return (false);
  }
  else if ((fp = cupsFileOpen(crtfile, "w")) != NULL)
  {
    DEBUG_printf(("1cupsCreateCredentials: Writing public key and X.509 certificate to \"%s\".", crtfile));
    cupsFileWrite(fp, (char *)buffer, bytes);
    cupsFileClose(fp);
  }
  else
  {
    DEBUG_printf(("1cupsCreateCredentials: Unable to create public key and X.509 certificate file \"%s\": %s", crtfile, strerror(errno)));
    _cupsSetError(IPP_STATUS_ERROR_INTERNAL, strerror(errno), 0);
    gnutls_x509_crt_deinit(crt);
    gnutls_x509_privkey_deinit(key);
    return (false);
  }

  // Cleanup...
  gnutls_x509_crt_deinit(crt);
  gnutls_x509_privkey_deinit(key);

  DEBUG_puts("1cupsCreateCredentials: Successfully created credentials.");

  return (true);
}


//
// 'cupsCreateCredentialsRequest()' - Make an X.509 Certificate Signing Request.
//
// This function creates an X.509 certificate signing request (CSR) and
// associated private key.  The CSR and key are stored in the directory "path"
// or, if "path" is `NULL`, in a per-user or system-wide (when running as root)
// certificate/key store.
//

bool					// O - `true` on success, `false` on error
cupsCreateCredentialsRequest(
    const char         *path,		// I - Directory path for certificate/key store or `NULL` for default
    cups_credpurpose_t purpose,		// I - Credential purposes
    cups_credtype_t    type,		// I - Credential type
    cups_credusage_t   usage,		// I - Credential usages
    const char         *organization,	// I - Organization or `NULL` to use common name
    const char         *org_unit,	// I - Organizational unit or `NULL` for none
    const char         *locality,	// I - City/town or `NULL` for "Unknown"
    const char         *state_province,	// I - State/province or `NULL` for "Unknown"
    const char         *country,	// I - Country or `NULL` for locale-based default
    const char         *common_name,	// I - Common name
    size_t             num_alt_names,	// I - Number of subject alternate names
    const char * const *alt_names)	// I - Subject Alternate Names
{
  (void)path;
  (void)purpose;
  (void)type;
  (void)usage;
  (void)organization;
  (void)org_unit;
  (void)locality;
  (void)state_province;
  (void)country;
  (void)common_name;
  (void)num_alt_names;
  (void)alt_names;

  return (false);
}


//
// 'httpCopyCredentials()' - Copy the credentials associated with the peer in
//                           an encrypted connection.
//

bool					// O - `true` on success, `false` on failure
httpCopyCredentials(
    http_t	 *http,			// I - Connection to server
    cups_array_t **credentials)		// O - Array of credentials
{
  unsigned		count;		// Number of certificates
  const gnutls_datum_t *certs;		// Certificates


  DEBUG_printf(("httpCopyCredentials(http=%p, credentials=%p)", http, credentials));

  if (credentials)
    *credentials = NULL;

  if (!http || !http->tls || !credentials)
    return (false);

  *credentials = cupsArrayNew(NULL, NULL, NULL, 0, NULL, NULL);
  certs        = gnutls_certificate_get_peers(http->tls, &count);

  DEBUG_printf(("1httpCopyCredentials: certs=%p, count=%u", certs, count));

  if (certs && count)
  {
    while (count > 0)
    {
      httpAddCredential(*credentials, certs->data, certs->size);
      certs ++;
      count --;
    }
  }

  return (true);
}


//
// '_httpCreateCredentials()' - Create credentials in the internal format.
//

http_tls_credentials_t			// O - Internal credentials
_httpCreateCredentials(
    cups_array_t *credentials)		// I - Array of credentials
{
  (void)credentials;

  return (NULL);
}


//
// 'httpCredentialsAreValidForName()' - Return whether the credentials are valid for the given name.
//

bool					// O - `true` if valid, `false` otherwise
httpCredentialsAreValidForName(
    cups_array_t *credentials,		// I - Credentials
    const char   *common_name)		// I - Name to check
{
  gnutls_x509_crt_t	cert;		// Certificate
  bool			result = false;	// Result


  cert = gnutls_create_credential((http_credential_t *)cupsArrayGetFirst(credentials));
  if (cert)
  {
    result = gnutls_x509_crt_check_hostname(cert, common_name) != 0;

    if (result)
    {
      gnutls_x509_crl_iter_t iter = NULL;
					// Iterator
      unsigned char	cserial[1024],	// Certificate serial number
			rserial[1024];	// Revoked serial number
      size_t		cserial_size,	// Size of cert serial number
			rserial_size;	// Size of revoked serial number

      cupsMutexLock(&tls_mutex);

      if (gnutls_x509_crl_get_crt_count(tls_crl) > 0)
      {
        cserial_size = sizeof(cserial);
        gnutls_x509_crt_get_serial(cert, cserial, &cserial_size);

	rserial_size = sizeof(rserial);

        while (!gnutls_x509_crl_iter_crt_serial(tls_crl, &iter, rserial, &rserial_size, NULL))
        {
          if (cserial_size == rserial_size && !memcmp(cserial, rserial, rserial_size))
	  {
	    result = false;
	    break;
	  }

	  rserial_size = sizeof(rserial);
	}
	gnutls_x509_crl_iter_deinit(iter);
      }

      cupsMutexUnlock(&tls_mutex);
    }

    gnutls_x509_crt_deinit(cert);
  }

  return (result);
}


//
// 'httpCredentialsGetTrust()' - Return the trust of credentials.
//

http_trust_t				// O - Level of trust
httpCredentialsGetTrust(
    cups_array_t *credentials,		// I - Credentials
    const char   *common_name)		// I - Common name for trust lookup
{
  http_trust_t		trust = HTTP_TRUST_OK;
					// Trusted?
  gnutls_x509_crt_t	cert;		// Certificate
  cups_array_t		*tcreds = NULL;	// Trusted credentials
  _cups_globals_t	*cg = _cupsGlobals();
					// Per-thread globals


  if (!common_name)
  {
    _cupsSetError(IPP_STATUS_ERROR_INTERNAL, _("No common name specified."), 1);
    return (HTTP_TRUST_UNKNOWN);
  }

  if ((cert = gnutls_create_credential((http_credential_t *)cupsArrayGetFirst(credentials))) == NULL)
  {
    _cupsSetError(IPP_STATUS_ERROR_INTERNAL, _("Unable to create credentials from array."), 1);
    return (HTTP_TRUST_UNKNOWN);
  }

  if (cg->any_root < 0)
  {
    _cupsSetDefaults();
    gnutls_load_crl();
  }

  // Look this common name up in the default keychains...
  httpLoadCredentials(NULL, &tcreds, common_name);

  if (tcreds)
  {
    char	credentials_str[1024],	// String for incoming credentials
		tcreds_str[1024];	// String for saved credentials

    httpCredentialsString(credentials, credentials_str, sizeof(credentials_str));
    httpCredentialsString(tcreds, tcreds_str, sizeof(tcreds_str));

    if (strcmp(credentials_str, tcreds_str))
    {
      // Credentials don't match, let's look at the expiration date of the new
      // credentials and allow if the new ones have a later expiration...
      if (!cg->trust_first)
      {
        // Do not trust certificates on first use...
        _cupsSetError(IPP_STATUS_ERROR_INTERNAL, _("Trust on first use is disabled."), 1);

        trust = HTTP_TRUST_INVALID;
      }
      else if (httpCredentialsGetExpiration(credentials) <= httpCredentialsGetExpiration(tcreds))
      {
        // The new credentials are not newly issued...
        _cupsSetError(IPP_STATUS_ERROR_INTERNAL, _("New credentials are older than stored credentials."), 1);

        trust = HTTP_TRUST_INVALID;
      }
      else if (!httpCredentialsAreValidForName(credentials, common_name))
      {
        // The common name does not match the issued certificate...
        _cupsSetError(IPP_STATUS_ERROR_INTERNAL, _("New credentials are not valid for name."), 1);

        trust = HTTP_TRUST_INVALID;
      }
      else if (httpCredentialsGetExpiration(tcreds) < time(NULL))
      {
        // Save the renewed credentials...
	trust = HTTP_TRUST_RENEWED;

        httpSaveCredentials(NULL, credentials, common_name);
      }
    }

    httpFreeCredentials(tcreds);
  }
  else if (cg->validate_certs && !httpCredentialsAreValidForName(credentials, common_name))
  {
    _cupsSetError(IPP_STATUS_ERROR_INTERNAL, _("No stored credentials, not valid for name."), 1);
    trust = HTTP_TRUST_INVALID;
  }
  else if (!cg->trust_first)
  {
    // See if we have a site CA certificate we can compare...
    if (!httpLoadCredentials(NULL, &tcreds, "site"))
    {
      if (cupsArrayGetCount(credentials) != (cupsArrayGetCount(tcreds) + 1))
      {
        // Certificate isn't directly generated from the CA cert...
        trust = HTTP_TRUST_INVALID;
      }
      else
      {
        // Do a tail comparison of the two certificates...
        http_credential_t *a, *b;	// Certificates

        for (a = (http_credential_t *)cupsArrayGetFirst(tcreds), b = (http_credential_t *)cupsArrayGetElement(credentials, 1); a && b; a = (http_credential_t *)cupsArrayGetNext(tcreds), b = (http_credential_t *)cupsArrayGetNext(credentials))
        {
	  if (a->datalen != b->datalen || memcmp(a->data, b->data, a->datalen))
	    break;
	}

        if (a || b)
	  trust = HTTP_TRUST_INVALID;
      }

      if (trust != HTTP_TRUST_OK)
	_cupsSetError(IPP_STATUS_ERROR_INTERNAL, _("Credentials do not validate against site CA certificate."), 1);
    }
    else
    {
      _cupsSetError(IPP_STATUS_ERROR_INTERNAL, _("Trust on first use is disabled."), 1);
      trust = HTTP_TRUST_INVALID;
    }
  }

  if (trust == HTTP_TRUST_OK && !cg->expired_certs)
  {
    time_t	curtime;		// Current date/time

    time(&curtime);
    if (curtime < gnutls_x509_crt_get_activation_time(cert) ||
        curtime > gnutls_x509_crt_get_expiration_time(cert))
    {
      _cupsSetError(IPP_STATUS_ERROR_INTERNAL, _("Credentials have expired."), 1);
      trust = HTTP_TRUST_EXPIRED;
    }
  }

  if (trust == HTTP_TRUST_OK && !cg->any_root && cupsArrayGetCount(credentials) == 1)
  {
    _cupsSetError(IPP_STATUS_ERROR_INTERNAL, _("Self-signed credentials are blocked."), 1);
    trust = HTTP_TRUST_INVALID;
  }

  gnutls_x509_crt_deinit(cert);

  return (trust);
}


//
// 'httpCredentialsGetExpiration()' - Return the expiration date of the credentials.
//

time_t					// O - Expiration date of credentials
httpCredentialsGetExpiration(
    cups_array_t *credentials)		// I - Credentials
{
  gnutls_x509_crt_t	cert;		// Certificate
  time_t		result = 0;	// Result


  cert = gnutls_create_credential((http_credential_t *)cupsArrayGetFirst(credentials));
  if (cert)
  {
    result = gnutls_x509_crt_get_expiration_time(cert);
    gnutls_x509_crt_deinit(cert);
  }

  return (result);
}


//
// 'httpCredentialsString()' - Return a string representing the credentials.
//

size_t					// O - Total size of credentials string
httpCredentialsString(
    cups_array_t *credentials,		// I - Credentials
    char         *buffer,		// I - Buffer or @code NULL@
    size_t       bufsize)		// I - Size of buffer
{
  http_credential_t	*first;		// First certificate
  gnutls_x509_crt_t	cert;		// Certificate


  DEBUG_printf(("httpCredentialsString(credentials=%p, buffer=%p, bufsize=" CUPS_LLFMT ")", credentials, buffer, CUPS_LLCAST bufsize));

  if (!buffer)
    return (0);

  if (bufsize > 0)
    *buffer = '\0';

  if ((first = (http_credential_t *)cupsArrayGetFirst(credentials)) != NULL &&
      (cert = gnutls_create_credential(first)) != NULL)
  {
    char		name[256],	// Common name associated with cert
			issuer[256];	// Issuer associated with cert
    size_t		len;		// Length of string
    time_t		expiration;	// Expiration date of cert
    char		expstr[256];	// Expiration date as string */
    int			sigalg;		// Signature algorithm
    unsigned char	md5_digest[16];	// MD5 result

    len = sizeof(name) - 1;
    if (gnutls_x509_crt_get_dn_by_oid(cert, GNUTLS_OID_X520_COMMON_NAME, 0, 0, name, &len) >= 0)
      name[len] = '\0';
    else
      cupsCopyString(name, "unknown", sizeof(name));

    len = sizeof(issuer) - 1;
    if (gnutls_x509_crt_get_issuer_dn_by_oid(cert, GNUTLS_OID_X520_ORGANIZATION_NAME, 0, 0, issuer, &len) >= 0)
      issuer[len] = '\0';
    else
      cupsCopyString(issuer, "unknown", sizeof(issuer));

    expiration = gnutls_x509_crt_get_expiration_time(cert);
    sigalg     = gnutls_x509_crt_get_signature_algorithm(cert);

    cupsHashData("md5", first->data, first->datalen, md5_digest, sizeof(md5_digest));

    snprintf(buffer, bufsize, "%s (issued by %s) / %s / %s / %02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X", name, issuer, httpGetDateString(expiration, expstr, sizeof(expstr)), gnutls_sign_get_name((gnutls_sign_algorithm_t)sigalg), md5_digest[0], md5_digest[1], md5_digest[2], md5_digest[3], md5_digest[4], md5_digest[5], md5_digest[6], md5_digest[7], md5_digest[8], md5_digest[9], md5_digest[10], md5_digest[11], md5_digest[12], md5_digest[13], md5_digest[14], md5_digest[15]);

    gnutls_x509_crt_deinit(cert);
  }

  DEBUG_printf(("1httpCredentialsString: Returning \"%s\".", buffer));

  return (strlen(buffer));
}


//
// '_httpFreeCredentials()' - Free internal credentials.
//

void
_httpFreeCredentials(
    http_tls_credentials_t credentials)	// I - Internal credentials
{
  (void)credentials;
}


//
// 'gnutls_create_credential()' - Create a single credential in the internal format.
//

static gnutls_x509_crt_t			// O - Certificate
gnutls_create_credential(
    http_credential_t *credential)		// I - Credential
{
  int			result;			// Result from GNU TLS
  gnutls_x509_crt_t	cert;			// Certificate
  gnutls_datum_t	datum;			// Data record


  DEBUG_printf(("3gnutls_create_credential(credential=%p)", credential));

  if (!credential)
    return (NULL);

  if ((result = gnutls_x509_crt_init(&cert)) < 0)
  {
    DEBUG_printf(("4gnutls_create_credential: init error: %s", gnutls_strerror(result)));
    return (NULL);
  }

  datum.data = credential->data;
  datum.size = credential->datalen;

  if ((result = gnutls_x509_crt_import(cert, &datum, GNUTLS_X509_FMT_DER)) < 0)
  {
    DEBUG_printf(("4gnutls_create_credential: import error: %s", gnutls_strerror(result)));

    gnutls_x509_crt_deinit(cert);
    return (NULL);
  }

  return (cert);
}


//
// 'gnutls_create_key()' - Create a private key.
//

static gnutls_x509_privkey_t		// O - Private key
gnutls_create_key(cups_credtype_t type)	// I - Type of key
{
  gnutls_x509_privkey_t	key;		// Private key


  gnutls_x509_privkey_init(&key);

  switch (type)
  {
    case CUPS_CREDTYPE_ECDSA_P256_SHA256 :
	gnutls_x509_privkey_generate(key, GNUTLS_PK_ECDSA, GNUTLS_CURVE_TO_BITS(GNUTLS_ECC_CURVE_SECP256R1), 0);
	break;

    case CUPS_CREDTYPE_ECDSA_P384_SHA256 :
	gnutls_x509_privkey_generate(key, GNUTLS_PK_ECDSA, GNUTLS_CURVE_TO_BITS(GNUTLS_ECC_CURVE_SECP384R1), 0);
	break;

    case CUPS_CREDTYPE_ECDSA_P521_SHA256 :
	gnutls_x509_privkey_generate(key, GNUTLS_PK_ECDSA, GNUTLS_CURVE_TO_BITS(GNUTLS_ECC_CURVE_SECP521R1), 0);
	break;

    case CUPS_CREDTYPE_RSA_2048_SHA256 :
	gnutls_x509_privkey_generate(key, GNUTLS_PK_RSA, 2048, 0);
	break;

    default :
    case CUPS_CREDTYPE_RSA_3072_SHA256 :
	gnutls_x509_privkey_generate(key, GNUTLS_PK_RSA, 3072, 0);
	break;

    case CUPS_CREDTYPE_RSA_4096_SHA256 :
	gnutls_x509_privkey_generate(key, GNUTLS_PK_RSA, 4096, 0);
	break;
  }

  return (key);
}


//
// 'gnutls_load_crl()' - Load the certificate revocation list, if any.
//

static void
gnutls_load_crl(void)
{
  cupsMutexLock(&tls_mutex);

  if (!gnutls_x509_crl_init(&tls_crl))
  {
    cups_file_t		*fp;		// CRL file
    char		filename[1024],	// site.crl
			line[256];	// Base64-encoded line
    unsigned char	*data = NULL;	// Buffer for cert data
    size_t		alloc_data = 0,	// Bytes allocated
			num_data = 0;	// Bytes used
    size_t		decoded;	// Bytes decoded
    gnutls_datum_t	datum;		// Data record


    http_make_path(filename, sizeof(filename), CUPS_SERVERROOT, "site", "crl");

    if ((fp = cupsFileOpen(filename, "r")) != NULL)
    {
      while (cupsFileGets(fp, line, sizeof(line)))
      {
	if (!strcmp(line, "-----BEGIN X509 CRL-----"))
	{
	  if (num_data)
	  {
	    // Missing END X509 CRL...
	    break;
	  }
	}
	else if (!strcmp(line, "-----END X509 CRL-----"))
	{
	  if (!num_data)
	  {
	    // Missing data...
	    break;
	  }

          datum.data = data;
	  datum.size = num_data;

	  gnutls_x509_crl_import(tls_crl, &datum, GNUTLS_X509_FMT_PEM);

	  num_data = 0;
	}
	else
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
	      break;

	    data       = tdata;
	    alloc_data += 1024;
	  }

	  decoded = (size_t)(alloc_data - num_data);
	  httpDecode64((char *)data + num_data, &decoded, line, NULL);
	  num_data += (size_t)decoded;
	}
      }

      cupsFileClose(fp);

      if (data)
	free(data);
    }
  }

  cupsMutexUnlock(&tls_mutex);
}


//
// 'gnutls_http_read()' - Read function for the GNU TLS library.
//

static ssize_t				// O - Number of bytes read or -1 on error
gnutls_http_read(
    gnutls_transport_ptr_t ptr,		// I - Connection to server
    void                   *data,	// I - Buffer
    size_t                 length)	// I - Number of bytes to read
{
  http_t	*http;			// HTTP connection
  ssize_t	bytes;			// Bytes read


  DEBUG_printf(("5gnutls_http_read(ptr=%p, data=%p, length=%d)", ptr, data, (int)length));

  http = (http_t *)ptr;

  if (!http->blocking || http->timeout_value > 0.0)
  {
    // Make sure we have data before we read...
    while (!_httpWait(http, http->wait_value, 0))
    {
      if (http->timeout_cb && (*http->timeout_cb)(http, http->timeout_data))
	continue;

      http->error = ETIMEDOUT;
      return (-1);
    }
  }

  bytes = recv(http->fd, data, length, 0);
  DEBUG_printf(("5gnutls_http_read: bytes=%d", (int)bytes));
  return (bytes);
}


//
// 'gnutls_http_write()' - Write function for the GNU TLS library.
//

static ssize_t				// O - Number of bytes written or -1 on error
gnutls_http_write(
    gnutls_transport_ptr_t ptr,		// I - Connection to server
    const void             *data,	// I - Data buffer
    size_t                 length)	// I - Number of bytes to write
{
  ssize_t bytes;			// Bytes written


  DEBUG_printf(("5gnutls_http_write(ptr=%p, data=%p, length=%d)", ptr, data,
                (int)length));
  bytes = send(((http_t *)ptr)->fd, data, length, 0);
  DEBUG_printf(("5gnutls_http_write: bytes=%d", (int)bytes));

  return (bytes);
}


//
// '_httpTLSInitialize()' - Initialize the TLS stack.
//

void
_httpTLSInitialize(void)
{
  // Initialize GNU TLS...
  gnutls_global_init();
}


//
// '_httpTLSPending()' - Return the number of pending TLS-encrypted bytes.
//

size_t					// O - Bytes available
_httpTLSPending(http_t *http)		// I - HTTP connection
{
  return (gnutls_record_check_pending(http->tls));
}


//
// '_httpTLSRead()' - Read from a SSL/TLS connection.
//

int					// O - Bytes read
_httpTLSRead(http_t *http,		// I - Connection to server
	     char   *buf,		// I - Buffer to store data
	     int    len)		// I - Length of buffer
{
  ssize_t	result;			// Return value


  result = gnutls_record_recv(http->tls, buf, (size_t)len);

  if (result < 0 && !errno)
  {
    // Convert GNU TLS error to errno value...
    switch (result)
    {
      case GNUTLS_E_INTERRUPTED :
	  errno = EINTR;
	  break;

      case GNUTLS_E_AGAIN :
          errno = EAGAIN;
          break;

      default :
          errno = EPIPE;
          break;
    }

    result = -1;
  }

  return ((int)result);
}


//
// '_httpTLSStart()' - Set up SSL/TLS support on a connection.
//

bool					// O - `true` on success, `false` on failure
_httpTLSStart(http_t *http)		// I - Connection to server
{
  char			hostname[256],	// Hostname
			*hostptr;	// Pointer into hostname
  int			status;		// Status of handshake
  gnutls_certificate_credentials_t *credentials;
					// TLS credentials
  char			priority_string[2048];
					// Priority string
  int			version;	// Current version
  double		old_timeout;	// Old timeout value
  http_timeout_cb_t	old_cb;		// Old timeout callback
  void			*old_data;	// Old timeout data
  static const char * const versions[] =// SSL/TLS versions
  {
    "VERS-SSL3.0",
    "VERS-TLS1.0",
    "VERS-TLS1.1",
    "VERS-TLS1.2",
    "VERS-TLS1.3",
    "VERS-TLS-ALL"
  };


  DEBUG_printf(("3_httpTLSStart(http=%p)", http));

  if (tls_options < 0)
  {
    DEBUG_puts("4_httpTLSStart: Setting defaults.");
    _cupsSetDefaults();
    DEBUG_printf(("4_httpTLSStart: tls_options=%x", tls_options));
  }

  if (http->mode == _HTTP_MODE_SERVER && !tls_keypath)
  {
    DEBUG_puts("4_httpTLSStart: cupsSetServerCredentials not called.");
    http->error  = errno = EINVAL;
    http->status = HTTP_STATUS_ERROR;
    _cupsSetError(IPP_STATUS_ERROR_INTERNAL, _("Server credentials not set."), 1);

    return (false);
  }

  credentials = (gnutls_certificate_credentials_t *)
                    malloc(sizeof(gnutls_certificate_credentials_t));
  if (credentials == NULL)
  {
    DEBUG_printf(("8_httpStartTLS: Unable to allocate credentials: %s",
                  strerror(errno)));
    http->error  = errno;
    http->status = HTTP_STATUS_ERROR;
    _cupsSetHTTPError(HTTP_STATUS_ERROR);

    return (false);
  }

  gnutls_certificate_allocate_credentials(credentials);
  status = gnutls_init(&http->tls, http->mode == _HTTP_MODE_CLIENT ? GNUTLS_CLIENT : GNUTLS_SERVER);
  if (!status)
    status = gnutls_set_default_priority(http->tls);

  if (status)
  {
    http->error  = EIO;
    http->status = HTTP_STATUS_ERROR;

    DEBUG_printf(("4_httpTLSStart: Unable to initialize common TLS parameters: %s", gnutls_strerror(status)));
    _cupsSetError(IPP_STATUS_ERROR_CUPS_PKI, gnutls_strerror(status), 0);

    gnutls_deinit(http->tls);
    gnutls_certificate_free_credentials(*credentials);
    free(credentials);
    http->tls = NULL;

    return (false);
  }

  if (http->mode == _HTTP_MODE_CLIENT)
  {
    // Client: get the hostname to use for TLS...
    if (httpAddrIsLocalhost(http->hostaddr))
    {
      cupsCopyString(hostname, "localhost", sizeof(hostname));
    }
    else
    {
      // Otherwise make sure the hostname we have does not end in a trailing dot.
      cupsCopyString(hostname, http->hostname, sizeof(hostname));
      if ((hostptr = hostname + strlen(hostname) - 1) >= hostname && *hostptr == '.')
	*hostptr = '\0';
    }

    status = gnutls_server_name_set(http->tls, GNUTLS_NAME_DNS, hostname, strlen(hostname));
  }
  else
  {
    // Server: get certificate and private key...
    char	crtfile[1024],		// Certificate file
		keyfile[1024];		// Private key file
    const char	*cn,			// Common name to lookup
		*cnptr;			// Pointer into common name
    bool	have_creds = false;	// Have credentials?

    if (http->fields[HTTP_FIELD_HOST])
    {
      // Use hostname for TLS upgrade...
      cupsCopyString(hostname, http->fields[HTTP_FIELD_HOST], sizeof(hostname));
    }
    else
    {
      // Resolve hostname from connection address...
      http_addr_t	addr;		// Connection address
      socklen_t		addrlen;	// Length of address

      addrlen = sizeof(addr);
      if (getsockname(http->fd, (struct sockaddr *)&addr, &addrlen))
      {
	DEBUG_printf(("4_httpTLSStart: Unable to get socket address: %s", strerror(errno)));
	hostname[0] = '\0';
      }
      else if (httpAddrIsLocalhost(&addr))
      {
	hostname[0] = '\0';
      }
      else
      {
	httpAddrLookup(&addr, hostname, sizeof(hostname));
        DEBUG_printf(("4_httpTLSStart: Resolved socket address to \"%s\".", hostname));
      }
    }

    if (isdigit(hostname[0] & 255) || hostname[0] == '[')
      hostname[0] = '\0';		// Don't allow numeric addresses

    cupsMutexLock(&tls_mutex);

    if (hostname[0])
      cn = hostname;
    else
      cn = tls_common_name;

    if (cn)
    {
      // First look in the CUPS keystore...
      http_make_path(crtfile, sizeof(crtfile), tls_keypath, cn, "crt");
      http_make_path(keyfile, sizeof(keyfile), tls_keypath, cn, "key");

      if (access(crtfile, R_OK) || access(keyfile, R_OK))
      {
        // No CUPS-managed certs, look for CA certs...
        char cacrtfile[1024], cakeyfile[1024];	// CA cert files

        snprintf(cacrtfile, sizeof(cacrtfile), "/etc/letsencrypt/live/%s/fullchain.pem", cn);
        snprintf(cakeyfile, sizeof(cakeyfile), "/etc/letsencrypt/live/%s/privkey.pem", cn);

        if ((access(cacrtfile, R_OK) || access(cakeyfile, R_OK)) && (cnptr = strchr(cn, '.')) != NULL)
        {
          // Try just domain name...
          cnptr ++;
          if (strchr(cnptr, '.'))
          {
            snprintf(cacrtfile, sizeof(cacrtfile), "/etc/letsencrypt/live/%s/fullchain.pem", cnptr);
            snprintf(cakeyfile, sizeof(cakeyfile), "/etc/letsencrypt/live/%s/privkey.pem", cnptr);
          }
        }

        if (!access(cacrtfile, R_OK) && !access(cakeyfile, R_OK))
        {
          // Use the CA certs...
          cupsCopyString(crtfile, cacrtfile, sizeof(crtfile));
          cupsCopyString(keyfile, cakeyfile, sizeof(keyfile));
        }
      }

      have_creds = !access(crtfile, R_OK) && !access(keyfile, R_OK);
    }

    if (!have_creds && tls_auto_create && cn)
    {
      DEBUG_printf(("4_httpTLSStart: Auto-create credentials for \"%s\".", cn));

      if (!cupsCreateCredentials(tls_keypath, false, CUPS_CREDPURPOSE_SERVER_AUTH, CUPS_CREDTYPE_DEFAULT, CUPS_CREDUSAGE_DEFAULT_TLS, NULL, NULL, NULL, NULL, NULL, cn, 0, NULL, NULL, time(NULL) + 3650 * 86400))
      {
	DEBUG_puts("4_httpTLSStart: cupsCreateCredentials failed.");
	http->error  = errno = EINVAL;
	http->status = HTTP_STATUS_ERROR;
	_cupsSetError(IPP_STATUS_ERROR_INTERNAL, _("Unable to create server credentials."), 1);
	cupsMutexUnlock(&tls_mutex);

	return (false);
      }
    }

    cupsMutexUnlock(&tls_mutex);

    DEBUG_printf(("4_httpTLSStart: Using certificate \"%s\" and private key \"%s\".", crtfile, keyfile));

    status = gnutls_certificate_set_x509_key_file(*credentials, crtfile, keyfile, GNUTLS_X509_FMT_PEM);
  }

  if (!status)
    status = gnutls_credentials_set(http->tls, GNUTLS_CRD_CERTIFICATE, *credentials);

  if (status)
  {
    http->error  = EIO;
    http->status = HTTP_STATUS_ERROR;

    DEBUG_printf(("4_httpTLSStart: Unable to complete client/server setup: %s", gnutls_strerror(status)));
    _cupsSetError(IPP_STATUS_ERROR_CUPS_PKI, gnutls_strerror(status), 0);

    gnutls_deinit(http->tls);
    gnutls_certificate_free_credentials(*credentials);
    free(credentials);
    http->tls = NULL;

    return (false);
  }

  cupsCopyString(priority_string, "NORMAL", sizeof(priority_string));

  if (tls_max_version < _HTTP_TLS_MAX)
  {
    // Require specific TLS versions...
    cupsConcatString(priority_string, ":-VERS-TLS-ALL", sizeof(priority_string));
    for (version = tls_min_version; version <= tls_max_version; version ++)
    {
      cupsConcatString(priority_string, ":+", sizeof(priority_string));
      cupsConcatString(priority_string, versions[version], sizeof(priority_string));
    }
  }
  else if (tls_min_version == _HTTP_TLS_SSL3)
  {
    // Allow all versions of TLS and SSL/3.0...
    cupsConcatString(priority_string, ":+VERS-TLS-ALL:+VERS-SSL3.0", sizeof(priority_string));
  }
  else
  {
    // Require a minimum version...
    cupsConcatString(priority_string, ":+VERS-TLS-ALL", sizeof(priority_string));
    for (version = 0; version < tls_min_version; version ++)
    {
      cupsConcatString(priority_string, ":-", sizeof(priority_string));
      cupsConcatString(priority_string, versions[version], sizeof(priority_string));
    }
  }

  if (tls_options & _HTTP_TLS_ALLOW_RC4)
    cupsConcatString(priority_string, ":+ARCFOUR-128", sizeof(priority_string));
  else
    cupsConcatString(priority_string, ":!ARCFOUR-128", sizeof(priority_string));

  cupsConcatString(priority_string, ":!ANON-DH", sizeof(priority_string));

  if (tls_options & _HTTP_TLS_DENY_CBC)
    cupsConcatString(priority_string, ":!AES-128-CBC:!AES-256-CBC:!CAMELLIA-128-CBC:!CAMELLIA-256-CBC:!3DES-CBC", sizeof(priority_string));

#ifdef HAVE_GNUTLS_PRIORITY_SET_DIRECT
  gnutls_priority_set_direct(http->tls, priority_string, NULL);

#else
  gnutls_priority_t priority;		// Priority

  gnutls_priority_init(&priority, priority_string, NULL);
  gnutls_priority_set(http->tls, priority);
  gnutls_priority_deinit(priority);
#endif // HAVE_GNUTLS_PRIORITY_SET_DIRECT

  gnutls_transport_set_ptr(http->tls, (gnutls_transport_ptr_t)http);
  gnutls_transport_set_pull_function(http->tls, gnutls_http_read);
#ifdef HAVE_GNUTLS_TRANSPORT_SET_PULL_TIMEOUT_FUNCTION
  gnutls_transport_set_pull_timeout_function(http->tls, (gnutls_pull_timeout_func)httpWait);
#endif // HAVE_GNUTLS_TRANSPORT_SET_PULL_TIMEOUT_FUNCTION
  gnutls_transport_set_push_function(http->tls, gnutls_http_write);

  // Enforce a minimum timeout of 10 seconds for the TLS handshake...
  old_timeout  = http->timeout_value;
  old_cb       = http->timeout_cb;
  old_data     = http->timeout_data;

  if (!old_cb || old_timeout < 10.0)
  {
    DEBUG_puts("4_httpTLSStart: Setting timeout to 10 seconds.");
    httpSetTimeout(http, 10.0, NULL, NULL);
  }

  // Do the TLS handshake...
  while ((status = gnutls_handshake(http->tls)) != GNUTLS_E_SUCCESS)
  {
    DEBUG_printf(("5_httpStartTLS: gnutls_handshake returned %d (%s)",
                  status, gnutls_strerror(status)));

    if (gnutls_error_is_fatal(status))
    {
      http->error  = EIO;
      http->status = HTTP_STATUS_ERROR;

      _cupsSetError(IPP_STATUS_ERROR_CUPS_PKI, gnutls_strerror(status), 0);

      gnutls_deinit(http->tls);
      gnutls_certificate_free_credentials(*credentials);
      free(credentials);
      http->tls = NULL;

      httpSetTimeout(http, old_timeout, old_cb, old_data);

      return (false);
    }
  }

  // Restore the previous timeout settings...
  httpSetTimeout(http, old_timeout, old_cb, old_data);

  http->tls_credentials = credentials;

  return (true);
}


//
// '_httpTLSStop()' - Shut down SSL/TLS on a connection.
//

void
_httpTLSStop(http_t *http)		// I - Connection to server
{
  int	error;				// Error code


  error = gnutls_bye(http->tls, http->mode == _HTTP_MODE_CLIENT ? GNUTLS_SHUT_RDWR : GNUTLS_SHUT_WR);
  if (error != GNUTLS_E_SUCCESS)
    _cupsSetError(IPP_STATUS_ERROR_INTERNAL, gnutls_strerror(errno), 0);

  gnutls_deinit(http->tls);
  http->tls = NULL;

  if (http->tls_credentials)
  {
    gnutls_certificate_free_credentials(*(http->tls_credentials));
    free(http->tls_credentials);
    http->tls_credentials = NULL;
  }
}


//
// '_httpTLSWrite()' - Write to a SSL/TLS connection.
//

int					// O - Bytes written
_httpTLSWrite(http_t     *http,		// I - Connection to server
	      const char *buf,		// I - Buffer holding data
	      int        len)		// I - Length of buffer
{
  ssize_t	result;			// Return value


  DEBUG_printf(("5_httpTLSWrite(http=%p, buf=%p, len=%d)", http, buf, len));

  result = gnutls_record_send(http->tls, buf, (size_t)len);

  if (result < 0 && !errno)
  {
    // Convert GNU TLS error to errno value...
    switch (result)
    {
      case GNUTLS_E_INTERRUPTED :
	  errno = EINTR;
	  break;

      case GNUTLS_E_AGAIN :
          errno = EAGAIN;
          break;

      default :
          errno = EPIPE;
          break;
    }

    result = -1;
  }

  DEBUG_printf(("5_httpTLSWrite: Returning %d.", (int)result));

  return ((int)result);
}
