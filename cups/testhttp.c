//
// HTTP test program for CUPS.
//
// Copyright © 2021-2024 by OpenPrinting.
// Copyright © 2007-2018 by Apple Inc.
// Copyright © 1997-2006 by Easy Software Products.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#include "cups-private.h"
#include "test-internal.h"


//
// Types and structures...
//

typedef struct uri_test_s		// URI test cases
{
  http_uri_status_t	result;		// Expected return value
  const char		*uri,		// URI
			*scheme,	// Scheme string
			*username,	// Username:password string
			*hostname,	// Hostname string
			*resource;	// Resource string
  int			port,		// Port number
			assemble_port;	// Port number for httpAssembleURI()
  http_uri_coding_t	assemble_coding;// Coding for httpAssembleURI()
} uri_test_t;


//
// Local globals...
//

static uri_test_t	uri_tests[] =	// URI test data
			{
			  // Start with valid URIs
			  { HTTP_URI_STATUS_OK, "file:/filename",
			    "file", "", "", "/filename", 0, 0,
			    HTTP_URI_CODING_MOST },
			  { HTTP_URI_STATUS_OK, "file:/filename%20with%20spaces",
			    "file", "", "", "/filename with spaces", 0, 0,
			    HTTP_URI_CODING_MOST  },
			  { HTTP_URI_STATUS_OK, "file:///filename",
			    "file", "", "", "/filename", 0, 0,
			    HTTP_URI_CODING_MOST  },
			  { HTTP_URI_STATUS_OK, "file:///filename%20with%20spaces",
			    "file", "", "", "/filename with spaces", 0, 0,
			    HTTP_URI_CODING_MOST  },
			  { HTTP_URI_STATUS_OK, "file://localhost/filename",
			    "file", "", "localhost", "/filename", 0, 0,
			    HTTP_URI_CODING_MOST  },
			  { HTTP_URI_STATUS_OK, "file://localhost/filename%20with%20spaces",
			    "file", "", "localhost", "/filename with spaces", 0, 0,
			    HTTP_URI_CODING_MOST  },
			  { HTTP_URI_STATUS_OK, "http://server/",
			    "http", "", "server", "/", 80, 0,
			    HTTP_URI_CODING_MOST  },
			  { HTTP_URI_STATUS_OK, "http://username@server/",
			    "http", "username", "server", "/", 80, 0,
			    HTTP_URI_CODING_MOST  },
			  { HTTP_URI_STATUS_OK, "http://username:passwor%64@server/",
			    "http", "username:password", "server", "/", 80, 0,
			    HTTP_URI_CODING_MOST  },
			  { HTTP_URI_STATUS_OK, "http://username:passwor%64@server:8080/",
			    "http", "username:password", "server", "/", 8080, 8080,
			    HTTP_URI_CODING_MOST  },
			  { HTTP_URI_STATUS_OK, "http://username:passwor%64@server:8080/directory/filename",
			    "http", "username:password", "server", "/directory/filename", 8080, 8080,
			    HTTP_URI_CODING_MOST  },
			  { HTTP_URI_STATUS_OK, "http://[2000::10:100]:631/ipp",
			    "http", "", "2000::10:100", "/ipp", 631, 631,
			    HTTP_URI_CODING_MOST  },
			  { HTTP_URI_STATUS_OK, "https://username:passwor%64@server/directory/filename",
			    "https", "username:password", "server", "/directory/filename", 443, 0,
			    HTTP_URI_CODING_MOST  },
			  { HTTP_URI_STATUS_OK, "ipp://username:passwor%64@[::1]/ipp",
			    "ipp", "username:password", "::1", "/ipp", 631, 0,
			    HTTP_URI_CODING_MOST  },
			  { HTTP_URI_STATUS_OK, "lpd://server/queue?reserve=yes",
			    "lpd", "", "server", "/queue?reserve=yes", 515, 0,
			    HTTP_URI_CODING_MOST  },
			  { HTTP_URI_STATUS_OK, "mailto:user@domain.com",
			    "mailto", "", "", "user@domain.com", 0, 0,
			    HTTP_URI_CODING_MOST  },
			  { HTTP_URI_STATUS_OK, "socket://server/",
			    "socket", "", "server", "/", 9100, 0,
			    HTTP_URI_CODING_MOST  },
			  { HTTP_URI_STATUS_OK, "socket://192.168.1.1:9101/",
			    "socket", "", "192.168.1.1", "/", 9101, 9101,
			    HTTP_URI_CODING_MOST  },
			  { HTTP_URI_STATUS_OK, "tel:8005551212",
			    "tel", "", "", "8005551212", 0, 0,
			    HTTP_URI_CODING_MOST  },
			  { HTTP_URI_STATUS_OK, "ipp://username:password@[v1.fe80::200:1234:5678:9abc+eth0]:999/ipp",
			    "ipp", "username:password", "fe80::200:1234:5678:9abc%eth0", "/ipp", 999, 999,
			    HTTP_URI_CODING_MOST  },
			  { HTTP_URI_STATUS_OK, "ipp://username:password@[fe80::200:1234:5678:9abc%25eth0]:999/ipp",
			    "ipp", "username:password", "fe80::200:1234:5678:9abc%eth0", "/ipp", 999, 999,
			    (http_uri_coding_t)(HTTP_URI_CODING_MOST | HTTP_URI_CODING_RFC6874) },
			  { HTTP_URI_STATUS_OK, "http://server/admin?DEVICE_URI=usb://HP/Photosmart%25202600%2520series?serial=MY53OK70V10400",
			    "http", "", "server", "/admin?DEVICE_URI=usb://HP/Photosmart%25202600%2520series?serial=MY53OK70V10400", 80, 0,
			    HTTP_URI_CODING_MOST  },
			  { HTTP_URI_STATUS_OK, "lpd://Acme%20Laser%20(01%3A23%3A45).local._tcp._printer/",
			    "lpd", "", "Acme Laser (01:23:45).local._tcp._printer", "/", 515, 0,
			    HTTP_URI_CODING_MOST  },
			  { HTTP_URI_STATUS_OK, "ipp://HP%20Officejet%204500%20G510n-z%20%40%20Will's%20MacBook%20Pro%2015%22._ipp._tcp.local./",
			    "ipp", "", "HP Officejet 4500 G510n-z @ Will's MacBook Pro 15\"._ipp._tcp.local.", "/", 631, 0,
			    HTTP_URI_CODING_MOST  },
			  { HTTP_URI_STATUS_OK, "ipp://%22%23%2F%3A%3C%3E%3F%40%5B%5C%5D%5E%60%7B%7C%7D/",
			    "ipp", "", "\"#/:<>?@[\\]^`{|}", "/", 631, 0,
			    HTTP_URI_CODING_MOST  },
			  { HTTP_URI_STATUS_UNKNOWN_SCHEME, "smb://server/Some%20Printer",
			    "smb", "", "server", "/Some Printer", 0, 0,
			    HTTP_URI_CODING_ALL },

			  // Missing scheme
			  { HTTP_URI_STATUS_MISSING_SCHEME, "/path/to/file/index.html",
			    "file", "", "", "/path/to/file/index.html", 0, 0,
			    HTTP_URI_CODING_MOST  },
			  { HTTP_URI_STATUS_MISSING_SCHEME, "//server/ipp",
			    "ipp", "", "server", "/ipp", 631, 0,
			    HTTP_URI_CODING_MOST  },

			  // Unknown scheme
			  { HTTP_URI_STATUS_UNKNOWN_SCHEME, "vendor://server/resource",
			    "vendor", "", "server", "/resource", 0, 0,
			    HTTP_URI_CODING_MOST  },

			  // Missing resource
			  { HTTP_URI_STATUS_MISSING_RESOURCE, "socket://[::192.168.2.1]",
			    "socket", "", "::192.168.2.1", "/", 9100, 0,
			    HTTP_URI_CODING_MOST  },
			  { HTTP_URI_STATUS_MISSING_RESOURCE, "socket://192.168.1.1:9101",
			    "socket", "", "192.168.1.1", "/", 9101, 0,
			    HTTP_URI_CODING_MOST  },

			  // Bad URI
			  { HTTP_URI_STATUS_BAD_URI, "",
			    "", "", "", "", 0, 0,
			    HTTP_URI_CODING_MOST  },

			  // Bad scheme
			  { HTTP_URI_STATUS_BAD_SCHEME, "://server/ipp",
			    "", "", "", "", 0, 0,
			    HTTP_URI_CODING_MOST  },
			  { HTTP_URI_STATUS_BAD_SCHEME, "bad_scheme://server/resource",
			    "", "", "", "", 0, 0,
			    HTTP_URI_CODING_MOST  },

			  // Bad username
			  { HTTP_URI_STATUS_BAD_USERNAME, "http://username:passwor%6@server/resource",
			    "http", "", "", "", 80, 0,
			    HTTP_URI_CODING_MOST  },

			  // Bad hostname
			  { HTTP_URI_STATUS_BAD_HOSTNAME, "http://[/::1]/index.html",
			    "http", "", "", "", 80, 0,
			    HTTP_URI_CODING_MOST  },
			  { HTTP_URI_STATUS_BAD_HOSTNAME, "http://[",
			    "http", "", "", "", 80, 0,
			    HTTP_URI_CODING_MOST  },
			  { HTTP_URI_STATUS_BAD_HOSTNAME, "http://serve%7/index.html",
			    "http", "", "", "", 80, 0,
			    HTTP_URI_CODING_MOST  },
			  { HTTP_URI_STATUS_BAD_HOSTNAME, "http://server with spaces/index.html",
			    "http", "", "", "", 80, 0,
			    HTTP_URI_CODING_MOST  },
			  { HTTP_URI_STATUS_BAD_HOSTNAME, "ipp://\"#/:<>?@[\\]^`{|}/",
			    "ipp", "", "", "", 631, 0,
			    HTTP_URI_CODING_MOST  },

			  // Bad port number
			  { HTTP_URI_STATUS_BAD_PORT, "http://127.0.0.1:9999a/index.html",
			    "http", "", "127.0.0.1", "", 0, 0,
			    HTTP_URI_CODING_MOST  },

			  // Bad resource
			  { HTTP_URI_STATUS_BAD_RESOURCE, "mailto:\r\nbla",
			    "mailto", "", "", "", 0, 0,
			    HTTP_URI_CODING_MOST  },
			  { HTTP_URI_STATUS_BAD_RESOURCE, "http://server/index.html%",
			    "http", "", "server", "", 80, 0,
			    HTTP_URI_CODING_MOST  },
			  { HTTP_URI_STATUS_BAD_RESOURCE, "http://server/index with spaces.html",
			    "http", "", "server", "", 80, 0,
			    HTTP_URI_CODING_MOST  }
			};
static const char * const base64_tests[][2] =
			{
			  { "A", "QQ==" },
			  // 010000 01
			  { "AB", "QUI=" },
			  // 010000 010100 0010
			  { "ABC", "QUJD" },
			  // 010000 010100 001001 000011
			  { "ABCD", "QUJDRA==" },
			  // 010000 010100 001001 000011 010001 00
			  { "ABCDE", "QUJDREU=" },
			  // 010000 010100 001001 000011 010001 000100 0101
			  { "ABCDEF", "QUJDREVG" },
			  // 010000 010100 001001 000011 010001 000100 010101 000110
			};


//
// Local functions...
//

static bool	test_date(time_t t);


//
// 'main()' - Main entry.
//

int					// O - Exit status
main(int  argc,				// I - Number of command-line arguments
     char *argv[])			// I - Command-line arguments
{
  int		i, j, k;		// Looping vars
  http_t	*http;			// HTTP connection
  http_encryption_t encryption;		// Encryption type
  http_status_t	status;			// Status of GET command
  int		failures;		// Number of test failures
  char		buffer[8192];		// Input buffer
  long		bytes;			// Number of bytes read
  FILE		*out;			// Output file
  char		encode[256],		// Base64-encoded string
		decode[256];		// Base64-decoded string
  size_t	decodelen;		// Length of decoded string
  const char	*decodeptr;		// Pointer into Base64 string
  char		scheme[HTTP_MAX_URI],	// Scheme from URI
		hostname[HTTP_MAX_URI],	// Hostname from URI
		username[HTTP_MAX_URI],	// Username:password from URI
		resource[HTTP_MAX_URI];	// Resource from URI
  int		port;			// Port number from URI
  http_uri_status_t uri_status;		// Status of URI separation
  http_addrlist_t *addrlist,		// Address list
		*addr;			// Current address
  off_t		length, total;		// Length and total bytes
  time_t	start, current;		// Start and end time
  const char	*encoding;		// Negotiated Content-Encoding
  static const char * const uri_status_strings[] =
  {					// URI encode/decode status strings
    "HTTP_URI_STATUS_OVERFLOW",
    "HTTP_URI_STATUS_BAD_ARGUMENTS",
    "HTTP_URI_STATUS_BAD_RESOURCE",
    "HTTP_URI_STATUS_BAD_PORT",
    "HTTP_URI_STATUS_BAD_HOSTNAME",
    "HTTP_URI_STATUS_BAD_USERNAME",
    "HTTP_URI_STATUS_BAD_SCHEME",
    "HTTP_URI_STATUS_BAD_URI",
    "HTTP_URI_STATUS_OK",
    "HTTP_URI_STATUS_MISSING_SCHEME",
    "HTTP_URI_STATUS_UNKNOWN_SCHEME",
    "HTTP_URI_STATUS_MISSING_RESOURCE"
  };


  // Do API tests if we don't have a URL on the command-line...
  if (argc == 1)
  {
    failures = 0;

    // httpGetDateString()/httpGetDateTime()
    if (!test_date(0))
      failures ++;
    if (!test_date(time(NULL)))
      failures ++;
    if (!test_date(INT_MAX))
      failures ++;
    if (!test_date(UINT_MAX))
      failures ++;

    // httpDecode64()/httpEncode64()
    testBegin("httpDecode64()/httpEncode64()");

    for (i = 0, j = 0; i < (int)(sizeof(base64_tests) / sizeof(base64_tests[0])); i ++)
    {
      httpEncode64(encode, sizeof(encode), base64_tests[i][0], strlen(base64_tests[i][0]), false);
      decodelen = sizeof(decode);
      httpDecode64(decode, &decodelen, base64_tests[i][1], &decodeptr);

      if (strcmp(decode, base64_tests[i][0]))
      {
        failures ++;

        if (j)
	{
	  testEnd(false);
	  j = 1;
	}

        testError("httpDecode64() returned \"%s\", expected \"%s\".", decode, base64_tests[i][0]);
      }
      else if (*decodeptr)
      {
        failures ++;

        if (j)
        {
          testEnd(false);
          j = 1;
	}

        testError("httpDecode64() returned \"%s\", expected end of string.", decodeptr);
      }

      if (strcmp(encode, base64_tests[i][1]))
      {
        failures ++;

        if (j)
	{
	  testEnd(false);
	  j = 1;
	}

        testError("httpEncode64() returned \"%s\", expected \"%s\".", encode, base64_tests[i][1]);
      }
    }

    if (!j)
      testEnd(true);

    // httpGetHostname()
    testBegin("httpGetHostname()");

    if (httpGetHostname(NULL, hostname, sizeof(hostname)))
      testEndMessage(true, "%s", hostname);
    else
    {
      failures ++;
      testEnd(false);
    }

    // httpAddrGetList()
    testBegin("httpAddrGetList(%s)", hostname);

    addrlist = httpAddrGetList(hostname, AF_UNSPEC, NULL);
    if (addrlist)
    {
      for (i = 0, addr = addrlist; addr; i ++, addr = addr->next)
      {
        char	numeric[1024];		// Numeric IP address


	httpAddrGetString(&(addr->addr), numeric, sizeof(numeric));
	if (!strcmp(numeric, "UNKNOWN"))
	  break;
      }

      if (addr)
        testEndMessage(false, "bad address for %s", hostname);
      else
        testEndMessage(true, "%d address(es) for %s", i, hostname);

      httpAddrFreeList(addrlist);
    }
    else if (isdigit(hostname[0] & 255))
    {
      testEndMessage(true, "ignored because hostname is numeric");
    }
    else if (!strncmp(hostname, "mac-", 4) && isdigit(hostname[4]))
    {
      testEndMessage(true, "ignored because GitHub Actions macOS runner doesn't resolve its own hostname");
    }
    else
    {
      failures ++;
      testEnd(false);
    }

    // Test httpSeparateURI()...
    testBegin("httpSeparateURI()");
    for (i = 0, j = 0; i < (int)(sizeof(uri_tests) / sizeof(uri_tests[0])); i ++)
    {
      uri_status = httpSeparateURI(HTTP_URI_CODING_MOST, uri_tests[i].uri, scheme, sizeof(scheme), username, sizeof(username), hostname, sizeof(hostname), &port, resource, sizeof(resource));
      if (uri_status != uri_tests[i].result || strcmp(scheme, uri_tests[i].scheme) || strcmp(username, uri_tests[i].username) || strcmp(hostname, uri_tests[i].hostname) || port != uri_tests[i].port || strcmp(resource, uri_tests[i].resource))
      {
        failures ++;

	if (!j)
	{
	  testEnd(false);
	  j = 1;
	}

        testError("\"%s\":", uri_tests[i].uri);

	if (uri_status != uri_tests[i].result)
	  testError("    Returned %s instead of %s", uri_status_strings[uri_status + 8], uri_status_strings[uri_tests[i].result + 8]);

        if (strcmp(scheme, uri_tests[i].scheme))
	  testError("    Scheme \"%s\" instead of \"%s\"", scheme, uri_tests[i].scheme);

	if (strcmp(username, uri_tests[i].username))
	  testError("    Username \"%s\" instead of \"%s\"", username, uri_tests[i].username);

	if (strcmp(hostname, uri_tests[i].hostname))
	  testError("    Hostname \"%s\" instead of \"%s\"", hostname, uri_tests[i].hostname);

	if (port != uri_tests[i].port)
	  testError("    Port %d instead of %d", port, uri_tests[i].port);

	if (strcmp(resource, uri_tests[i].resource))
	  testError("    Resource \"%s\" instead of \"%s\"", resource, uri_tests[i].resource);
      }
    }

    if (!j)
      testEndMessage(true, "%d URIs tested", (int)(sizeof(uri_tests) / sizeof(uri_tests[0])));

    // Test httpAssembleURI()...
    testBegin("httpAssembleURI()");
    for (i = 0, j = 0, k = 0; i < (int)(sizeof(uri_tests) / sizeof(uri_tests[0])); i ++)
    {
      if (uri_tests[i].result == HTTP_URI_STATUS_OK && !strstr(uri_tests[i].uri, "%64") && strstr(uri_tests[i].uri, "//"))
      {
        k ++;
	uri_status = httpAssembleURI(uri_tests[i].assemble_coding, buffer, sizeof(buffer), uri_tests[i].scheme, uri_tests[i].username, uri_tests[i].hostname, uri_tests[i].assemble_port, uri_tests[i].resource);

        if (uri_status != HTTP_URI_STATUS_OK)
	{
          failures ++;

	  if (!j)
	  {
	    testEnd(false);
	    j = 1;
	  }

          testError("\"%s\": %s", uri_tests[i].uri, uri_status_strings[uri_status + 8]);
        }
	else if (strcmp(buffer, uri_tests[i].uri))
	{
          failures ++;

	  if (!j)
	  {
	    testEnd(false);
	    j = 1;
	  }

          testError("\"%s\": assembled = \"%s\"", uri_tests[i].uri, buffer);
	}
      }
    }

    if (!j)
      testEndMessage(true, "%d URIs tested", k);

    // httpAssembleUUID
    testBegin("httpAssembleUUID");
    httpAssembleUUID("hostname.example.com", 631, "printer", 12345, buffer, sizeof(buffer));
    if (strncmp(buffer, "urn:uuid:", 9))
    {
      testEndMessage(false, "%s", buffer);
      failures ++;
    }
    else
      testEndMessage(true, "%s", buffer);

    return (failures);
  }
  else if (strstr(argv[1], "._tcp"))
  {
    // Test resolving an mDNS name.
    char	resolved[1024];		// Resolved URI

    testBegin("httpResolveURI(%s, HTTP_RESOLVE_DEFAULT)", argv[1]);

    if (!httpResolveURI(argv[1], resolved, sizeof(resolved), HTTP_RESOLVE_DEFAULT, NULL, NULL))
    {
      testEnd(false);
      return (1);
    }
    else
      testEndMessage(true, "%s", resolved);

    testBegin("httpResolveURI(%s, HTTP_RESOLVE_FQDN)", argv[1]);

    if (!httpResolveURI(argv[1], resolved, sizeof(resolved), HTTP_RESOLVE_FQDN, NULL, NULL))
    {
      testEnd(false);
      return (1);
    }
    else if (strstr(resolved, ".local:"))
    {
      testEndMessage(false, "%s", resolved);
      return (1);
    }
    else
    {
      testEndMessage(true, "%s", resolved);
      return (0);
    }
  }
  else if (!strcmp(argv[1], "-d") && argc == 3)
  {
    // Test httpDecode64
    size_t	bufsize = sizeof(buffer);
					// Output size

    if (httpDecode64(buffer, &bufsize, argv[2], NULL))
    {
      fwrite(buffer, 1, bufsize, stdout);
      return (0);
    }

    return (1);
  }
  else if (!strcmp(argv[1], "-e") && argc == 3)
  {
    // Test httpEncode64 for Base64
    if (httpEncode64(buffer, sizeof(buffer), argv[2], strlen(argv[2]), false))
    {
      puts(buffer);
      return (0);
    }

    return (1);
  }
  else if (!strcmp(argv[1], "-E") && argc == 3)
  {
    // Test httpEncode64 for Base64url
    if (httpEncode64(buffer, sizeof(buffer), argv[2], strlen(argv[2]), true))
    {
      puts(buffer);
      return (0);
    }

    return (1);
  }
  else if (!strcmp(argv[1], "-u") && argc == 3)
  {
    // Test URI separation...
    uri_status = httpSeparateURI(HTTP_URI_CODING_ALL, argv[2], scheme, sizeof(scheme), username, sizeof(username), hostname, sizeof(hostname), &port, resource, sizeof(resource));
    printf("uri_status = %s\n", uri_status_strings[uri_status + 8]);
    printf("scheme     = \"%s\"\n", scheme);
    printf("username   = \"%s\"\n", username);
    printf("hostname   = \"%s\"\n", hostname);
    printf("port       = %d\n", port);
    printf("resource   = \"%s\"\n", resource);

    return (0);
  }

  // Test HTTP GET requests...
  http = NULL;
  out  = stdout;

  for (i = 1; i < argc; i ++)
  {
    int new_auth;

    if (!strcmp(argv[i], "-o"))
    {
      i ++;
      if (i >= argc)
        break;

      if ((out = fopen(argv[i], "wb")) == NULL)
      {
        perror(argv[i]);
        return (1);
      }

      continue;
    }

    httpSeparateURI(HTTP_URI_CODING_MOST, argv[i], scheme, sizeof(scheme),
                    username, sizeof(username),
                    hostname, sizeof(hostname), &port,
		    resource, sizeof(resource));

    if (!_cups_strcasecmp(scheme, "https") || !_cups_strcasecmp(scheme, "ipps") ||
        port == 443)
      encryption = HTTP_ENCRYPTION_ALWAYS;
    else
      encryption = HTTP_ENCRYPTION_IF_REQUESTED;

    http = httpConnect(hostname, port, NULL, AF_UNSPEC, encryption, 1, 30000, NULL);
    if (http == NULL)
    {
      perror(hostname);
      continue;
    }

    if (httpIsEncrypted(http))
    {
      char *creds;
      char info[1024], expstr[256];
      static const char *trusts[] = { "OK", "Invalid", "Changed", "Expired", "Renewed", "Unknown" };
      if ((creds = httpCopyPeerCredentials(http)) != NULL)
      {
	char *lcreds;
        http_trust_t trust = cupsGetCredentialsTrust(NULL, hostname, creds, /*require_ca*/true);

        cupsGetCredentialsInfo(creds, info, sizeof(info));

//	printf("Count: %u\n", (unsigned)cupsArrayGetCount(creds));
        printf("Trust: %s (%s)\n", trusts[trust], cupsGetErrorString());
        printf("Expiration: %s\n", httpGetDateString(cupsGetCredentialsExpiration(creds), expstr, sizeof(expstr)));
        printf("IsValidName: %s\n", cupsAreCredentialsValidForName(hostname, creds) ? "true" : "false");
        printf("String: \"%s\"\n", info);

	printf("LoadCredentials: %s\n", (lcreds = cupsCopyCredentials(NULL, hostname)) != NULL ? "true" : "false");
	cupsGetCredentialsInfo(lcreds, info, sizeof(info));
//	printf("    Count: %u\n", (unsigned)cupsArrayGetCount(lcreds));
	printf("    String: \"%s\"\n", info);

#if 0
        if (lcreds && cupsArrayGetCount(creds) == cupsArrayGetCount(lcreds))
        {
          http_credential_t	*cred, *lcred;

          for (i = 1, cred = (http_credential_t *)cupsArrayGetFirst(creds), lcred = (http_credential_t *)cupsArrayGetFirst(lcreds);
               cred && lcred;
               i ++, cred = (http_credential_t *)cupsArrayGetNext(creds), lcred = (http_credential_t *)cupsArrayGetNext(lcreds))
          {
            if (cred->datalen != lcred->datalen)
              printf("    Credential #%d: Different lengths (saved=%d, current=%d)\n", i, (int)cred->datalen, (int)lcred->datalen);
            else if (memcmp(cred->data, lcred->data, cred->datalen))
              printf("    Credential #%d: Different data\n", i);
            else
              printf("    Credential #%d: Matches\n", i);
          }
        }
#endif // 0

        if (trust != HTTP_TRUST_OK)
	{
	  printf("SaveCredentials: %s\n", cupsSaveCredentials(NULL, hostname, creds, /*key*/NULL) ? "true" : "false");
	  trust = cupsGetCredentialsTrust(NULL, hostname, creds, /*require_ca*/false);
	  printf("New Trust: %s\n", trusts[trust]);
 	  printf("SaveCredentials (NULL): %s\n", cupsSaveCredentials(NULL, hostname, /*creds*/NULL, /*key*/NULL) ? "true" : "false");
	}

        free(creds);
        free(lcreds);
      }
      else
      {
        puts("No credentials!");
      }
    }

    printf("Checking file \"%s\"...\n", resource);

    new_auth = 0;

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

      if (http->authstring && !strncmp(http->authstring, "Digest ", 7) && !new_auth)
        _httpSetDigestAuthString(http, http->nextnonce, "HEAD", resource);

      httpClearFields(http);
      httpSetField(http, HTTP_FIELD_AUTHORIZATION, httpGetAuthString(http));
      httpSetField(http, HTTP_FIELD_ACCEPT_LANGUAGE, "en");

      if (!httpWriteRequest(http, "HEAD", resource))
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

      while ((status = httpUpdate(http)) == HTTP_STATUS_CONTINUE);

      new_auth = 0;

      if (status == HTTP_STATUS_UNAUTHORIZED)
      {
        // Flush any error message...
	httpFlush(http);

        // See if we can do authentication...
        new_auth = 1;

	if (!cupsDoAuthentication(http, "HEAD", resource))
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
    while (status == HTTP_STATUS_UNAUTHORIZED ||
           status == HTTP_STATUS_UPGRADE_REQUIRED);

    if (status == HTTP_STATUS_OK)
      puts("HEAD OK:");
    else
      printf("HEAD failed with status %d...\n", status);

    encoding = httpGetContentEncoding(http);

    printf("Requesting file \"%s\" (Accept-Encoding: %s)...\n", resource,
           encoding ? encoding : "identity");

    new_auth = 0;

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

      if (http->authstring && !strncmp(http->authstring, "Digest ", 7) && !new_auth)
        _httpSetDigestAuthString(http, http->nextnonce, "GET", resource);

      httpClearFields(http);
      httpSetField(http, HTTP_FIELD_AUTHORIZATION, httpGetAuthString(http));
      httpSetField(http, HTTP_FIELD_ACCEPT_LANGUAGE, "en");
      httpSetField(http, HTTP_FIELD_ACCEPT_ENCODING, encoding);

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

      while ((status = httpUpdate(http)) == HTTP_STATUS_CONTINUE);

      new_auth = 0;

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

    if (status == HTTP_STATUS_OK)
      puts("GET OK:");
    else
      printf("GET failed with status %d...\n", status);

    start  = time(NULL);
    length = httpGetLength(http);
    total  = 0;

    while ((bytes = httpRead(http, buffer, sizeof(buffer))) > 0)
    {
      total += bytes;
      fwrite(buffer, (size_t)bytes, 1, out);
      if (out != stdout)
      {
        current = time(NULL);
        if (current == start)
          current ++;

        printf("\r" CUPS_LLFMT "/" CUPS_LLFMT " bytes ("
	       CUPS_LLFMT " bytes/sec)      ", CUPS_LLCAST total,
	       CUPS_LLCAST length, CUPS_LLCAST (total / (current - start)));
        fflush(stdout);
      }
    }
  }

  if (out != stdout)
    putchar('\n');

  puts("Closing connection to server...");
  httpClose(http);

  if (out != stdout)
    fclose(out);

  return (0);
}


//
// 'test_date()' - Test the date/time functions for a specific time.
//

static bool				// O - `true` on success, `false` on failure
test_date(time_t t)			// I - Time in seconds since Jan 1, 1970
{
  char		dateval[256];		// Date string
  time_t	timeval;		// Time value


  testBegin("httpGetDateString(%ld)", (long)t);
  if (httpGetDateString(t, dateval, sizeof(dateval)))
  {
    testEndMessage(true, dateval);
  }
  else
  {
    testEnd(false);
    return (false);
  }

  testBegin("httpGetDateTime(\"%s\")", dateval);
  if ((timeval = httpGetDateTime(dateval)) == t)
  {
    testEnd(true);
    return (true);
  }
  else
  {
    testEndMessage(false, "got %ld, expected %ld", timeval, t);
    return (false);
  }
}
