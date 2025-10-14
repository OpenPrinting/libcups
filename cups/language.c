//
// I18N/language support for CUPS.
//
// Copyright © 2022-2025 by OpenPrinting.
// Copyright © 2007-2017 by Apple Inc.
// Copyright © 1997-2007 by Easy Software Products.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#include "cups-private.h"
#include <sys/stat.h>
#if _WIN32
#  include <io.h>
#else
#  include <unistd.h>
#endif // _WIN32

#include "strings/ca_strings.h"
#include "strings/cs_strings.h"
#include "strings/da_strings.h"
#include "strings/de_strings.h"
#include "strings/en_strings.h"
#include "strings/es_strings.h"
#include "strings/fr_strings.h"
#include "strings/it_strings.h"
#include "strings/ja_strings.h"
#include "strings/pt_BR_strings.h"
#include "strings/ru_strings.h"
#include "strings/zh_CN_strings.h"


//
// Types...
//

typedef struct _cups_message_s		// Message catalog entry
{
  char			*key,		// Key string
			*text;		// Localized text string
} _cups_message_t;

struct _cups_lang_s			// Language Cache
{
  cups_lang_t		*next;		// Next language in cache
  cups_rwlock_t		rwlock;		// Reader/writer lock
  char			language[16];	// Language/locale name
  size_t		num_messages,	// Number of messages
			alloc_messages;	// Allocated messages
  _cups_message_t	*messages;	// Messages
};


//
// Local globals...
//

static cups_mutex_t	lang_mutex = CUPS_MUTEX_INITIALIZER;
					// Mutex to control access to cache
static cups_lang_t	*lang_cache = NULL;
					// Language string cache
static char		*lang_directory = NULL;
					// Directory for strings files...


//
// Local functions...
//

static cups_lang_t	*cups_lang_new(const char *language);
static int		cups_message_compare(_cups_message_t *m1, _cups_message_t *m2);


//
// 'cupsLangAddStrings()' - Add strings for the specified language.
//

bool					// O - `true` on success, `false` on failure
cupsLangAddStrings(
    const char *language,		// I - Language name
    const char *strings)		// I - Contents of ".strings" file
{
  cups_lang_t	*lang;			// Language data


  if ((lang = cupsLangFind(language)) != NULL)
    return (cupsLangLoadStrings(lang, NULL, strings));
  else
    return (false);
}


//
// 'cupsLangFind()' - Find a language localization.
//

cups_lang_t *				// O - Language data
cupsLangFind(const char *language)	// I - Language or locale name
{
  char		langname[16];		// Requested language name
  cups_lang_t	*lang;			// Current language...


  DEBUG_printf("2cupsLangFind(language=\"%s\")", language);

  if (!language)
    return (cupsLangDefault());

  cupsMutexLock(&lang_mutex);

  cupsCopyString(langname, language, sizeof(langname));
  if (langname[2] == '-')
    langname[2] = '_';

  for (lang = lang_cache; lang; lang = lang->next)
  {
    if (!_cups_strcasecmp(lang->language, langname))
      break;
  }

  if (!lang)
  {
    // Create the language if it doesn't exist...
    lang = cups_lang_new(langname);
  }

  cupsMutexUnlock(&lang_mutex);

  return (lang);
}


//
// 'cupsLangFormatString()' - Create a localized formatted string.
//

const char *				// O - Formatted string
cupsLangFormatString(
    cups_lang_t *lang,			// I - Language data
    char        *buffer,		// I - Output buffer
    size_t      bufsize,		// I - Size of output buffer
    const char  *format,		// I - Printf-style format string
    ...)				// I - Additional arguments
{
  va_list	ap;			// Pointer to additional arguments


  va_start(ap, format);
  vsnprintf(buffer, bufsize, cupsLangGetString(lang, format), ap);
  va_end(ap);

  return (buffer);
}


//
// 'cupsLangGetName()' - Get the language name.
//

const char *				// O - Language name
cupsLangGetName(cups_lang_t *lang)	// I - Language data
{
  return (lang ? lang->language : NULL);
}


//
// 'cupsLangGetString()' - Get a localized message string.
//
// This function gets a localized UTF-8 message string for the specified
// language.  If the message is not localized, the original message pointer is
// returned.
//

const char *				// O - Localized message
cupsLangGetString(cups_lang_t *lang,	// I - Language
                  const char  *message)	// I - Message
{
  _cups_message_t	key,		// Search key
			*match;		// Matching message
  const char		*text;		// Localized message text


  DEBUG_printf("cupsLangGetString(lang=%p(%s), message=\"%s\")", (void *)lang, lang ? lang->language : "null", message);

  // Range check input...
  if (!lang || !lang->num_messages || !message || !*message)
    return (message);

  cupsRWLockRead(&lang->rwlock);

  key.key = (char *)message;
  match   = bsearch(&key, lang->messages, lang->num_messages, sizeof(_cups_message_t), (int (*)(const void *, const void *))cups_message_compare);
  text    = match ? match->text : message;

  cupsRWUnlock(&lang->rwlock);

  return (text);
}


//
// 'cupsLangIsRTL()' - Get the writing direction.
//
// This function gets the writing direction for the specified language.
//

bool					// O - `true` if right-to-left, `false` if left-to-right
cupsLangIsRTL(cups_lang_t *lang)	// I - Language
{
  // Range check input...
  if (!lang)
    return (false);

  // The following languages are written from right to left: Arabic, Aramaic,
  // Azeri, Divehi, Fulah, Hebrew, Kurdish, N'ko, Persian, Rohingya, Syriac, and
  // Urdu.  Not all of these have language codes...
  return (!strncmp(lang->language, "ar", 2) || !strncmp(lang->language, "dv", 2) || !strncmp(lang->language, "ff", 2) || !strncmp(lang->language, "he", 2) || !strncmp(lang->language, "ku", 2) || !strncmp(lang->language, "fa", 2) || !strncmp(lang->language, "ur", 2));
}


//
// 'cupsLangLoadStrings()' - Load a message catalog for a language.
//

bool				// O - `true` on success, `false` on failure
cupsLangLoadStrings(
    cups_lang_t *lang,			// I - Language data
    const char  *filename,		// I - Filename of `NULL` for none
    const char  *strings)		// I - Strings or `NULL` for none
{
  bool		ret = true;		// Return value
  int		linenum;		// Current line number in data
  const char	*data,			// Pointer to strings data
		*dataptr;		// Pointer into string data
  char		key[1024],		// Key string
		text[1024],		// Localized text string
		*ptr;			// Pointer into strings
  _cups_message_t *m,			// Pointer to message
		mkey;			// Search key
  size_t	num_messages;		// New number of messages


  if (filename)
  {
    // Load the strings file...
    int		fd;			// File descriptor
    struct stat	fileinfo;		// File information
    ssize_t	bytes;			// Bytes read

    if ((fd = open(filename, O_RDONLY)) < 0)
    {
      _cupsSetError(IPP_STATUS_ERROR_INTERNAL, strerror(errno), 0);
      return (false);
    }

    if (fstat(fd, &fileinfo))
    {
      _cupsSetError(IPP_STATUS_ERROR_INTERNAL, strerror(errno), 0);
      close(fd);
      return (false);
    }

    if ((ptr = malloc((size_t)(fileinfo.st_size + 1))) == NULL)
    {
      _cupsSetError(IPP_STATUS_ERROR_INTERNAL, strerror(errno), 0);
      close(fd);
      return (false);
    }

    if ((bytes = read(fd, ptr, (size_t)fileinfo.st_size)) < 0)
    {
      _cupsSetError(IPP_STATUS_ERROR_INTERNAL, strerror(errno), 0);
      close(fd);
      free(ptr);
      return (false);
    }

    close(fd);

    ptr[bytes] = '\0';
    data       = ptr;
  }
  else
  {
    // Use in-memory strings data...
    data = (const char *)strings;
  }

  if (!data)
  {
    _cupsSetError(IPP_STATUS_ERROR_INTERNAL, strerror(EINVAL), 0);
    return (false);
  }

  // Scan the in-memory strings data and add key/text pairs...
  //
  // Format of strings files is:
  //
  // "key" = "text";
  cupsRWLockWrite(&lang->rwlock);

  num_messages = lang->num_messages;
  mkey.key     = key;

  for (dataptr = data, linenum = 1; *dataptr; dataptr ++)
  {
    // Skip leading whitespace...
    while (*dataptr && isspace(*dataptr & 255))
    {
      if (*dataptr == '\n')
        linenum ++;

      dataptr ++;
    }

    if (!*dataptr)
    {
      // End of string...
      break;
    }
    else if (*dataptr == '/' && dataptr[1] == '*')
    {
      // Start of C-style comment...
      for (dataptr += 2; *dataptr; dataptr ++)
      {
        if (*dataptr == '*' && dataptr[1] == '/')
	{
	  dataptr += 2;
	  break;
	}
	else if (*dataptr == '\n')
	  linenum ++;
      }

      if (!*dataptr)
        break;
    }
    else if (*dataptr != '\"')
    {
      // Something else we don't recognize...
      snprintf(text, sizeof(text), "Syntax error on line %d of '%s'.", linenum, filename ? filename : "in-memory");
      _cupsSetError(IPP_STATUS_ERROR_INTERNAL, text, 0);
      ret = false;
      break;
    }

    // Parse key string...
    dataptr ++;
    for (ptr = key; *dataptr && *dataptr != '\"'; dataptr ++)
    {
      if (*dataptr == '\\' && dataptr[1])
      {
        // Escaped character...
        int	ch;			// Character

        dataptr ++;
        if (*dataptr == '\\' || *dataptr == '\'' || *dataptr == '\"')
        {
          ch = *dataptr;
	}
	else if (*dataptr == 'n')
	{
	  ch = '\n';
	}
	else if (*dataptr == 'r')
	{
	  ch = '\r';
	}
	else if (*dataptr == 't')
	{
	  ch = '\t';
	}
	else if (*dataptr >= '0' && *dataptr <= '3' && dataptr[1] >= '0' && dataptr[1] <= '7' && dataptr[2] >= '0' && dataptr[2] <= '7')
	{
	  // Octal escape
	  ch = ((*dataptr - '0') << 6) | ((dataptr[1] - '0') << 3) | (dataptr[2] - '0');
	  dataptr += 2;
	}
	else
	{
	  snprintf(text, sizeof(text), "Invalid escape in key string on line %d of '%s'.", linenum, filename ? filename : "in-memory");
	  _cupsSetError(IPP_STATUS_ERROR_INTERNAL, text, 0);
	  ret = false;
	  break;
	}

        if (ptr < (key + sizeof(key) - 1))
          *ptr++ = (char)ch;
      }
      else if (ptr < (key + sizeof(key) - 1))
      {
        *ptr++ = *dataptr;
      }
    }

    if (!*dataptr)
    {
      snprintf(text, sizeof(text), "Unterminated key string on line %d of '%s'.", linenum, filename ? filename : "in-memory");
      _cupsSetError(IPP_STATUS_ERROR_INTERNAL, text, 0);
      ret = false;
      break;
    }

    dataptr ++;
    *ptr = '\0';

    // Parse separator...
    while (*dataptr && isspace(*dataptr & 255))
    {
      if (*dataptr == '\n')
        linenum ++;

      dataptr ++;
    }

    if (*dataptr != '=')
    {
      snprintf(text, sizeof(text), "Missing separator on line %d of '%s'.", linenum, filename ? filename : "in-memory");
      _cupsSetError(IPP_STATUS_ERROR_INTERNAL, text, 0);
      ret = false;
      break;
    }

    dataptr ++;
    while (*dataptr && isspace(*dataptr & 255))
    {
      if (*dataptr == '\n')
        linenum ++;

      dataptr ++;
    }

    if (*dataptr != '\"')
    {
      snprintf(text, sizeof(text), "Missing text string on line %d of '%s'.", linenum, filename ? filename : "in-memory");
      _cupsSetError(IPP_STATUS_ERROR_INTERNAL, text, 0);
      ret = false;
      break;
    }

    // Parse text string...
    dataptr ++;
    for (ptr = text; *dataptr && *dataptr != '\"'; dataptr ++)
    {
      if (*dataptr == '\\')
      {
        // Escaped character...
        int	ch;			// Character

        dataptr ++;
        if (*dataptr == '\\' || *dataptr == '\'' || *dataptr == '\"')
        {
          ch = *dataptr;
	}
	else if (*dataptr == 'n')
	{
	  ch = '\n';
	}
	else if (*dataptr == 'r')
	{
	  ch = '\r';
	}
	else if (*dataptr == 't')
	{
	  ch = '\t';
	}
	else if (*dataptr >= '0' && *dataptr <= '3' && dataptr[1] >= '0' && dataptr[1] <= '7' && dataptr[2] >= '0' && dataptr[2] <= '7')
	{
	  // Octal escape
	  ch = ((*dataptr - '0') << 6) | ((dataptr[1] - '0') << 3) | (dataptr[2] - '0');
	  dataptr += 2;
	}
	else
	{
	  snprintf(text, sizeof(text), "Invalid escape in text string on line %d of '%s'.", linenum, filename ? filename : "in-memory");
	  _cupsSetError(IPP_STATUS_ERROR_INTERNAL, text, 0);
	  ret = false;
	  break;
	}

        if (ptr < (text + sizeof(text) - 1))
          *ptr++ = (char)ch;
      }
      else if (ptr < (text + sizeof(text) - 1))
      {
        *ptr++ = *dataptr;
      }
    }

    if (!*dataptr)
    {
      snprintf(text, sizeof(text), "Unterminated text string on line %d of '%s'.", linenum, filename ? filename : "in-memory");
      _cupsSetError(IPP_STATUS_ERROR_INTERNAL, text, 0);
      ret = false;
      break;
    }

    dataptr ++;
    *ptr = '\0';

    // Look for terminator, then add the pair...
    if (*dataptr != ';')
    {
      snprintf(text, sizeof(text), "Missing terminator on line %d of '%s'.", linenum, filename ? filename : "in-memory");
      _cupsSetError(IPP_STATUS_ERROR_INTERNAL, text, 0);
      ret = false;
      break;
    }

    dataptr ++;

    // Add the message if it doesn't already exist...
    if (lang->num_messages > 0 && bsearch(&mkey, lang->messages, lang->num_messages, sizeof(_cups_message_t), (int (*)(const void *, const void *))cups_message_compare))
      continue;

    if (num_messages >= lang->alloc_messages)
    {
      if ((m = realloc(lang->messages, (lang->alloc_messages + 1024) * sizeof(_cups_message_t))) == NULL)
      {
        _cupsSetError(IPP_STATUS_ERROR_INTERNAL, strerror(errno), 0);
        ret = false;
        break;
      }

      lang->messages       = m;
      lang->alloc_messages += 1024;
    }

    m = lang->messages + num_messages;

    if ((m->key = _cupsStrAlloc(key)) == NULL || (m->text = _cupsStrAlloc(text)) == NULL)
    {
      _cupsSetError(IPP_STATUS_ERROR_INTERNAL, strerror(errno), 0);
      _cupsStrFree(m->key);
      _cupsStrFree(m->text);
      ret = false;
      break;
    }

    num_messages ++;
  }

  // Re-sort messages as needed...
  if (num_messages > lang->num_messages)
  {
    lang->num_messages = num_messages;
    qsort(lang->messages, lang->num_messages, sizeof(_cups_message_t), (int (*)(const void *, const void *))cups_message_compare);
  }

  cupsRWUnlock(&lang->rwlock);

  // Free temporary storage and return...
  if (data != strings)
    free((void *)data);

  return (ret);
}


//
// 'cupsLangSetDirectory()' - Set a directory containing localizations.
//

void
cupsLangSetDirectory(const char *d)	// I - Directory name
{
  if (d)
  {
    cupsMutexLock(&lang_mutex);

    free(lang_directory);
    lang_directory = strdup(d);

    cupsMutexUnlock(&lang_mutex);
  }
}


//
// 'cups_lang_new()' - Create a new language.
//

static cups_lang_t *			// O - Language data
cups_lang_new(const char *language)	// I - Language name
{
  cups_lang_t	*lang;			// Language data
  char		filename[1024];		// Strings file...
  bool		status;			// Load status


  // Create an empty language data structure...
  if ((lang = calloc(1, sizeof(cups_lang_t))) == NULL)
    return (NULL);

  cupsRWInit(&lang->rwlock);
  cupsCopyString(lang->language, language, sizeof(lang->language));

  // Add strings...
  if (!_cups_strncasecmp(language, "ca", 2))
    status = cupsLangLoadStrings(lang, NULL, ca_strings);
  else if (!_cups_strncasecmp(language, "cs", 2))
    status = cupsLangLoadStrings(lang, NULL, cs_strings);
  else if (!_cups_strncasecmp(language, "da", 2))
    status = cupsLangLoadStrings(lang, NULL, da_strings);
  else if (!_cups_strncasecmp(language, "de", 2))
    status = cupsLangLoadStrings(lang, NULL, de_strings);
  else if (!_cups_strncasecmp(language, "es", 2))
    status = cupsLangLoadStrings(lang, NULL, es_strings);
  else if (!_cups_strncasecmp(language, "fr", 2))
    status = cupsLangLoadStrings(lang, NULL, fr_strings);
  else if (!_cups_strncasecmp(language, "it", 2))
    status = cupsLangLoadStrings(lang, NULL, it_strings);
  else if (!_cups_strncasecmp(language, "ja", 2))
    status = cupsLangLoadStrings(lang, NULL, ja_strings);
  else if (!_cups_strncasecmp(language, "pt", 2))
    status = cupsLangLoadStrings(lang, NULL, pt_BR_strings);
  else if (!_cups_strncasecmp(language, "ru", 2))
    status = cupsLangLoadStrings(lang, NULL, ru_strings);
  else if (!_cups_strncasecmp(language, "zh", 2))
    status = cupsLangLoadStrings(lang, NULL, zh_CN_strings);
  else
    status = cupsLangLoadStrings(lang, NULL, en_strings);

  if (status && lang_directory)
  {
    snprintf(filename, sizeof(filename), "%s/%s.strings", lang_directory, language);
    if (access(filename, 0) && language[2])
    {
      char	baselang[3];		// Base language name

      cupsCopyString(baselang, language, sizeof(baselang));
      snprintf(filename, sizeof(filename), "%s/%s.strings", lang_directory, baselang);
    }

    if (!access(filename, 0))
      status = cupsLangLoadStrings(lang, filename, NULL);
  }

  if (!status)
  {
    // Free memory if the load failed...
    size_t	i;			// Looping var

    for (i = 0; i < lang->num_messages; i ++)
    {
      _cupsStrFree(lang->messages[i].key);
      _cupsStrFree(lang->messages[i].text);
    }

    free(lang->messages);
    free(lang);

    return (NULL);
  }

  // Add this language to the front of the list...
  lang->next = lang_cache;
  lang_cache = lang;

  return (lang);
}


//
// 'cups_message_compare()' - Compare two messages.
//

static int				// O - Result of comparison
cups_message_compare(
    _cups_message_t *m1,		// I - First message
    _cups_message_t *m2)		// I - Second message
{
  return (strcmp(m1->key, m2->key));
}
