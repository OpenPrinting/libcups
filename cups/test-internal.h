//
// Unit test header for C/C++ programs.
//
// Copyright © 2021 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#ifndef TEST_H
#  define TEST_H
#  include <stdio.h>
#  include <stdlib.h>
#  include <stdarg.h>
#  include <stdbool.h>
#  include <string.h>
#  if _WIN32
#    define isatty(f) _isatty(f)
#  else
#    include <unistd.h>
#  endif // !_WIN32
#  ifdef __cplusplus
extern "C" {
#  endif // __cplusplus

//
// This header implements a simple unit test framework for C/C++ programs.  Inline
// functions are provided to write a test summary to stdout and the details to stderr.
//

static int test_progress;		// Current progress

// Start a test
static inline void
testBegin(const char *title, ...)	// I - printf-style title string
{
  char		buffer[1024];		// Formatted title string
  va_list	ap;			// Pointer to additional arguments


  // Format the title string
  va_start(ap, title);
  vsnprintf(buffer, sizeof(buffer), title, ap);
  va_end(ap);

  // Send the title to stdout and stderr...
  test_progress = 0;

  printf("%s: ", buffer);
  fflush(stdout);

  if (!isatty(2))
    fprintf(stderr, "%s: ", buffer);
}

// End a test with no additional information
static inline void
testEnd(bool pass)			// I - `true` if the test passed, `false` otherwise
{
  // Send the test result to stdout and stderr
  if (test_progress)
    putchar('\b');

  puts(pass ? "PASS" : "FAIL");
  if (!isatty(2))
    fputs(pass ? "PASS\n" : "FAIL\n", stderr);
}


// End a test with no additional information
static inline void
testEndMessage(bool       pass,		// I - `true` if the test passed, `false` otherwise
               const char *message, ...)// I - printf-style message
{
  char		buffer[1024];		// Formatted title string
  va_list	ap;			// Pointer to additional arguments


  // Format the title string
  va_start(ap, message);
  vsnprintf(buffer, sizeof(buffer), message, ap);
  va_end(ap);

  // Send the test result to stdout and stderr
  if (test_progress)
    putchar('\b');

  printf(pass ? "PASS (%s)\n" : "FAIL (%s)\n", buffer);
  if (!isatty(2))
    fprintf(stderr, pass ? "PASS (%s)\n" : "FAIL (%s)\n", buffer);
}

// Show/update a progress spinner
static inline void
testProgress(void)
{
  if (test_progress)
    putchar('\b');
  putchar("-\\|/"[test_progress & 3]);
  fflush(stdout);

  test_progress ++;
}

// Show an error to stderr...
static inline void
testError(const char *error, ...)	// I - printf-style error string
{
  char		buffer[1024];		// Formatted title string
  va_list	ap;			// Pointer to additional arguments


  // Format the error string
  va_start(ap, error);
  vsnprintf(buffer, sizeof(buffer), error, ap);
  va_end(ap);

  // Send the error to stderr...
  fprintf(stderr, "%s\n", buffer);
}

// Show a hex dump of a buffer to stderr...
static inline void
testHexDump(const unsigned char *buffer,// I - Buffer
            size_t              bytes)	// I - Number of bytes
{
  size_t	i, j;			// Looping vars
  int		ch;			// Current ASCII char


  // Show lines of 16 bytes at a time...
  for (i = 0; i < bytes; i += 16)
  {
    // Show the offset...
    fprintf(stderr, "%04x ", (unsigned)i);

    // Then up to 16 bytes in hex...
    for (j = 0; j < 16; j ++)
    {
      if ((i + j) < bytes)
        fprintf(stderr, " %02x", buffer[i + j]);
      else
        fputs("   ", stderr);
    }

    // Then the ASCII representation of the bytes...
    fputs("  ", stderr);

    for (j = 0; j < 16 && (i + j) < bytes; j ++)
    {
      ch = buffer[i + j] & 127;

      if (ch < ' ' || ch == 127)
        fputc('.', stderr);
      else
        fputc(ch, stderr);
    }

    fputc('\n', stderr);
  }
}

#  ifdef __cplusplus
}
#  endif // __cplusplus
#endif // !TEST_H
