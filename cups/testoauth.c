//
// OAuth API unit tests for CUPS.
//
// Copyright © 2024 by OpenPrinting.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#include "cups.h"
#include "oauth.h"
#include "test-internal.h"


//
// Local constants...
//

#define TEST_OAUTH_URI	"https://samples.auth0.com"


//
// Local functions...
//

static int	unit_tests(void);
static int	usage(FILE *out);


//
// 'main()' - Main entry.
//

int					// O - Exit status
main(int  argc,				// I - Number of command-line arguments
     char *argv[])			// I - Command-line arguments
{
  int		i;			// Looping var
  const char	*opt,			// Current option
		*oauth_uri = NULL,	// OAuth authorization server URI
		*command = NULL,	// Command
//		*resource_uri = NULL,	// Resource URI
		*scopes = NULL;		// Scopes


  // Run unit tests if no arguments are specified...
  if (argc == 1)
    return (unit_tests());

  // Otherwise parse the command-line...
  for (i = 1; i < argc; i ++)
  {
    if (!strcmp(argv[i], "--help"))
    {
      return (usage(stdout));
    }
    else if (argv[i][0] == '-' && argv[i][1] != '-')
    {
      for (opt = argv[i] + 1; *opt; opt ++)
      {
        switch (*opt)
        {
          case 'a' : // -a AUTH-URI
              i ++;
              if (i >= argc)
              {
                fputs("testoauth: Missing Authorization Server URI after '-a'.\n", stderr);
                return (usage(stderr));
              }

              oauth_uri = argv[i];
              break;

          case 's' :
              i ++;
              if (i >= argc)
              {
                fputs("testoauth: Missing scope(s) after '-s'.\n", stderr);
                return (usage(stderr));
              }

              scopes = argv[i];
              break;

          default :
              fprintf(stderr, "testoauth: Unknown option '-%c'.\n", *opt);
              return (usage(stderr));
        }
      }
    }
    else if (strncmp(argv[i], "--", 2) && !command)
    {
      command = argv[i];
      i ++;
      break;
    }
    else
    {
      fprintf(stderr, "testoauth: Unknown option '%s'.\n", argv[i]);
      return (usage(stderr));
    }
  }

  if (!oauth_uri)
    oauth_uri = TEST_OAUTH_URI;

  if (oauth_uri)
    puts(oauth_uri);

  if (scopes)
    puts(scopes);

  if (command)
    puts(command);

  return (0);
}


//
// 'unit_tests()' - Run unit tests.
//

static int				// O - Exit status
unit_tests(void)
{
  return (1);
}


//
// 'usage()' - Show usage.
//

static int				// O - Exit status
usage(FILE *out)			// I - Output file
{
  fputs("Usage: testoauth [-a OAUTH-URI] [-s SCOPE(S)] [COMMAND [ARGUMENT(S)]]\n", out);
  fputs("Commands:\n", out);
  fputs("  authorize RESOURCE-URI\n", out);
  fputs("  clear RESOURCE-URI\n", out);
  fputs("  get-access-token RESOURCE-URI\n", out);
  fputs("  get-client-id\n", out);
  fputs("  get-metadata\n", out);
  fputs("  get-refresh-token RESOURCE-URI\n", out);
  fputs("  get-user-id RESOURCE-URI\n", out);
  fputs("  set-client-id CLIENT-ID CLIENT-SECRET\n", out);

  return (out == stdout ? 0 : 1);
}
