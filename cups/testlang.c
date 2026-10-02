//
// Localization test program for CUPS.
//
// Usage:
//
//   ./testlang [-l locale] ["String to localize"]
//
// Copyright © 2021-2024 by OpenPrinting.
// Copyright © 2007-2017 by Apple Inc.
// Copyright © 1997-2006 by Easy Software Products.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#include <config.h>
#include "cups.h"
#include "dir.h"
#include "language.h"
#include "test-internal.h"


//
// Local functions...
//

static int	test_language(const char *locale);
static int	test_string(cups_lang_t *language, const char *msgid);
static void	usage(void);


//
// 'main()' - Load the specified language and show the strings for yes and no.
//

int					// O - Exit status
main(int  argc,				// I - Number of command-line arguments
     char *argv[])			// I - Command-line arguments
{
  int		i;			// Looping var
  const char	*opt;			// Current option
  int		errors = 0;		// Number of errors
  int		dotests = 1;		// Do standard tests?
  const char	*lang = NULL;		// Single language test?
  cups_lang_t	*language = NULL;	// Message catalog


  // Parse command-line...
  cupsLangSetLocale(argv);

  for (i = 1; i < argc; i ++)
  {
    if (argv[i][0] == '-')
    {
      if (!strcmp(argv[i], "--help"))
      {
        usage();
      }
      else
      {
        for (opt = argv[i] + 1; *opt; opt ++)
        {
          switch (*opt)
          {
            case 'l' :
                i ++;
                if (i >= argc)
                {
                  usage();
                  return (1);
                }

                lang = argv[i];
		break;

            default :
                usage();
                return (1);
	  }
        }
      }
    }
    else
    {
      if (!language)
        language = cupsLangFind(lang);

      dotests = 0;
      errors += test_string(language, argv[i]);
    }
  }

  if (dotests)
  {
    if (lang)
    {
      // Test a single language...
      errors += test_language(lang);
    }
    else
    {
      // Test all locales we find in LOCALEDIR...
      cups_dir_t	*dir;		// Locale directory
      cups_dentry_t	*dent;		// Directory entry

      if ((dir = cupsDirOpen("strings")) != NULL)
      {
	while ((dent = cupsDirRead(dir)) != NULL)
	{
	  if (strstr(dent->filename, ".strings"))
	  {
	    char	temp[6],	// Temporary anguage
	  		*tempptr;	// Pointer into temporary language

            cupsCopyString(temp, dent->filename, sizeof(temp));
            if ((tempptr = strchr(temp, '.')) != NULL)
              *tempptr = '\0';

	    errors += test_language(temp);
	  }
	}
      }
      else
      {
        // No strings directory, just use the default language...
        errors += test_language(NULL);
      }

      cupsDirClose(dir);
    }

    if (!errors)
      puts("ALL TESTS PASSED");
  }

  return (errors > 0);
}


//
// 'test_language()' - Test a specific language...
//

static int				// O - Number of errors
test_language(const char *lang)		// I - Locale language code, NULL for default
{
  int		errors = 0;		// Number of errors
  cups_lang_t	*language = NULL,	// Message catalog
		*language2 = NULL;	// Message catalog (second time)


  // Override the locale environment as needed...
  if (lang)
  {
    // Test the specified locale code...
    setenv("LANG", lang, 1);
    setenv("SOFTWARE", "CUPS/" LIBCUPS_VERSION, 1);

    testBegin("cupsLangFind(\"%s\")", lang);
    if ((language = cupsLangFind(lang)) == NULL)
    {
      testEnd(false);
      errors ++;
    }
    else if (strcasecmp(cupsLangGetName(language), lang))
    {
      testEndMessage(false, "got \"%s\"", cupsLangGetName(language));
      errors ++;
    }
    else
    {
      testEnd(true);
    }

    testBegin("cupsLangFind(\"%s\") again", lang);
    if ((language2 = cupsLangFind(lang)) == NULL)
    {
      testEnd(false);
      errors ++;
    }
    else if (strcasecmp(cupsLangGetName(language2), lang))
    {
      testEndMessage(false, "got \"%s\"", cupsLangGetName(language2));
      errors ++;
    }
    else if (language2 != language)
    {
      testEndMessage(false, "cache failure");
      errors ++;
    }
    else
    {
      testEnd(true);
    }
  }
  else
  {
    // Test the default locale...
    testBegin("cupsLangDefault");
    if ((language = cupsLangDefault()) == NULL)
    {
      testEnd(false);
      errors ++;
    }
    else
    {
      testEnd(true);
    }

    testBegin("cupsLangDefault again");
    if ((language2 = cupsLangDefault()) == NULL)
    {
      testEnd(false);
      errors ++;
    }
    else if (language2 != language)
    {
      testEndMessage(false, "cache failure");
      errors ++;
    }
    else
    {
      testEnd(true);
    }
  }

  testBegin("cupsLangGetName(language)");
  testEndMessage(true, "\"%s\"", cupsLangGetName(language));

  errors += test_string(language, "Accepted");
  errors += test_string(language, "Self-signed credentials are blocked.");

  if (language != language2)
  {
    testBegin("cupsLangGetName(language2)");
    testEndMessage(true, "\"%s\"", cupsLangGetName(language2));
  }

  return (errors);
}


//
// 'test_string()' - Test the localization of a string.
//

static int				// O - 1 on failure, 0 on success
test_string(cups_lang_t *language,	// I - Language
            const char  *msgid)		// I - Message
{
  const char  *msgstr;			// Localized string


  // Get the localized string and then see if we got what we expected.
  //
  // For the POSIX locale, the string pointers should be the same.
  // For any other locale, the string pointers should be different.
  testBegin("cupsLangGetString(\"%s\")", msgid);
  msgstr = cupsLangGetString(language, msgid);
  if (strcmp(cupsLangGetName(language), "C") && msgid == msgstr)
  {
    testEndMessage(false, "no message catalog loaded");
    return (1);
  }
  else if (!strcmp(cupsLangGetName(language), "C") && msgid != msgstr)
  {
    testEndMessage(false, "POSIX locale is localized");
    return (1);
  }

  testEndMessage(true, "\"%s\"", msgstr);

  return (0);
}


//
// 'usage()' - Show program usage.
//

static void
usage(void)
{
  puts("Usage: ./testlang [-l locale] [\"String to localize\"]");
  puts("");
  puts("If no arguments are specified, all locales are tested.");
}
