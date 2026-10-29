//
// String API unit test program for CUPS.
//
// Copyright © 2026 by OpenPrinting.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#include "string-private.h"
#include "test-internal.h"
#include "cups.h"


//
// 'main()' - Main entry.
//

int					// O - Exit status
main(void)
{
  char		buffer[64],		// Large output buffer
		small[2];		// Buffer too small to hold output
  ssize_t	large_len,		// Required length with a large buffer
		small_len;		// Required length with a small buffer


  // Basic formatting...
  testBegin("cupsFormatString('%%s %%d')");
  large_len = cupsFormatString(buffer, sizeof(buffer), "%s %d", "test", 42);
  testEndMessage(large_len == 7 && !strcmp(buffer, "test 42"), "\"%s\"", buffer);

  // The returned "bytes needed" value must not depend on the output buffer
  // size.  A "%c" conversion followed by more arguments used to drop its
  // argument when the buffer was already full, so every later argument was
  // read from the wrong place (and a following "%s"/"%p" dereferenced a
  // non-pointer value).
  testBegin("cupsFormatString('%%c') argument sync");
  large_len = cupsFormatString(buffer, sizeof(buffer), "X%c%d", 'A', 999999);
  small_len = cupsFormatString(small, sizeof(small), "X%c%d", 'A', 999999);
  testEndMessage(large_len == small_len, "large=%d small=%d", (int)large_len, (int)small_len);

  return (testsPassed ? 0 : 1);
}
