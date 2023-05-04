//
// X.509 credentials test program for CUPS.
//
// Copyright © 2022-2023 by OpenPrinting.
// Copyright © 2007-2016 by Apple Inc.
// Copyright © 1997-2006 by Easy Software Products.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//
// Usage: testcreds [OPTIONS] [SUB-COMMAND] [ARGUMENT]
//
// Sub-Commands:
//
//   ca COMMON-NAME             Sign a CSR to produce a certificate.
//   cacert COMMON-NAME         Create a CA certificate.
//   cert COMMON-NAME           Create a certificate.
//   client URI                 Connect to URI.
//   csr COMMON-NAME            Create a certificate signing request.
//   server COMMON-NAME[:PORT]  Run a HTTPS server (default port 8NNN.)
//   show COMMON-NAME           Show stored credentials for COMMON-NAME.
//
// Options:
//
//   -C COUNTRY                 Set country.
//   -L LOCALITY                Set locality name.
//   -O ORGANIZATION            Set organization name.
//   -S STATE                   Set state.
//   -U ORGANIZATIONAL-UNIT     Set organizational unit name.
//   -a SUBJECT-ALT-NAME        Add a subjectAltName.
//   -d DAYS                    Set expiration date in days.
//   -p PURPOSE                 Comma-delimited certificate purpose (serverAuth,
//                              clientAuth, codeSigning, emailProtection,
//                              timeStamping, OCSPSigning)
//   -r ROOT-NAME               Name of root certificate
//   -t TYPE                    Certificate type (rsa-2048, rsa-3072, rsa-4096,
//                              ecdsa-p256, ecdsa-p384, ecdsa-p521)
//   -u USAGE                   Comma-delimited key usage (digitalSignature,
//                              nonRepudiation, keyEncipherment,
//                              dataEncipherment, keyAgreement, keyCertSign,
//                              cRLSign, encipherOnly, decipherOnly, default-ca,
//                              default-tls)
//

#include "cups-private.h"
#include "test-internal.h"


//
// Local functions...
//

static int	test_ca(const char *arg);
static int	test_cacert(const char *arg);
static int	test_cert(const char *arg);
static int	test_client(const char *arg);
static int	test_csr(const char *arg);
static int	test_server(const char *arg);
static int	test_show(const char *arg);
static int	usage(FILE *fp);


//
// 'main()' - Main entry.
//

int					// O - Exit status
main(int  argc,				// I - Number of command-line arguments
     char *argv[])			// I - Command-line arguments
{
  int		i;			// Looping var
  const char	*subcommand = NULL,	// Sub-command
		*arg = NULL,		// Argument for sub-command
		*opt,			// Current option character
		*root_name = NULL,	// Name of root certificate
		*organization = NULL,	// Organization
		*org_unit = NULL,	// Organizational unit
		*locality = NULL,	// Locality
		*state = NULL,		// State/province
		*country = NULL,	// Country
		*alt_names[100];	// Subject alternate names
  size_t	num_alt_names = 0;
  int		days;			// Days until expiration
  cups_credpurpose_t purpose = CUPS_CREDPURPOSE_SERVER_AUTH;
					// Certificate purpose
  cups_credtype_t type = CUPS_CREDTYPE_DEFAULT;
					// Certificate type
  cups_credusage_t keyusage = CUPS_CREDUSAGE_DEFAULT_TLS;
					// Key usage


  // Check command-line...
  for (i = 1; i < argc; i ++)
  {
    if (!strcmp(argv[i], "--help"))
    {
      return (usage(stdout));
    }
    else if (!strncmp(argv[i], "--", 2))
    {
      fprintf(stderr, "testcreds: Unknown option '%s'.\n", argv[i]);
      return (usage(stderr));
    }
    else if (argv[i][0] == '-')
    {
      for (opt = argv[i] + 1; *opt; opt ++)
      {
        switch (*opt)
        {
          case 'C' : // -C COUNTRY
              i ++;
              if (i >= argc)
              {
                fputs("testcreds: Missing country after '-C'.\n", stderr);
                return (usage(stderr));
	      }
	      country = argv[i];
	      break;

          case 'L' : // -L LOCALITY
              i ++;
              if (i >= argc)
              {
                fputs("testcreds: Missing locality/city/town after '-L'.\n", stderr);
                return (usage(stderr));
	      }
	      locality = argv[i];
	      break;

          case 'O' : // -O ORGANIZATION
              i ++;
              if (i >= argc)
              {
                fputs("testcreds: Missing organization after '-O'.\n", stderr);
                return (usage(stderr));
	      }
	      organization = argv[i];
	      break;

          case 'S' : // -S STATE
              i ++;
              if (i >= argc)
              {
                fputs("testcreds: Missing state/province after '-S'.\n", stderr);
                return (usage(stderr));
	      }
	      state = argv[i];
	      break;

          case 'U' : // -U ORGANIZATIONAL-UNIT
              i ++;
              if (i >= argc)
              {
                fputs("testcreds: Missing organizational unit after '-U'.\n", stderr);
                return (usage(stderr));
	      }
	      org_unit = argv[i];
	      break;

          case 'a' : // -a SUBJECT-ALT-NAME
              i ++;
              if (i >= argc)
              {
                fputs("testcreds: Missing subjectAltName after '-a'.\n", stderr);
                return (usage(stderr));
	      }
	      if (num_alt_names >= (sizeof(alt_names) / sizeof(alt_names[0])))
	      {
	        fputs("testcreds: Too many subjectAltName values.\n", stderr);
	        return (1);
	      }
	      alt_names[num_alt_names ++] = argv[i];
	      break;

          case 'd' : // -d DAYS
              i ++;
              if (i >= argc)
              {
                fputs("testcreds: Missing expiration days after '-d'.\n", stderr);
                return (usage(stderr));
	      }
	      if ((days = atoi(argv[i])) <= 0)
	      {
	        fprintf(stderr, "testcreds: Bad DAYS value '%s' after '-d'.\n", argv[i]);
	        return (1);
	      }
	      break;

          case 'p' : // -p PURPOSE
              i ++;
              if (i >= argc)
              {
                fputs("testcreds: Missing purpose after '-p'.\n", stderr);
                return (usage(stderr));
	      }
	      purpose = 0;
	      if (strstr(argv[i], "serverAuth"))
	        purpose |= CUPS_CREDPURPOSE_SERVER_AUTH;
	      if (strstr(argv[i], "clientAuth"))
	        purpose |= CUPS_CREDPURPOSE_CLIENT_AUTH;
	      if (strstr(argv[i], "codeSigning"))
	        purpose |= CUPS_CREDPURPOSE_CODE_SIGNING;
	      if (strstr(argv[i], "emailProtection"))
	        purpose |= CUPS_CREDPURPOSE_EMAIL_PROTECTION;
	      if (strstr(argv[i], "timeStamping"))
	        purpose |= CUPS_CREDPURPOSE_TIME_STAMPING;
	      if (strstr(argv[i], "OCSPSigning"))
	        purpose |= CUPS_CREDPURPOSE_OCSP_SIGNING;
              if (purpose == 0)
              {
                fprintf(stderr, "testcreds: Bad purpose '%s'.\n", argv[i]);
                return (usage(stderr));
	      }
	      break;

          case 'r' : // -r ROOT-NAME
              i ++;
              if (i >= argc)
              {
                fputs("testcreds: Missing root name after '-r'.\n", stderr);
                return (usage(stderr));
	      }
	      root_name = argv[i];
	      break;

          case 't' : // -t TYPE
              i ++;
              if (i >= argc)
              {
                fputs("testcreds: Missing certificate type after '-t'.\n", stderr);
                return (usage(stderr));
	      }
	      if (!strcmp(argv[i], "default"))
	      {
	        type = CUPS_CREDTYPE_DEFAULT;
	      }
	      else if (!strcmp(argv[i], "rsa-2048"))
	      {
	        type = CUPS_CREDTYPE_RSA_2048_SHA256;
	      }
	      else if (!strcmp(argv[i], "rsa-3072"))
	      {
	        type = CUPS_CREDTYPE_RSA_3072_SHA256;
	      }
	      else if (!strcmp(argv[i], "rsa-4096"))
	      {
	        type = CUPS_CREDTYPE_RSA_4096_SHA256;
	      }
	      else if (!strcmp(argv[i], "ecdsa-p256"))
	      {
	        type = CUPS_CREDTYPE_ECDSA_P256_SHA256;
	      }
	      else if (!strcmp(argv[i], "ecdsa-p384"))
	      {
	        type = CUPS_CREDTYPE_ECDSA_P384_SHA256;
	      }
	      else if (!strcmp(argv[i], "ecdsa-p521"))
	      {
	        type = CUPS_CREDTYPE_ECDSA_P521_SHA256;
	      }
	      else
	      {
	        fprintf(stderr, "testcreds: Bad certificate type '%s'.\n", argv[i]);
	        return (usage(stderr));
	      }
	      break;

          case 'u' : // -u USAGE
              i ++;
              if (i >= argc)
              {
                fputs("testcreds: Missing key usage after '-u'.\n", stderr);
                return (usage(stderr));
	      }
	      keyusage = 0;
	      if (strstr(argv[i], "default-ca"))
	        keyusage = CUPS_CREDUSAGE_DEFAULT_CA;
	      if (strstr(argv[i], "default-tls"))
	        keyusage = CUPS_CREDUSAGE_DEFAULT_TLS;
	      if (strstr(argv[i], "digitalSignature"))
	        keyusage |= CUPS_CREDUSAGE_DIGITAL_SIGNATURE;
	      if (strstr(argv[i], "nonRepudiation"))
	        keyusage |= CUPS_CREDUSAGE_NON_REPUDIATION;
	      if (strstr(argv[i], "keyEncipherment"))
	        keyusage |= CUPS_CREDUSAGE_KEY_ENCIPHERMENT;
	      if (strstr(argv[i], "dataEncipherment"))
	        keyusage |= CUPS_CREDUSAGE_DATA_ENCIPHERMENT;
	      if (strstr(argv[i], "keyAgreement"))
	        keyusage |= CUPS_CREDUSAGE_KEY_AGREEMENT;
	      if (strstr(argv[i], "keyCertSign"))
	        keyusage |= CUPS_CREDUSAGE_KEY_CERT_SIGN;
	      if (strstr(argv[i], "cRLSign"))
	        keyusage |= CUPS_CREDUSAGE_CRL_SIGN;
	      if (strstr(argv[i], "encipherOnly"))
	        keyusage |= CUPS_CREDUSAGE_ENCIPHER_ONLY;
	      if (strstr(argv[i], "decipherOnly"))
	        keyusage |= CUPS_CREDUSAGE_DECIPHER_ONLY;
	      if (keyusage == 0)
	      {
	        fprintf(stderr, "testcreds: Bad key usage '%s'.\n", argv[i]);
	        return (usage(stderr));
	      }
	      break;

          default :
              fprintf(stderr, "testcreds: Unknown option '-%c'.\n", *opt);
              return (usage(stderr));
	}
      }
    }
    else if (!subcommand)
    {
      subcommand = argv[i];
    }
    else if (!arg)
    {
      arg = argv[i];
    }
    else
    {
      fprintf(stderr, "testcreds: Unknown option '%s'.\n", argv[i]);
      return (usage(stderr));
    }
  }

  if (!subcommand)
  {
    fputs("testcreds: Missing sub-command.\n", stderr);
    return (usage(stderr));
  }
  else if (!arg)
  {
    fputs("testcreds: Missing sub-command argument.\n", stderr);
    return (usage(stderr));
  }

  // Run the corresponding sub-command...
  if (!strcmp(subcommand, "ca"))
  {
    return (test_ca(arg));
  }
  else if (!strcmp(subcommand, "cacert"))
  {
    return (test_cacert(arg));
  }
  else if (!strcmp(subcommand, "cert"))
  {
    return (test_cert(arg));
  }
  else if (!strcmp(subcommand, "client"))
  {
    return (test_client(arg));
  }
  else if (!strcmp(subcommand, "csr"))
  {
    return (test_csr(arg));
  }
  else if (!strcmp(subcommand, "server"))
  {
    return (test_server(arg));
  }
  else if (!strcmp(subcommand, "show"))
  {
    return (test_show(arg));
  }
  else
  {
    fprintf(stderr, "testcreds: Unknown sub-command '%s'.\n", subcommand);
    return (usage(stderr));
  }
}


//
// 'test_ca()' - Test generating a certificate from a CSR.
//

static int				// O - Exit status
test_ca(const char *arg)		// I - Common name
{
  (void)arg;
  return (1);
}


//
// 'test_cacert()' - Test creating a CA certificate.
//

static int				// O - Exit status
test_cacert(const char *arg)		// I - Common name
{
  (void)arg;
  return (1);
}


//
// 'test_cert()' - Test creating a self-signed certificate.
//

static int				// O - Exit status
test_cert(const char *arg)		// I - Common name
{
  (void)arg;
  return (1);
}


//
// 'test_client()' - Test connecting to a HTTPS server.
//

static int				// O - Exit status
test_client(const char *arg)		// I - URI
{
  http_t	*http;			// HTTP connection
  char		scheme[HTTP_MAX_URI],	// Scheme from URI
		hostname[HTTP_MAX_URI],	// Hostname from URI
		username[HTTP_MAX_URI],	// Username:password from URI
		resource[HTTP_MAX_URI];	// Resource from URI
  int		port;			// Port number from URI
  http_trust_t	trust;			// Trust evaluation for connection
  cups_array_t	*hcreds,		// Credentials from connection
		*tcreds;		// Credentials from trust store
  char		hinfo[1024],		// String for connection credentials
		tinfo[1024],		// String for trust store credentials
		datestr[256];		// Date string
  static const char *trusts[] =		// Trust strings
  { "OK", "Invalid", "Changed", "Expired", "Renewed", "Unknown" };


  // Connect to the host and validate credentials...
  if (httpSeparateURI(HTTP_URI_CODING_MOST, arg, scheme, sizeof(scheme), username, sizeof(username), hostname, sizeof(hostname), &port, resource, sizeof(resource)) < HTTP_URI_STATUS_OK)
  {
    fprintf(stderr, "testcreds: Bad URI '%s'.\n", arg);
    return (1);
  }

  if ((http = httpConnect(hostname, port, NULL, AF_UNSPEC, HTTP_ENCRYPTION_ALWAYS, 1, 30000, NULL)) == NULL)
  {
    fprintf(stderr, "testcreds: Unable to connect to '%s' on port %d: %s\n", hostname, port, cupsLastErrorString());
    return (1);
  }

  puts("TLS Server Credentials:");
  if (httpCopyCredentials(http, &hcreds))
  {
    trust = httpCredentialsGetTrust(hcreds, hostname);

    httpCredentialsString(hcreds, hinfo, sizeof(hinfo));

    printf("    Certificate Count: %u\n", (unsigned)cupsArrayGetCount(hcreds));
    if (trust == HTTP_TRUST_OK)
      puts("    Trust: OK");
    else
      printf("    Trust: %s (%s)\n", trusts[trust], cupsLastErrorString());
    printf("    Expiration: %s\n", httpGetDateString(httpCredentialsGetExpiration(hcreds), datestr, sizeof(datestr)));
    printf("    IsValidName: %s\n", httpCredentialsAreValidForName(hcreds, hostname) ? "true" : "false");
    printf("    String: \"%s\"\n", hinfo);

    httpFreeCredentials(hcreds);
  }
  else
  {
    puts("    Not present (error).");
  }

  puts("");

  printf("Trust Store for \"%s\":\n", hostname);

  if (httpLoadCredentials(NULL, &tcreds, hostname))
  {
    httpCredentialsString(tcreds, tinfo, sizeof(tinfo));

    printf("    Certificate Count: %u\n", (unsigned)cupsArrayGetCount(tcreds));
    printf("    Expiration: %s\n", httpGetDateString(httpCredentialsGetExpiration(tcreds), datestr, sizeof(datestr)));
    printf("    IsValidName: %s\n", httpCredentialsAreValidForName(tcreds, hostname) ? "true" : "false");
    printf("    String: \"%s\"\n", tinfo);

    httpFreeCredentials(tcreds);
  }
  else
  {
    puts("    Not present.");
  }

  return (0);
}


//
// 'test_csr()' - Test creating a certificate signing request.
//

static int				// O - Exit status
test_csr(const char *arg)		// I - Common name
{
  (void)arg;
  return (1);
}


//
// 'test_server()' - Test running a server.
//

static int				// O - Exit status
test_server(const char *arg)		// I - Hostname/port
{
  (void)arg;
  return (1);
}


//
// 'test_show()' - Test showing stored certificates.
//

static int				// O - Exit status
test_show(const char *arg)		// I - Common name
{
  (void)arg;
  return (1);
}


//
// 'usage()' - Show program usage...
//

static int				// O - Exit code
usage(FILE *fp)				// I - Output file (stdout or stderr)
{
  fputs("Usage: testcreds [OPTIONS] [SUB-COMMAND] [ARGUMENT]\n", fp);
  fputs("\n", fp);
  fputs("Sub-Commands:\n", fp);
  fputs("\n", fp);
  fputs("  ca COMMON-NAME             Sign a CSR to produce a certificate.\n", fp);
  fputs("  cacert COMMON-NAME         Create a CA certificate.\n", fp);
  fputs("  cert COMMON-NAME           Create a certificate.\n", fp);
  fputs("  client URI                 Connect to URI.\n", fp);
  fputs("  csr COMMON-NAME            Create a certificate signing request.\n", fp);
  fputs("  server COMMON-NAME[:PORT]  Run a HTTPS server (default port 8NNN.)\n", fp);
  fputs("  show COMMON-NAME           Show stored credentials for COMMON-NAME.\n", fp);
  fputs("\n", fp);
  fputs("Options:\n", fp);
  fputs("\n", fp);
  fputs("  -C COUNTRY                 Set country.\n", fp);
  fputs("  -L LOCALITY                Set locality name.\n", fp);
  fputs("  -O ORGANIZATION            Set organization name.\n", fp);
  fputs("  -S STATE                   Set state.\n", fp);
  fputs("  -U ORGANIZATIONAL-UNIT     Set organizational unit name.\n", fp);
  fputs("  -a SUBJECT-ALT-NAME        Add a subjectAltName.\n", fp);
  fputs("  -d DAYS                    Set expiration date in days.\n", fp);
  fputs("  -p PURPOSE                 Comma-delimited certificate purpose (serverAuth, clientAuth, codeSigning, emailProtection, timeStamping, OCSPSigning)\n", fp);
  fputs("  -r ROOT-NAME               Name of root certificate\n", fp);
  fputs("  -t TYPE                    Certificate type (rsa-2048, rsa-3072, rsa-4096, ecdsa-p256, ecdsa-p384, ecdsa-p521)\n", fp);
  fputs("  -u USAGE                   Comma-delimited key usage (digitalSignature, nonRepudiation, keyEncipherment, dataEncipherment, keyAgreement, keyCertSign, cRLSign, encipherOnly, decipherOnly, default-ca, default-tls)\n", fp);

  return (fp == stderr);
}
