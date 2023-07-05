//
// Hash API unit tests for CUPS.
//
// Copyright © 2023 by OpenPrinting.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#include "cups.h"
#include "test-internal.h"


//
// Local functions...
//

static void	usage(FILE *fp);


//
// 'main()' - Main entry.
//

int					// O - Exit status
main(int  argc,				// I - Number of command-line arguments
     char *argv[])			// I - Command-line arguments
{
  int		i;			// Looping var
  unsigned char	hash[64];		// Hash value
  ssize_t	hashsize;		// Size of hash
  char		hex[256];		// Hex string for hash


  if (argc == 1)
  {
    // Do unit tests...
    static const char *text = "The quick brown fox jumps over the lazy dog";
    static const char *key = "key";
    static const char	* const tests[][3] =
    { // algorithm, hash, hmac
      { "md5", "9e107d9d372bb6826bd81d3542a419d6", "80070713463e7749b90c2dc24911e275" },
      { "sha", "2fd4e1c67a2d28fced849ee1bb76e7391b93eb12", "de7c9b85b8b78aa6bc8a7a36f70a90701c9db4d9" },
      { "sha2-224", "730e109bd7a8a32b1cb9d9a09aa2325d2430587ddbc0c38bad911525", "" },
      { "sha2-256", "d7a8fbb307d7809469ca9abcb0082e4f8d5651e46d3cdb762d02d0bf37c9e592", "f7bc83f430538424b13298e6aa6fb143ef4d59a14946175997479dbc2d1a3cd8" },
      { "sha2-384", "ca737f1014a48f4c0b6dd43cb177b0afd9e5169367544c494011e3317dbf9a509cb1e5dc1e85a941bbee3d7f2afbc9b1", "" },
      { "sha2-512", "07e547d9586f6a73f73fbac0435ed76951218fb7d0c8d788a309d785436bbb642e93a252a954f23912547d1e8a3b5ed6e1bfd7097821233fa0538f3db854fee6", "b42af09057bac1e2d41708e48a902e09b5ff7f12ab428a4fe86653c73dd248fb82f948a549f7b791a5b41915ee4d1ec3935357e4e2317250d0372afa2ebeeb3a" },
    };

    for (i = 0; i < (int)(sizeof(tests) / sizeof(tests[0])); i ++)
    {
      testBegin("cupsHashData('%s')", tests[i][0]);
      if ((hashsize = cupsHashData(tests[i][0], text, strlen(text), hash, sizeof(hash))) < 0)
        testEndMessage(false, "%s", cupsGetErrorString());
      else if (strcasecmp(cupsHashString(hash, (size_t)hashsize, hex, sizeof(hex)), tests[i][1]))
        testEndMessage(false, "expected '%s', got '%s'", tests[i][1], hex);
      else
        testEnd(true);

      if (!tests[i][2][0])
        continue;

      testBegin("cupsHMACData('%s')", tests[i][0]);
      if ((hashsize = cupsHMACData(tests[i][0], (unsigned char *)key, strlen(key), text, strlen(text), hash, sizeof(hash))) < 0)
        testEndMessage(false, "%s", cupsGetErrorString());
      else if (strcasecmp(cupsHashString(hash, (size_t)hashsize, hex, sizeof(hex)), tests[i][2]))
        testEndMessage(false, "expected '%s', got '%s'", tests[i][1], hex);
      else
        testEnd(true);
    }

    if (!testsPassed)
      return (1);
  }
  else
  {
    // Try loading hash strings on the command-line...
    const char	*opt,			// Current option
		*algorithm = NULL,	// Algorithm name
		*key = NULL;		// Private key

    for (i = 1; i < argc; i ++)
    {
      if (!strcmp(argv[i], "--help"))
      {
        usage(stdout);
        return (0);
      }
      else if (!strncmp(argv[i], "--", 2))
      {
        fprintf(stderr, "testhash: Unknown option '%s'.\n", argv[i]);
        usage(stderr);
        return (1);
      }
      else if (argv[i][0] == '-')
      {
        for (opt = argv[i] + 1; *opt; opt ++)
        {
          switch (*opt)
          {
            case 'a' : // -a ALGORITHM
                i ++;
                if (i >= argc)
                {
                  fputs("testhash: Missing algorithm after '-a'.\n", stderr);
                  usage(stderr);
                  return (1);
                }

                algorithm = argv[i];
                break;

            case 'k' : // -a KEY
                i ++;
                if (i >= argc)
                {
                  fputs("testhash: Missing key after '-k'.\n", stderr);
                  usage(stderr);
                  return (1);
                }

                key = argv[i];
                break;

            default :
                fprintf(stderr, "testhash: Unknown option '-%c'.\n", *opt);
                usage(stderr);
                return (1);
          }
        }
      }
      else if (!algorithm)
      {
        fputs("testhash: Missing algorithm.\n", stderr);
        usage(stderr);
        return (1);
      }
      else
      {
        // Hash the string...
        if (key)
          hashsize = cupsHMACData(algorithm, (unsigned char *)key, strlen(key), argv[i], strlen(argv[i]), hash, sizeof(hash));
	else
	  hashsize = cupsHashData(algorithm, argv[i], strlen(argv[i]), hash, sizeof(hash));

        if (hashsize < 0)
        {
          fprintf(stderr, "'%s': %s\n", argv[i], cupsGetErrorString());
	  return (1);
	}

        printf("'%s': %s\n", argv[i], cupsHashString(hash, (size_t)hashsize, hex, sizeof(hex)));
      }
    }
  }

  return (0);
}


//
// 'usage()' - Show program usage.
//

static void
usage(FILE *fp)				// I - Output file
{
  fputs("Usage: ./testhash [OPTIONS] [STRINGS]\n", fp);
  fputs("Options:\n", fp);
  fputs("  --help         Show program usage.\n", fp);
  fputs("  -a ALGORITHM   Specify hash algorithm.\n", fp);
  fputs("  -k KEY         Specify HMAC key.\n", fp);
}

