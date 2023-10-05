//
// CUPS API test program for CUPS.
//
// Copyright © 2007-2018 by Apple Inc.
// Copyright © 2007 by Easy Software Products.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more information.
//

#undef _CUPS_NO_DEPRECATED
#include "cups-private.h"
#include <stdlib.h>
#include "test-internal.h"


//
// Local functions...
//

static bool	dests_equal(cups_dest_t *a, cups_dest_t *b);
static bool	enum_cb(void *user_data, unsigned flags, cups_dest_t *dest);
static void	show_diffs(cups_dest_t *a, cups_dest_t *b);


//
// 'main()' - Main entry.
//

int					// O - Exit status
main(int  argc,				// I - Number of command-line arguments
     char *argv[])			// I - Command-line arguments
{
  unsigned	numbers[100];		// Random numbers
  http_t	*http,			// First HTTP connection
		*http2;			// Second HTTP connection
  int		status = 0,		// Exit status
		i;			// Looping var
  size_t	num_dests;		// Number of destinations
  cups_dest_t	*dests,			// Destinations
		*dest,			// Current destination
		*named_dest;		// Current named destination
  const char	*dest_name,		// Destination name
		*dval;			// Destination value
#if 0
  int		num_jobs;		// Number of jobs for queue
  cups_job_t	*jobs;			// Jobs for queue
#endif // 0


  if (argc > 1)
  {
    if (!strcmp(argv[1], "enum"))
    {
      // ./testcups enum [bw color mono duplex simplex staple copies collate
      //                  punch cover bind sort mfp printer large medium small]
      cups_ptype_t	mask = CUPS_PTYPE_LOCAL,
					// Printer type mask
			type = CUPS_PTYPE_LOCAL;
					// Printer type
      int		msec = 0;	// Timeout in milliseconds


      for (i = 2; i < argc; i ++)
      {
        if (isdigit(argv[i][0] & 255) || argv[i][0] == '.')
        {
          msec = (int)(atof(argv[i]) * 1000);
        }
        else if (!_cups_strcasecmp(argv[i], "bw"))
        {
          mask |= CUPS_PTYPE_BW;
          type |= CUPS_PTYPE_BW;
        }
        else if (!_cups_strcasecmp(argv[i], "color"))
        {
          mask |= CUPS_PTYPE_COLOR;
          type |= CUPS_PTYPE_COLOR;
        }
        else if (!_cups_strcasecmp(argv[i], "mono"))
        {
          mask |= CUPS_PTYPE_COLOR;
        }
        else if (!_cups_strcasecmp(argv[i], "duplex"))
        {
          mask |= CUPS_PTYPE_DUPLEX;
          type |= CUPS_PTYPE_DUPLEX;
        }
        else if (!_cups_strcasecmp(argv[i], "simplex"))
        {
          mask |= CUPS_PTYPE_DUPLEX;
        }
        else if (!_cups_strcasecmp(argv[i], "staple"))
        {
          mask |= CUPS_PTYPE_STAPLE;
          type |= CUPS_PTYPE_STAPLE;
        }
        else if (!_cups_strcasecmp(argv[i], "copies"))
        {
          mask |= CUPS_PTYPE_COPIES;
          type |= CUPS_PTYPE_COPIES;
        }
        else if (!_cups_strcasecmp(argv[i], "collate"))
        {
          mask |= CUPS_PTYPE_COLLATE;
          type |= CUPS_PTYPE_COLLATE;
        }
        else if (!_cups_strcasecmp(argv[i], "punch"))
        {
          mask |= CUPS_PTYPE_PUNCH;
          type |= CUPS_PTYPE_PUNCH;
        }
        else if (!_cups_strcasecmp(argv[i], "cover"))
        {
          mask |= CUPS_PTYPE_COVER;
          type |= CUPS_PTYPE_COVER;
        }
        else if (!_cups_strcasecmp(argv[i], "bind"))
        {
          mask |= CUPS_PTYPE_BIND;
          type |= CUPS_PTYPE_BIND;
        }
        else if (!_cups_strcasecmp(argv[i], "sort"))
        {
          mask |= CUPS_PTYPE_SORT;
          type |= CUPS_PTYPE_SORT;
        }
        else if (!_cups_strcasecmp(argv[i], "mfp"))
        {
          mask |= CUPS_PTYPE_MFP;
          type |= CUPS_PTYPE_MFP;
        }
        else if (!_cups_strcasecmp(argv[i], "printer"))
        {
          mask |= CUPS_PTYPE_MFP;
        }
        else if (!_cups_strcasecmp(argv[i], "large"))
        {
          mask |= CUPS_PTYPE_LARGE;
          type |= CUPS_PTYPE_LARGE;
        }
        else if (!_cups_strcasecmp(argv[i], "medium"))
        {
          mask |= CUPS_PTYPE_MEDIUM;
          type |= CUPS_PTYPE_MEDIUM;
        }
        else if (!_cups_strcasecmp(argv[i], "small"))
        {
          mask |= CUPS_PTYPE_SMALL;
          type |= CUPS_PTYPE_SMALL;
        }
        else
        {
          fprintf(stderr, "Unknown argument \"%s\" ignored...\n", argv[i]);
        }
      }

      cupsEnumDests(CUPS_DEST_FLAGS_NONE, msec, NULL, type, mask, enum_cb, NULL);
    }
    else if (!strcmp(argv[1], "password"))
    {
      const char *pass = cupsGetPassword("Password:", NULL, NULL, NULL);
					  // Password string

      if (pass)
	printf("Password entered: %s\n", pass);
      else
	puts("No password entered.");
    }
    else if (!strcmp(argv[1], "print") && argc == 5)
    {
      // ./testcups print printer file interval
      cups_dinfo_t	*dinfo;		// Destination information
      int		interval,	// Interval between writes
			job_id;		// Job ID
      cups_file_t	*fp;		// Print file
      char		buffer[16384];	// Read/write buffer
      ssize_t		bytes;		// Bytes read/written

      if ((dest = cupsGetNamedDest(CUPS_HTTP_DEFAULT, argv[2], NULL)) == NULL)
      {
        printf("Unable to find printer '%s': %s\n", argv[2], cupsGetErrorString());
        return (1);
      }

      if ((dinfo = cupsCopyDestInfo(CUPS_HTTP_DEFAULT, dest)) == NULL)
      {
        printf("Unable to get information about printer '%s': %s\n", argv[2], cupsGetErrorString());
        return (1);
      }

      if ((fp = cupsFileOpen(argv[3], "r")) == NULL)
      {
	printf("Unable to open \"%s\": %s\n", argv[3], strerror(errno));
	return (1);
      }

      if (cupsCreateDestJob(CUPS_HTTP_DEFAULT, dest, dinfo, &job_id, "testcups", 0, NULL) > IPP_STATUS_OK_CONFLICTING)
      {
	printf("Unable to create print job on '%s': %s\n", argv[2], cupsGetErrorString());
	return (1);
      }

      interval = atoi(argv[4]);

      if (cupsStartDestDocument(CUPS_HTTP_DEFAULT, dest, dinfo, job_id, argv[3], CUPS_FORMAT_AUTO, 0, NULL, true) != HTTP_STATUS_CONTINUE)
      {
	puts("Unable to start document!");
	return (1);
      }

      while ((bytes = cupsFileRead(fp, buffer, sizeof(buffer))) > 0)
      {
	printf("Writing %d bytes...\n", (int)bytes);

	if (cupsWriteRequestData(CUPS_HTTP_DEFAULT, buffer, (size_t)bytes) != HTTP_STATUS_CONTINUE)
	{
	  puts("Unable to write bytes!");
	  return (1);
	}

        if (interval > 0)
	  sleep((unsigned)interval);
      }

      cupsFileClose(fp);

      if (cupsFinishDestDocument(CUPS_HTTP_DEFAULT, dest, dinfo) > IPP_STATUS_OK_IGNORED_OR_SUBSTITUTED)
      {
	puts("Unable to finish document!");
	return (1);
      }
    }
    else
    {
      puts("Usage:");
      puts("");
      puts("Run basic unit tests:");
      puts("");
      puts("    ./testcups");
      puts("");
      puts("Enumerate printers (for N seconds, -1 for indefinitely):");
      puts("");
      puts("    ./testcups enum [seconds]");
      puts("");
      puts("Ask for a password:");
      puts("");
      puts("    ./testcups password");
      puts("");
      puts("Get the PPD file:");
      puts("");
      puts("Print a file (interval controls delay between buffers in seconds):");
      puts("");
      puts("    ./testcups print printer file interval");
      return (1);
    }

    return (0);
  }

  //
  // cupsGetRand()
  //

  testBegin("cupsGetRand");
  for (i = 0; i < 100; i ++)
    numbers[i] = cupsGetRand();
  for (i = 0; i < 99; i ++)
  {
    if (numbers[i] != numbers[i + 1])
      break;
  }
  testEnd(i < 99);
  if (i >= 99)
  {
    for (i = 0; i < 100; i ++)
      testMessage("    numbers[%d]=%u", i, numbers[i]);
  }


  //
  // _cupsConnect() connection reuse...
  //

  testBegin("_cupsConnect");
  http  = _cupsConnect();
  http2 = _cupsConnect();

  if (http == http2)
  {
    testEnd(true);
  }
  else
  {
    testEndMessage(false, "different connections");
    return (1);
  }

  //
  // cupsGetDests()
  //

  testBegin("cupsGetDests");

  num_dests = cupsGetDests(CUPS_HTTP_DEFAULT, &dests);

  if (num_dests == 0)
  {
    testEnd(false);
    status = 1;
  }
  else
  {
    size_t count;			// Count

    testEndMessage(true, "%u dests", (unsigned)num_dests);

    for (count = num_dests, dest = dests; count > 0; count --, dest ++)
    {
      if (dest->instance)
        testMessage("    %s/%s%s", dest->name, dest->instance, dest->is_default ? " **DEFAULT**" : "");
      else
        testMessage("    %s %s", dest->name, dest->is_default ? " **DEFAULT**" : "");
    }
  }

  //
  // cupsGetDest(NULL)
  //

  testBegin("cupsGetDest(NULL)");

  if ((dest = cupsGetDest(NULL, NULL, num_dests, dests)) == NULL)
  {
    size_t count;			// Count

    for (count = num_dests, dest = dests; count > 0; count --, dest ++)
    {
      if (dest->is_default)
        break;
    }

    if (count)
    {
      status = 1;
      testEnd(false);
    }
    else
    {
      testEndMessage(true, "no default");
    }

    dest = NULL;
  }
  else
    testEndMessage(true, "%s", dest->name);

  //
  // cupsGetNamedDest(NULL, NULL, NULL)
  //

  testBegin("cupsGetNamedDest(NULL, NULL, NULL)");

  if ((named_dest = cupsGetNamedDest(NULL, NULL, NULL)) == NULL || !dests_equal(dest, named_dest))
  {
    if (!dest)
    {
      testEndMessage(true, "no default");
    }
    else if (named_dest)
    {
      testEndMessage(false, "different values");
      show_diffs(dest, named_dest);
      status = 1;
    }
    else
    {
      testEndMessage(false, "no default");
      status = 1;
    }
  }
  else
  {
    testEndMessage(true, "%s", named_dest->name);
  }

  if (named_dest)
    cupsFreeDests(1, named_dest);

  //
  // cupsGetDest(printer)
  //

  for (i = 0, dest_name = NULL; i < (int)num_dests; i ++)
  {
    if ((dval = cupsGetOption("printer-is-temporary", dests[i].num_options, dests[i].options)) != NULL && !strcmp(dval, "false"))
    {
      dest_name = dests[i].name;
      break;
    }
  }

  testBegin("cupsGetDest(\"%s\"): ", dest_name);

  if ((dest = cupsGetDest(dest_name, NULL, num_dests, dests)) == NULL)
  {
    testEnd(false);
    status = 1;
  }
  else
  {
    testEnd(true);

    //
    // cupsGetNamedDest(NULL, printer, instance)
    //

    testBegin("cupsGetNamedDest(NULL, \"%s\", \"%s\"): ", dest->name, dest->instance);

    if ((named_dest = cupsGetNamedDest(NULL, dest->name, dest->instance)) == NULL || !dests_equal(dest, named_dest))
    {
      if (named_dest)
      {
	testEndMessage(false, "different values");
	show_diffs(dest, named_dest);
      }
      else
      {
	testEndMessage(false, "no destination");
      }

      status = 1;
    }
    else
    {
      testEnd(true);
    }

    if (named_dest)
      cupsFreeDests(1, named_dest);
  }

#if 0
  //
  // cupsPrintFile()
  //

  testBegin("cupsPrintFile");
  fflush(stdout);

  if (cupsPrintFile(dest->name, "../test/testfile.pdf", "Test Page",
                    dest->num_options, dest->options) <= 0)
  {
    testEndMessage(false, "%s", cupsGetErrorString());
    return (1);
  }
  else
    testEnd(true);


  //
  // cupsGetJobs()
  //

  testBegin("cupsGetJobs");
  fflush(stdout);

  num_jobs = cupsGetJobs(&jobs, NULL, 0, -1);

  if (num_jobs == 0)
  {
    testEnd(false);
    return (1);
  }
  else
    testEnd(true);

  cupsFreeJobs(num_jobs, jobs);
  cupsFreeDests(num_dests, dests);
#endif // 0

  return (status);
}


//
// 'dests_equal()' - Determine whether two destinations are equal.
//

static bool				// O - `true` if equal, `false` if not equal
dests_equal(cups_dest_t *a,		// I - First destination
            cups_dest_t *b)		// I - Second destination
{
  size_t	i;			// Looping var
  cups_option_t	*aoption;		// Current option
  const char	*bval;			// Option value


  if (a == b)
    return (true);

  if (!a || !b)
    return (false);

  if (_cups_strcasecmp(a->name, b->name) || (a->instance && !b->instance) || (!a->instance && b->instance) || (a->instance && _cups_strcasecmp(a->instance, b->instance)) || a->num_options != b->num_options)
    return (false);

  for (i = a->num_options, aoption = a->options; i > 0; i --, aoption ++)
  {
    if ((bval = cupsGetOption(aoption->name, b->num_options, b->options)) == NULL || strcmp(aoption->value, bval))
      return (false);
  }

  return (true);
}


//
// 'enum_cb()' - Report additions and removals.
//

static bool				// O - `true` to continue, `false` to stop
enum_cb(void        *user_data,		// I - User data (unused)
        unsigned    flags,		// I - Destination flags
        cups_dest_t *dest)		// I - Destination
{
  int		i;			// Looping var
  cups_option_t	*option;		// Current option


  (void)user_data;

  if (flags & CUPS_DEST_FLAGS_REMOVED)
    printf("Removed '%s':\n", dest->name);
  else
    printf("Added '%s':\n", dest->name);

  for (i = dest->num_options, option = dest->options; i > 0; i --, option ++)
    printf("    %s=\"%s\"\n", option->name, option->value);

  putchar('\n');

  return (true);
}


//
// 'show_diffs()' - Show differences between two destinations.
//

static void
show_diffs(cups_dest_t *a,		// I - First destination
           cups_dest_t *b)		// I - Second destination
{
  size_t	i;			// Looping var
  cups_option_t	*aoption;		// Current option
  cups_option_t	*boption;		// Current option
  const char	*bval;			// Option value


  if (!a || !b)
    return;

  puts("    Item                  cupsGetDest               cupsGetNamedDest");
  puts("    --------------------  ------------------------  ------------------------");

  if (_cups_strcasecmp(a->name, b->name))
    printf("    name                  %-24.24s  %-24.24s\n", a->name, b->name);

  if ((a->instance && !b->instance) ||
      (!a->instance && b->instance) ||
      (a->instance && _cups_strcasecmp(a->instance, b->instance)))
    printf("    instance              %-24.24s  %-24.24s\n",
           a->instance ? a->instance : "(null)",
	   b->instance ? b->instance : "(null)");

  if (a->num_options != b->num_options)
    printf("    num_options           %-24u  %-24u\n", (unsigned)a->num_options, (unsigned)b->num_options);

  for (i = a->num_options, aoption = a->options; i > 0; i --, aoption ++)
  {
    if ((bval = cupsGetOption(aoption->name, b->num_options, b->options)) == NULL || strcmp(aoption->value, bval))
      printf("    %-20.20s  %-24.24s  %-24.24s\n", aoption->name, aoption->value, bval ? bval : "(null)");
  }

  for (i = b->num_options, boption = b->options; i > 0; i --, boption ++)
  {
    if (!cupsGetOption(boption->name, a->num_options, a->options))
      printf("    %-20.20s  %-24.24s  %-24.24s\n", boption->name, boption->value, "(null)");
  }
}
