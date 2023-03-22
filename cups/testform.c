//
// Form API unit test program for CUPS.
//
// Copyright © 2023 by OpenPrinting.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

//
// Include necessary headers...
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
  int		status = 0;		// Exit status
  size_t	num_vars;		// Number of variables
  cups_option_t	*vars;			// Variables
  char		*data;			// Form data


  if (argc == 1)
  {
    // Do canned API tests...

  }
  else
  {
    // Parse command-line...
    int		i;			// Looping var
    const char	*opt;			// Current option

    for (i = 1; i < argc; i ++)
    {
      if (!strcmp(argv[i], "--help"))
      {
        // --help
        usage(stdout);
      }
      else if (!strncmp(argv[i], "--", 2))
      {
        // Unknown option
        fprintf(stderr, "testform: Unknown option '%s'.\n", argv[i]);
        usage(stderr);
        status = 1;
        break;
      }
      else if (argv[i][0] == '-')
      {
        // Process options
        for (opt = argv[i] + 1; *opt && !status; opt ++)
        {
          switch (*opt)
          {
            case 'f' : // -f FORM-DATA
                i ++;
                if (i >= argc)
                {
                  fputs("testform: Missing form data after '-f'.\n", stderr);
		  usage(stderr);
		  status = 1;
		  break;
		}

		num_vars = cupsFormDecode(argv[i], &vars);
		if (num_vars == 0)
		{
		  fprintf(stderr, "testform: %s\n", cupsLastErrorString());
		  status = 1;
		}
		else
		{
		  size_t	j;	// Looping var

		  for (j = 0; j < num_vars; j ++)
		    printf("%s=%s\n", vars[j].name, vars[j].value);

		  cupsFreeOptions(num_vars, vars);
		}
                break;

            case 'o' : // -o 'NAME=VALUE [... NAME=VALUE]'
                i ++;
                if (i >= argc)
                {
                  fputs("testform: Missing form data after '-o'.\n", stderr);
		  usage(stderr);
		  status = 1;
		  break;
		}

                num_vars = cupsParseOptions(argv[i], 0, &vars);
                data     = cupsFormEncode(num_vars, vars);

                if (data)
                {
                  puts(data);
		  free(data);
		}
		else
		{
		  fprintf(stderr, "testform: %s\n", cupsLastErrorString());
		  status = 1;
		}

		cupsFreeOptions(num_vars, vars);
                break;

	    default :
		fprintf(stderr, "testform: Unknown option '-%c'.\n", *opt);
		usage(stderr);
		status = 1;
		break;
          }
        }
      }
      else
      {
        // Unknown option...
        fprintf(stderr, "testform: Unknown argument '%s'.\n", argv[i]);
        usage(stderr);
        status = 1;
        break;
      }
    }
  }

  return (status);
}


//
// 'usage()' - Show program usage.
//

static void
usage(FILE *fp)				// I - Output file
{
  fputs("Usage: ./testform [OPTIONS]\n", fp);
  fputs("Options:\n", fp);
  fputs("  --help                            Show program help.\n", fp);
  fputs("  -f FORM-DATA                      Decode form data.\n", fp);
  fputs("  -o 'NAME=VALUE [... NAME=VALUE]'  Encode form data.\n", fp);
}
