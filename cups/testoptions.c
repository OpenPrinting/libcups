/*
 * Option unit test program for CUPS.
 *
 * Copyright © 2021 by OpenPrinting.
 * Copyright © 2008-2016 by Apple Inc.
 *
 * Licensed under Apache License v2.0.  See the file "LICENSE" for more
 * information.
 */

/*
 * Include necessary headers...
 */

#include "cups-private.h"
#include "test-internal.h"


/*
 * 'main()' - Test option processing functions.
 */

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line arguments */
     char *argv[])			/* I - Command-line arguments */
{
  int		status = 0;		/* Exit status */
  size_t	num_options;		/* Number of options */
  cups_option_t	*options;		/* Options */
  const char	*value;			/* Value of an option */
  ipp_t		*request;		/* IPP request */
  ipp_attribute_t *attr;		/* IPP attribute */
  size_t	count;			/* Number of attributes */


  if (argc == 1)
  {
   /*
    * cupsParseOptions()
    */

    testBegin("cupsParseOptions");

    num_options = cupsParseOptions("foo=1234 "
				   "bar=\"One Fish\",\"Two Fish\",\"Red Fish\","
				   "\"Blue Fish\" "
				   "baz={param1=1 param2=2} "
				   "foobar=FOO\\ BAR "
				   "barfoo=barfoo "
				   "barfoo=\"\'BAR FOO\'\" "
				   "auth-info=user,pass\\\\,word\\\\\\\\", 0, &options);

    if (num_options != 6)
    {
      testEndMessage(false, "num_options=%d, expected 6", num_options);
      status ++;
    }
    else if ((value = cupsGetOption("foo", num_options, options)) == NULL || strcmp(value, "1234"))
    {
      testEndMessage(false, "foo=\"%s\", expected \"1234\"", value);
      status ++;
    }
    else if ((value = cupsGetOption("bar", num_options, options)) == NULL || strcmp(value, "One Fish,Two Fish,Red Fish,Blue Fish"))
    {
      testEndMessage(false, "bar=\"%s\", expected \"One Fish,Two Fish,Red Fish,Blue Fish\"", value);
      status ++;
    }
    else if ((value = cupsGetOption("baz", num_options, options)) == NULL || strcmp(value, "{param1=1 param2=2}"))
    {
      testEndMessage(false, "baz=\"%s\", expected \"{param1=1 param2=2}\"", value);
      status ++;
    }
    else if ((value = cupsGetOption("foobar", num_options, options)) == NULL || strcmp(value, "FOO BAR"))
    {
      testEndMessage(false, "foobar=\"%s\", expected \"FOO BAR\"", value);
      status ++;
    }
    else if ((value = cupsGetOption("barfoo", num_options, options)) == NULL || strcmp(value, "\'BAR FOO\'"))
    {
      testEndMessage(false, "barfoo=\"%s\", expected \"\'BAR FOO\'\"", value);
      status ++;
    }
    else if ((value = cupsGetOption("auth-info", num_options, options)) == NULL || strcmp(value, "user,pass\\,word\\\\"))
    {
      testEndMessage(false, "auth-info=\"%s\", expected \"user,pass\\,word\\\\\"", value);
      status ++;
    }
    else
      testEnd(true);

    testBegin("cupsEncodeOptions");
    request = ippNew();
    ippSetOperation(request, IPP_OP_PRINT_JOB);

    cupsEncodeOptions(request, num_options, options, IPP_TAG_JOB);
    for (count = 0, attr = ippFirstAttribute(request); attr; attr = ippNextAttribute(request), count ++);
    if (count != 6)
    {
      testEndMessage(false, "%d attributes, expected 6", count);
      status ++;
    }
    else if ((attr = ippFindAttribute(request, "foo", IPP_TAG_ZERO)) == NULL)
    {
      testEndMessage(false, "Unable to find attribute \"foo\"");
      status ++;
    }
    else if (ippGetValueTag(attr) != IPP_TAG_NAME)
    {
      testEndMessage(false, "\"foo\" of type %s, expected name", ippTagString(ippGetValueTag(attr)));
      status ++;
    }
    else if (ippGetCount(attr) != 1)
    {
      testEndMessage(false, "\"foo\" has %d values, expected 1", (int)ippGetCount(attr));
      status ++;
    }
    else if (strcmp(ippGetString(attr, 0, NULL), "1234"))
    {
      testEndMessage(false, "\"foo\" has value %s, expected 1234", ippGetString(attr, 0, NULL));
      status ++;
    }
    else if ((attr = ippFindAttribute(request, "auth-info", IPP_TAG_ZERO)) == NULL)
    {
      testEndMessage(false, "Unable to find attribute \"auth-info\"");
      status ++;
    }
    else if (ippGetValueTag(attr) != IPP_TAG_TEXT)
    {
      testEndMessage(false, "\"auth-info\" of type %s, expected text", ippTagString(ippGetValueTag(attr)));
      status ++;
    }
    else if (ippGetCount(attr) != 2)
    {
      testEndMessage(false, "\"auth-info\" has %d values, expected 2", (int)ippGetCount(attr));
      status ++;
    }
    else if (strcmp(ippGetString(attr, 0, NULL), "user"))
    {
      testEndMessage(false, "\"auth-info\"[0] has value \"%s\", expected \"user\"", ippGetString(attr, 0, NULL));
      status ++;
    }
    else if (strcmp(ippGetString(attr, 1, NULL), "pass,word\\"))
    {
      testEndMessage(false, "\"auth-info\"[1] has value \"%s\", expected \"pass,word\\\"", ippGetString(attr, 1, NULL));
      status ++;
    }
    else
      testEnd(true);
  }
  else
  {
    size_t		i;		/* Looping var */
    cups_option_t	*option;	/* Current option */


    num_options = cupsParseOptions(argv[1], 0, &options);

    for (i = 0, option = options; i < num_options; i ++, option ++)
      printf("options[%u].name=\"%s\", value=\"%s\"\n", (unsigned)i, option->name, option->value);
  }

  exit (status);
}
