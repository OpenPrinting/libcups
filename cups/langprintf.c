//
// Localized printf/puts functions for CUPS.
//
// Copyright © 2022 by OpenPrinting.
// Copyright © 2007-2014 by Apple Inc.
// Copyright © 2002-2007 by Easy Software Products.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#include "cups-private.h"
#ifdef HAVE_LANGINFO_H
#  include <langinfo.h>
#endif // HAVE_LANGINFO_H
#ifdef HAVE_COREFOUNDATION_H
#  include <CoreFoundation/CoreFoundation.h>
#endif // HAVE_COREFOUNDATION_H


//
// Local functions...
//

static const char	*cups_lang_default(void);


//
// 'cupsLangDefault()' - Return the default language.
//

cups_lang_t *				// O - Language data
cupsLangDefault(void)
{
  _cups_globals_t *cg = _cupsGlobals();	// Global data


  // Load the default language as needed...
  if (!cg->lang_default)
    cg->lang_default = cupsLangFind(cups_lang_default());

  return (cg->lang_default);
}


//
// 'cupsLangGetEncoding()' - Get the default encoding for the current locale.
//

cups_encoding_t				// O - Character encoding
cupsLangGetEncoding(void)
{
  _cups_globals_t *cg = _cupsGlobals();	// Global data


  if (!cg->lang_default)
    cg->lang_default = cupsLangDefault();

  return (cg->lang_encoding);
}


//
// 'cupsLangPrintf()' - Print a formatted message string to a file.
//

ssize_t					// O - Number of bytes written
cupsLangPrintf(FILE       *fp,		// I - File to write to
	       const char *format,	// I - Message string to use
	       ...)			// I - Additional arguments as needed
{
  ssize_t	bytes;			// Number of bytes formatted
  char		buffer[2048],		// Message buffer
		output[8192];		// Output buffer
  va_list 	ap;			// Pointer to additional arguments
  _cups_globals_t *cg = _cupsGlobals();	// Global data


  // Range check...
  if (!fp || !format)
    return (-1);

  if (!cg->lang_default)
    cg->lang_default = cupsLangDefault();

  // Format the string...
  va_start(ap, format);
  vsnprintf(buffer, sizeof(buffer) - 1, cupsLangGetString(cg->lang_default, format), ap);
  va_end(ap);

  cupsConcatString(buffer, "\n", sizeof(buffer));

  // Transcode to the destination charset...
  bytes = cupsUTF8ToCharset(output, buffer, sizeof(output), cg->lang_encoding);

  // Write the string and return the number of bytes written...
  if (bytes > 0)
    return ((ssize_t)fwrite(output, 1, (size_t)bytes, fp));
  else
    return (bytes);
}


//
// 'cupsLangPuts()' - Print a static message string to a file.
//

ssize_t					// O - Number of bytes written
cupsLangPuts(FILE       *fp,		// I - File to write to
             const char *message)	// I - Message string to use
{
  ssize_t	bytes;			// Number of bytes formatted
  char		output[8192];		// Message buffer
  _cups_globals_t *cg = _cupsGlobals();	// Global data


  // Range check...
  if (!fp || !message)
    return (-1);

  if (!cg->lang_default)
    cg->lang_default = cupsLangDefault();

  // Transcode to the destination charset...
  bytes = cupsUTF8ToCharset(output, cupsLangGetString(cg->lang_default, message), sizeof(output) - 4, cg->lang_encoding);
  bytes += cupsUTF8ToCharset(output + bytes, "\n", sizeof(output) - (size_t)bytes, cg->lang_encoding);

  // Write the string and return the number of bytes written...
  if (bytes > 0)
    return ((ssize_t)fwrite(output, 1, (size_t)bytes, fp));
  else
    return (bytes);
}


//
// 'cupsLangSetLocale()' - Set the current locale and transcode the command-line.
//

void
cupsLangSetLocale(char *argv[])		// IO - Command-line arguments
{
  int		i;			// Looping var
  char		buffer[8192];		// Command-line argument buffer
  _cups_globals_t *cg = _cupsGlobals();	// Global data
#ifdef LC_TIME
  const char	*lc_time;		// Current LC_TIME value
  char		new_lc_time[255],	// New LC_TIME value
		*charset;		// Pointer to character set
#endif // LC_TIME


  // Set the locale so that times, etc. are displayed properly.
  //
  // Unfortunately, while we need the localized time value, we *don't*
  // want to use the localized charset for the time value, so we need
  // to set LC_TIME to the locale name with .UTF-8 on the end (if
  // the locale includes a character set specifier...)
  setlocale(LC_ALL, "");

#ifdef LC_TIME
  if ((lc_time = setlocale(LC_TIME, NULL)) == NULL)
    lc_time = setlocale(LC_ALL, NULL);

  if (lc_time)
  {
    cupsCopyString(new_lc_time, lc_time, sizeof(new_lc_time));
    if ((charset = strchr(new_lc_time, '.')) == NULL)
      charset = new_lc_time + strlen(new_lc_time);

    cupsCopyString(charset, ".UTF-8", sizeof(new_lc_time) - (size_t)(charset - new_lc_time));
  }
  else
    cupsCopyString(new_lc_time, "C", sizeof(new_lc_time));

  setlocale(LC_TIME, new_lc_time);
#endif // LC_TIME

  // Initialize the default language info...
  if (!cg->lang_default)
    cg->lang_default = cupsLangDefault();

  // Transcode the command-line arguments from the locale charset to UTF-8...
  if (cg->lang_encoding != CUPS_ENCODING_US_ASCII && cg->lang_encoding != CUPS_ENCODING_UTF_8)
  {
    for (i = 1; argv[i]; i ++)
    {
      // Try converting from the locale charset to UTF-8...
      if (cupsCharsetToUTF8(buffer, argv[i], sizeof(buffer), cg->lang_encoding) < 0)
        continue;

      // Save the new string if it differs from the original...
      if (strcmp(buffer, argv[i]))
        argv[i] = strdup(buffer);
    }
  }
}


//
// 'cups_lang_default()' - Get the default locale name...
//

static const char *			// O - Locale string
cups_lang_default(void)
{
  _cups_globals_t	*cg = _cupsGlobals();
  					// Pointer to library globals
#ifndef __APPLE__
  static const char * const locale_encodings[] =
  {					// Locale charset names
    "ASCII",	"ISO88591",	"ISO88592",	"ISO88593",
    "ISO88594",	"ISO88595",	"ISO88596",	"ISO88597",
    "ISO88598",	"ISO88599",	"ISO885910",	"UTF8",
    "ISO885913",	"ISO885914",	"ISO885915",	"CP874",
    "CP1250",	"CP1251",	"CP1252",	"CP1253",
    "CP1254",	"CP1255",	"CP1256",	"CP1257",
    "CP1258",	"KOI8R",	"KOI8U",	"ISO885911",
    "ISO885916",	"MACROMAN",	"",		"",

    "",		"",		"",		"",
    "",		"",		"",		"",
    "",		"",		"",		"",
    "",		"",		"",		"",
    "",		"",		"",		"",
    "",		"",		"",		"",
    "",		"",		"",		"",
    "",		"",		"",		"",

    "CP932",	"CP936",	"CP949",	"CP950",
    "CP1361",	"GB18030",	"",		"",
    "",		"",		"",		"",
    "",		"",		"",		"",
    "",		"",		"",		"",
    "",		"",		"",		"",
    "",		"",		"",		"",
    "",		"",		"",		"",

    "",		"",		"",		"",
    "",		"",		"",		"",
    "",		"",		"",		"",
    "",		"",		"",		"",
    "",		"",		"",		"",
    "",		"",		"",		"",
    "",		"",		"",		"",
    "",		"",		"",		"",

    "EUCCN",	"EUCJP",	"EUCKR",	"EUCTW",
    "SHIFT_JISX0213"
  };
#endif // !__APPLE__


  DEBUG_puts("2cups_lang_default()");

  // Only lookup the locale the first time.
  if (!cg->lang_name[0])
  {
    const char		*lang;		// LANG environment variable
#ifdef __APPLE__
    CFBundleRef		bundle;		// Main bundle (if any)
    CFArrayRef		bundleList;	// List of localizations in bundle
    CFPropertyListRef 	localizationList = NULL;
					// List of localization data
    CFStringRef		languageName,	// Current language name
			localeName;	// Current locale name

    if (getenv("SOFTWARE") != NULL && (lang = getenv("LANG")) != NULL)
    {
      DEBUG_printf(("3cups_lang_default: Using LANG=%s", lang));
      cupsCopyString(cg->lang_name, lang, sizeof(cg->lang_name));
      return (cg->lang_name);
    }
    else if ((bundle = CFBundleGetMainBundle()) != NULL && (bundleList = CFBundleCopyBundleLocalizations(bundle)) != NULL)
    {
      CFURLRef resources = CFBundleCopyResourcesDirectoryURL(bundle);
					// Resource directory for program

      DEBUG_puts("3cups_lang_default: Getting localizationList from bundle.");

      if (resources)
      {
        CFStringRef	cfpath = CFURLCopyPath(resources);
					// Directory path of bundle
	char		path[1024];	// C string with path

        if (cfpath)
	{
	  // See if we have an Info.plist file in the bundle...
	  CFStringGetCString(cfpath, path, sizeof(path), kCFStringEncodingUTF8);
	  DEBUG_printf(("3cups_lang_default: Got a resource URL (\"%s\")", path));
	  cupsConcatString(path, "Contents/Info.plist", sizeof(path));

          if (!access(path, R_OK))
	    localizationList = CFBundleCopyPreferredLocalizationsFromArray(bundleList);
	  else
	    DEBUG_puts("3cups_lang_default: No Info.plist, ignoring resource URL...");

	  CFRelease(cfpath);
	}

	CFRelease(resources);
      }
      else
      {
        DEBUG_puts("3cups_lang_default: No resource URL.");
      }

      CFRelease(bundleList);
    }

    if (!localizationList)
    {
      DEBUG_puts("3cups_lang_default: Getting localizationList from preferences.");

      localizationList = CFPreferencesCopyAppValue(CFSTR("AppleLanguages"), kCFPreferencesCurrentApplication);
    }

    if (localizationList)
    {
#  ifdef DEBUG
      if (CFGetTypeID(localizationList) == CFArrayGetTypeID())
        DEBUG_printf(("3cups_lang_default: Got localizationList, %d entries.", (int)CFArrayGetCount(localizationList)));
      else
        DEBUG_puts("3cups_lang_default: Got localizationList but not an array.");
#  endif // DEBUG

      // Make sure the localization list is an array...
      if (CFGetTypeID(localizationList) == CFArrayGetTypeID() && CFArrayGetCount(localizationList) > 0)
      {
        // Make sure the first element is a string...
	languageName = CFArrayGetValueAtIndex(localizationList, 0);

	if (languageName && CFGetTypeID(languageName) == CFStringGetTypeID())
	{
	  // Get the locale identifier for the given language...
	  if ((localeName = CFLocaleCreateCanonicalLocaleIdentifierFromString(kCFAllocatorDefault, languageName)) != NULL)
	  {
	    // Use the locale name...
	    if (!CFStringGetCString(localeName, cg->lang_name, (CFIndex)sizeof(cg->lang_name), kCFStringEncodingASCII))
	    {
	      // Use default locale...
	      cg->lang_name[0] = '\0';
	    }

            CFRelease(localeName);
	  }
	  else if (!CFStringGetCString(languageName, cg->lang_name, (CFIndex)sizeof(cg->lang_name), kCFStringEncodingASCII))
	  {
	    // Use default locale...
	    cg->lang_name[0] = '\0';
	  }
	}
      }

      CFRelease(localizationList);
    }

    // Always use UTF-8 on macOS...
    cg->lang_encoding = CUPS_ENCODING_UTF_8;

#else // !__APPLE__
    size_t		i;		// Looping var
    const char		*lptr;		// Pointer into the language
    char		charset[32],	// Character set name
			*csptr;		// Pointer into character set

    // See if the locale has been set; if it is still "C" or "POSIX", use the
    // environment to get the default...
#  ifdef LC_MESSAGES
    lang = setlocale(LC_MESSAGES, NULL);
#  else
    lang = setlocale(LC_ALL, NULL);
#  endif // LC_MESSAGES

    DEBUG_printf(("3cups_lang_default: Current locale is \"%s\".", lang));

    if (!lang || !strcmp(lang, "C") || !strcmp(lang, "POSIX"))
    {
      // Get the character set from the LC_xxx locale setting...
      if ((lang = getenv("LC_CTYPE")) == NULL)
      {
        if ((lang = getenv("LC_ALL")) == NULL)
	{
	  if ((lang = getenv("LANG")) == NULL)
	    lang = "en_US";
	}
      }

      if ((lptr = strchr(lang, '.')) != NULL)
      {
        // Extract the character set from the environment...
	for (csptr = charset, lptr ++; *lptr; lptr ++)
	{
	  if (csptr < (charset + sizeof(charset) - 1) && _cups_isalnum(*lptr))
	    *csptr++ = *lptr;
	}

        *csptr = '\0';
        DEBUG_printf(("3cups_lang_default: Charset set to \"%s\" via environment.", charset));
      }

      // Get the locale for messages from the LC_MESSAGES locale setting...
      if ((lang = getenv("LC_MESSAGES")) == NULL)
      {
        if ((lang = getenv("LC_ALL")) == NULL)
        {
	  if ((lang = getenv("LANG")) == NULL)
	    lang = "en_US";
	}
      }
    }

    if (lang)
    {
      // Copy the language over...
      cupsCopyString(cg->lang_name, lang, sizeof(cg->lang_name));

      if ((lptr = strchr(lang, '.')) != NULL)
      {
        // Extract the character set from the environment...
	for (csptr = charset, lptr ++; *lptr; lptr ++)
	{
	  if (csptr < (charset + sizeof(charset) - 1) && _cups_isalnum(*lptr))
	    *csptr++ = *lptr;
	}

        *csptr = '\0';

        DEBUG_printf(("3cups_lang_default: Charset set to \"%s\" via setlocale().", charset));

        if ((csptr = strchr(cg->lang_name, '.')) != NULL)
          *csptr = '\0';		// Strip charset from locale name...
      }
    }

#ifdef CODESET
    // On systems that support the nl_langinfo(CODESET) call, use this value as
    // the character set...
    if (!charset[0] && (lptr = nl_langinfo(CODESET)) != NULL)
    {
      // Copy all of the letters and numbers in the CODESET string...
      for (csptr = charset; *lptr; lptr ++)
      {
	if (_cups_isalnum(*lptr) && csptr < (charset + sizeof(charset) - 1))
	  *csptr++ = *lptr;
      }
      *csptr = '\0';

      DEBUG_printf(("3cups_lang_default: Charset set to \"%s\" via nl_langinfo(CODESET).", charset));
    }
#endif // CODESET

    // Set the encoding, defaulting to UTF-8...
    cg->lang_encoding = CUPS_ENCODING_AUTO;

    for (i = 0; i < (sizeof(locale_encodings) / sizeof(locale_encodings[0])); i ++)
    {
      if (!_cups_strcasecmp(charset, locale_encodings[i]))
      {
        cg->lang_encoding = (cups_encoding_t)i;
        break;
      }
    }

    if (cg->lang_encoding == CUPS_ENCODING_AUTO)
    {
      // Map alternate names for various character sets...
      if (!_cups_strcasecmp(charset, "iso-2022-jp") || !_cups_strcasecmp(charset, "sjis"))
	cg->lang_encoding = CUPS_ENCODING_WINDOWS_932;
      else if (!_cups_strcasecmp(charset, "iso-2022-cn"))
	cg->lang_encoding = CUPS_ENCODING_WINDOWS_936;
      else if (!_cups_strcasecmp(charset, "iso-2022-kr"))
	cg->lang_encoding = CUPS_ENCODING_WINDOWS_949;
      else if (!_cups_strcasecmp(charset, "big5"))
	cg->lang_encoding = CUPS_ENCODING_WINDOWS_950;
      else
        cg->lang_encoding = CUPS_ENCODING_UTF_8;
    }
#endif // __APPLE__

    // Map default/new locales to their legacy counterparts...
    if (!cg->lang_name[0] || !strcmp(cg->lang_name, "en"))
    {
      // Default to en_US...
      cupsCopyString(cg->lang_name, "en_US", sizeof(cg->lang_name));
    }
    else if (!strncmp(cg->lang_name, "nb", 2))
    {
      // "nb" == Norwegian Bokmal, "no" is the legacy name...
      cupsCopyString(cg->lang_name, "no", sizeof(cg->lang_name));
    }
    else if (!strncmp(cg->lang_name, "zh-Hans", 7) || !strncmp(cg->lang_name, "zh_HANS", 7))
    {
      // Simplified Chinese (China)
      cupsCopyString(cg->lang_name, "zh_CN", sizeof(cg->lang_name));
    }
    else if (!strncmp(cg->lang_name, "zh-Hant", 7) || !strncmp(cg->lang_name, "zh_HANT", 7))
    {
      // Traditional Chinese (Taiwan)
      cupsCopyString(cg->lang_name, "zh_TW", sizeof(cg->lang_name));
    }

    DEBUG_printf(("3cups_lang_default: Using locale \"%s\".", cg->lang_name));
  }
  else
  {
    DEBUG_printf(("3cups_lang_default: Using previous locale \"%s\".", cg->lang_name));
  }

  // Return the cached locale...
  return (cg->lang_name);
}
