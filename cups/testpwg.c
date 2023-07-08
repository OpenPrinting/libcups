//
// PWG unit test program for CUPS.
//
// Copyright © 2023 by OpenPrinting.
// Copyright © 2009-2016 by Apple Inc.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more information.
//

#include "cups.h"
#include "pwg-private.h"
#include "test-internal.h"
#include <ctype.h>


//
// 'main()' - Main entry.
//

int					// O - Exit status
main(int  argc,				// I - Number of command-line args
     char *argv[])			// I - Command-line arguments
{
  const pwg_media_t *pwg;		// PWG media size
  size_t	i,			// Looping var
		num_media;		// Number of media sizes
  const pwg_media_t *mediatable;	// Media size table


  if (argc > 1)
  {
    // Decode media sizes...
    int	j;				// Looping var

    for (j = 1; j < argc; j ++)
    {
      int	width, length;		// Width and length

      if (isdigit(argv[j][0] & 255) && sscanf(argv[j], "%d,%d", &width, &length) == 2)
        pwg = pwgMediaForSize(width, length);
      else if ((pwg = pwgMediaForLegacy(argv[j])) == NULL)
        pwg = pwgMediaForPWG(argv[j]);

      if (pwg)
        printf("%s = %s (%.2fx%.2fmm", argv[j], pwg->pwg, pwg->width * 0.01, pwg->length * 0.01);
      else
        printf("%s = BAD\n", argv[j]);
    }

    return (0);
  }

  testBegin("pwgMediaForPWG(\"iso_a4_210x297mm\")");
  if ((pwg = pwgMediaForPWG("iso_a4_210x297mm")) == NULL)
    testEndMessage(false, "not found");
  else if (strcmp(pwg->pwg, "iso_a4_210x297mm"))
    testEndMessage(false, "%s", pwg->pwg);
  else if (pwg->width != 21000 || pwg->length != 29700)
    testEndMessage(false, "%dx%d", pwg->width, pwg->length);
  else
    testEnd(true);

  testBegin("pwgMediaForPWG(\"roll_max_36.1025x3622.0472in\")");
  if ((pwg = pwgMediaForPWG("roll_max_36.1025x3622.0472in")) == NULL)
    testEndMessage(false, "not found");
  else if (pwg->width != 91700 || pwg->length != 9199999)
    testEndMessage(false, "%dx%d", pwg->width, pwg->length);
  else
    testEndMessage(true, "%dx%d", pwg->width, pwg->length);

  testBegin("pwgMediaForPWG(\"disc_test_10x100mm\")");
  if ((pwg = pwgMediaForPWG("disc_test_10x100mm")) == NULL)
    testEndMessage(false, "not found");
  else if (pwg->width != 10000 || pwg->length != 10000)
    testEndMessage(false, "%dx%d", pwg->width, pwg->length);
  else
    testEndMessage(true, "%dx%d", pwg->width, pwg->length);

  testBegin("pwgMediaForLegacy(\"na-letter\")");
  if ((pwg = pwgMediaForLegacy("na-letter")) == NULL)
    testEndMessage(false, "not found");
  else if (strcmp(pwg->pwg, "na_letter_8.5x11in"))
    testEndMessage(false, "%s", pwg->pwg);
  else if (pwg->width != 21590 || pwg->length != 27940)
    testEndMessage(false, "%dx%d", pwg->width, pwg->length);
  else
    testEnd(true);

  testBegin("pwgMediaForPPD(\"4x6\")");
  if ((pwg = pwgMediaForPPD("4x6")) == NULL)
    testEndMessage(false, "not found");
  else if (strcmp(pwg->pwg, "na_index-4x6_4x6in"))
    testEndMessage(false, "%s", pwg->pwg);
  else if (pwg->width != 10160 || pwg->length != 15240)
    testEndMessage(false, "%dx%d", pwg->width, pwg->length);
  else
    testEnd(true);

  testBegin("pwgMediaForPPD(\"10x15cm\")");
  if ((pwg = pwgMediaForPPD("10x15cm")) == NULL)
    testEndMessage(false, "not found");
  else if (strcmp(pwg->pwg, "om_100x150mm_100x150mm"))
    testEndMessage(false, "%s", pwg->pwg);
  else if (pwg->width != 10000 || pwg->length != 15000)
    testEndMessage(false, "%dx%d", pwg->width, pwg->length);
  else
    testEnd(true);

  testBegin("pwgMediaForPPD(\"Custom.10x15cm\")");
  if ((pwg = pwgMediaForPPD("Custom.10x15cm")) == NULL)
    testEndMessage(false, "not found");
  else if (strcmp(pwg->pwg, "custom_10x15cm_100x150mm"))
    testEndMessage(false, "%s", pwg->pwg);
  else if (pwg->width != 10000 || pwg->length != 15000)
    testEndMessage(false, "%dx%d", pwg->width, pwg->length);
  else
    testEnd(true);

  testBegin("pwgMediaForSize(29700, 42000)");
  if ((pwg = pwgMediaForSize(29700, 42000)) == NULL)
    testEndMessage(false, "not found");
  else if (strcmp(pwg->pwg, "iso_a3_297x420mm"))
    testEndMessage(false, "%s", pwg->pwg);
  else
    testEnd(true);

  testBegin("pwgMediaForSize(9842, 19050)");
  if ((pwg = pwgMediaForSize(9842, 19050)) == NULL)
    testEndMessage(false, "not found");
  else if (strcmp(pwg->pwg, "na_monarch_3.875x7.5in"))
    testEndMessage(false, "%s", pwg->pwg);
  else
    testEndMessage(true, "%s", pwg->pwg);

  testBegin("pwgMediaForSize(9800, 19000)");
  if ((pwg = pwgMediaForSize(9800, 19000)) == NULL)
    testEndMessage(false, "not found");
  else if (strcmp(pwg->pwg, "jpn_you6_98x190mm"))
    testEndMessage(false, "%s", pwg->pwg);
  else
    testEndMessage(true, "%s", pwg->pwg);

  testBegin("Duplicate size test");
  for (mediatable = _pwgMediaTable(&num_media); num_media > 1; num_media --, mediatable ++)
  {
    for (i = num_media - 1, pwg = mediatable + 1; i > 0; i --, pwg ++)
    {
      if (pwg->width == mediatable->width && pwg->length == mediatable->length)
      {
	testEndMessage(false, "%s and %s have the same dimensions (%dx%d)", pwg->pwg, mediatable->pwg, pwg->width, pwg->length);
	break;
      }
    }

    if (i > 0)
      break;
  }

  if (num_media == 1)
    testEnd(true);

  return (testsPassed ? 0 : 1);
}
