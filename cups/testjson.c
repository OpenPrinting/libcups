//
// JSON API unit tests for CUPS.
//
// Copyright © 2022 by OpenPrinting.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#include "cups.h"
#include "json.h"
#include "test-internal.h"


//
// 'main()' - Main entry.
//

int					// O - Exit status
main(int  argc,				// I - Number of command-line arguments
     char *argv[])			// I - Command-line arguments
{
  int		i;			// Looping var
  cups_json_t	*json;			// JSON root object


  if (argc == 1)
  {
    // Do unit tests...
  }
  else
  {
    // Try loading JSON files/strings on the command-line...
    for (i = 1; i < argc; i ++)
    {
      if (argv[i][0] == '{')
      {
        // Load JSON string...
        if ((json = cupsJSONLoadString(argv[i])) != NULL)
          printf("string%d: OK, %u key/value pairs in root object.\n", i, (unsigned)(cupsJSONGetCount(json) / 2));
        else
          fprintf(stderr, "string%d: %s\n", i, cupsLastErrorString());
      }
      else if ((json = cupsJSONLoadFile(argv[i])) != NULL)
	printf("%s: OK, %u key/value pairs in root object.\n", argv[i], (unsigned)(cupsJSONGetCount(json) / 2));
      else
	fprintf(stderr, "%s: %s\n", argv[i], cupsLastErrorString());

      cupsJSONDelete(json);
    }
  }

  return (0);
}
