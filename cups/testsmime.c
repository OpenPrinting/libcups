//
// S/MIME API unit tests for CUPS.
//
// Copyright © 2023 by OpenPrinting.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#include "smime.h"
#include "test-internal.h"


//
// 'main()' - Main entry.
//

int					// O - Exit status
main(int  argc,				// I - Number of command-line arguments
     char *argv[])			// I - Command-line arguments
{
  (void)argc;
  (void)argv;

  return (testsPassed ? 0 : 1);
}
