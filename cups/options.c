//
// Option routines for CUPS.
//
// Copyright © 2022-2023 by OpenPrinting.
// Copyright © 2007-2017 by Apple Inc.
// Copyright © 1997-2007 by Easy Software Products.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#include "cups-private.h"


//
// Local functions...
//

static int	cups_compare_options(cups_option_t *a, cups_option_t *b);
static size_t	cups_find_option(const char *name, size_t num_options, cups_option_t *option, int *rdiff);


//
// 'cupsAddIntegerOption()' - Add an integer option to an option array.
//
// New option arrays can be initialized simply by passing 0 for the
// "num_options" parameter.
//

size_t					// O  - Number of options
cupsAddIntegerOption(
    const char    *name,		// I  - Name of option
    long           value,		// I  - Value of option
    size_t        num_options,		// I  - Number of options
    cups_option_t **options)		// IO - Pointer to options
{
  char	strvalue[32];			// String value


  snprintf(strvalue, sizeof(strvalue), "%ld", value);

  return (cupsAddOption(name, strvalue, num_options, options));
}


//
// 'cupsAddOption()' - Add an option to an option array.
//
// New option arrays can be initialized simply by passing 0 for the
// "num_options" parameter.
//

size_t					// O  - Number of options
cupsAddOption(const char    *name,	// I  - Name of option
              const char    *value,	// I  - Value of option
	      size_t        num_options,// I  - Number of options
              cups_option_t **options)	// IO - Pointer to options
{
  cups_option_t	*temp;			// Pointer to new option
  size_t	insert;			// Insertion point
  int		diff;			// Result of search


  if (!name || !name[0] || !value || !options)
    return (num_options);

  if (!_cups_strcasecmp(name, "cupsPrintQuality"))
    num_options = cupsRemoveOption("print-quality", num_options, options);
  else if (!_cups_strcasecmp(name, "print-quality"))
    num_options = cupsRemoveOption("cupsPrintQuality", num_options, options);

  // Look for an existing option with the same name...
  if (num_options == 0)
  {
    insert = 0;
    diff   = 1;
  }
  else
  {
    insert = cups_find_option(name, num_options, *options, &diff);

    if (diff > 0)
      insert ++;
  }

  if (diff)
  {
    // No matching option name...
    if ((temp = (cups_option_t *)realloc(num_options ? *options : NULL, sizeof(cups_option_t) * (num_options + 1))) == NULL)
      return (0);

    *options = temp;

    if (insert < num_options)
      memmove(temp + insert + 1, temp + insert, (num_options - insert) * sizeof(cups_option_t));

    temp        += insert;
    temp->name  = _cupsStrAlloc(name);
    num_options ++;
  }
  else
  {
    // Match found; free the old value...
    temp = *options + insert;
    _cupsStrFree(temp->value);
  }

  temp->value = _cupsStrAlloc(value);

  return (num_options);
}


//
// 'cupsFreeOptions()' - Free all memory used by options.
//

void
cupsFreeOptions(
    size_t        num_options,		// I - Number of options
    cups_option_t *options)		// I - Pointer to options
{
  size_t	i;			// Looping var


  if (num_options == 0 || !options)
    return;

  for (i = 0; i < num_options; i ++)
  {
    _cupsStrFree(options[i].name);
    _cupsStrFree(options[i].value);
  }

  free(options);
}


//
// 'cupsGetIntegerOption()' - Get an integer option value.
//
// `LONG_MIN` is returned when the option does not exist, is not an integer, or
// exceeds the range of values for the `long` type.
//

long					// O - Option value or `LONG_MIN`
cupsGetIntegerOption(
    const char    *name,		// I - Name of option
    size_t        num_options,		// I - Number of options
    cups_option_t *options)		// I - Options
{
  const char	*value = cupsGetOption(name, num_options, options);
					// String value of option
  char		*ptr;			// Pointer into string value
  long		lvalue;			// Integer value


  if (!value || !*value)
    return (LONG_MIN);

  lvalue = strtol(value, &ptr, 10);

  if (*ptr)
    return (LONG_MIN);
  else
    return (lvalue);
}


//
// 'cupsGetOption()' - Get an option value.
//

const char *				// O - Option value or @code NULL@
cupsGetOption(const char    *name,	// I - Name of option
              size_t        num_options,// I - Number of options
              cups_option_t *options)	// I - Options
{
  int		diff;			// Result of comparison
  size_t	match;			// Matching index


  if (!name || num_options == 0 || !options)
    return (NULL);

  match = cups_find_option(name, num_options, options, &diff);

  if (!diff)
    return (options[match].value);

  return (NULL);
}


//
// 'cupsParseOptions()' - Parse options from a command-line argument.
//
// This function converts space-delimited name/value pairs according
// to the PAPI text option ABNF specification. Collection values
// ("name={a=... b=... c=...}") are stored with the curley brackets
// intact - use @code cupsParseOptions@ on the value to extract the
// collection attributes.
//

size_t					// O - Number of options found
cupsParseOptions(
    const char    *arg,			// I - Argument to parse
    size_t        num_options,		// I - Number of options
    cups_option_t **options)		// O - Options found
{
  char	*copyarg,			// Copy of input string
	*ptr,				// Pointer into string
	*name,				// Pointer to name
	*value,				// Pointer to value
	sep,				// Separator character
	quote;				// Quote character


  // Range check input...
  if (!arg)
    return (num_options);

  if (!options)
    return (0);

  // Make a copy of the argument string and then divide it up...
  if ((copyarg = strdup(arg)) == NULL)
  {
    DEBUG_puts("1cupsParseOptions: Unable to copy arg string");
    return (num_options);
  }

  if (*copyarg == '{')
  {
    // Remove surrounding {} so we can parse "{name=value ... name=value}"...
    if ((ptr = copyarg + strlen(copyarg) - 1) > copyarg && *ptr == '}')
    {
      *ptr = '\0';
      ptr  = copyarg + 1;
    }
    else
      ptr = copyarg;
  }
  else
  {
    ptr = copyarg;
  }

  // Skip leading spaces...
  while (_cups_isspace(*ptr))
    ptr ++;

  // Loop through the string...
  while (*ptr != '\0')
  {
    // Get the name up to a SPACE, =, or end-of-string...
    name = ptr;
    while (!strchr("\f\n\r\t\v =", *ptr) && *ptr)
      ptr ++;

    // Avoid an empty name...
    if (ptr == name)
      break;

    // Skip trailing spaces...
    while (_cups_isspace(*ptr))
      *ptr++ = '\0';

    if ((sep = *ptr) == '=')
      *ptr++ = '\0';

    if (sep != '=')
    {
      // Boolean option...
      if (!_cups_strncasecmp(name, "no", 2))
        num_options = cupsAddOption(name + 2, "false", num_options, options);
      else
        num_options = cupsAddOption(name, "true", num_options, options);

      continue;
    }

    // Remove = and parse the value...
    value = ptr;

    while (*ptr && !_cups_isspace(*ptr))
    {
      if (*ptr == ',')
      {
        ptr ++;
      }
      else if (*ptr == '\'' || *ptr == '\"')
      {
        // Quoted string constant...
	quote = *ptr;
	_cups_strcpy(ptr, ptr + 1);

	while (*ptr != quote && *ptr)
	{
	  if (*ptr == '\\' && ptr[1])
	    _cups_strcpy(ptr, ptr + 1);

	  ptr ++;
	}

	if (*ptr)
	  _cups_strcpy(ptr, ptr + 1);
      }
      else if (*ptr == '{')
      {
        // Collection value...
	int depth;			// Nesting depth for braces

	for (depth = 0; *ptr; ptr ++)
	{
	  if (*ptr == '{')
	  {
	    depth ++;
	  }
	  else if (*ptr == '}')
	  {
	    depth --;
	    if (!depth)
	    {
	      ptr ++;
	      break;
	    }
	  }
	  else if (*ptr == '\\' && ptr[1])
	  {
	    _cups_strcpy(ptr, ptr + 1);
	  }
	}
      }
      else
      {
        // Normal space-delimited string...
	while (*ptr && !_cups_isspace(*ptr))
	{
	  if (*ptr == '\\' && ptr[1])
	    _cups_strcpy(ptr, ptr + 1);

	  ptr ++;
	}
      }
    }

    if (*ptr != '\0')
      *ptr++ = '\0';

    // Skip trailing whitespace...
    while (_cups_isspace(*ptr))
      ptr ++;

    // Add the string value...
    num_options = cupsAddOption(name, value, num_options, options);
  }

  // Free the copy of the argument we made and return the number of options found.
  free(copyarg);

  return (num_options);
}


//
// 'cupsRemoveOption()' - Remove an option from an option array.
//

size_t					// O  - New number of options
cupsRemoveOption(
    const char    *name,		// I  - Option name
    size_t        num_options,		// I  - Current number of options
    cups_option_t **options)		// IO - Options
{
  size_t	i;			// Looping var
  cups_option_t	*option;		// Current option


  // Range check input...
  if (!name || num_options == 0 || !options)
    return (num_options);

  // Loop for the option...
  for (i = num_options, option = *options; i > 0; i --, option ++)
  {
    if (!_cups_strcasecmp(name, option->name))
      break;
  }

  if (i)
  {
    // Remove this option from the array...
    num_options --;
    i --;

    _cupsStrFree(option->name);
    _cupsStrFree(option->value);

    if (i > 0)
      memmove(option, option + 1, i * sizeof(cups_option_t));
  }

  // Return the new number of options...
  return (num_options);
}


//
// 'cups_compare_options()' - Compare two options.
//

static int				// O - Result of comparison
cups_compare_options(cups_option_t *a,	// I - First option
		     cups_option_t *b)	// I - Second option
{
  return (_cups_strcasecmp(a->name, b->name));
}


//
// 'cups_find_option()' - Find an option using a binary search.
//

static size_t				// O - Index of match
cups_find_option(
    const char    *name,		// I - Option name
    size_t        num_options,		// I - Number of options
    cups_option_t *options,		// I - Options
    int           *rdiff)		// O - Difference of match
{
  size_t	left,			// Low mark for binary search
		right,			// High mark for binary search
		current;		// Current index
  int		diff;			// Result of comparison
  cups_option_t	key;			// Search key


  key.name = (char *)name;

  // Start search in the middle...
  left  = 0;
  right = num_options - 1;

  do
  {
    current = (left + right) / 2;
    diff    = cups_compare_options(&key, options + current);

    if (diff == 0)
      break;
    else if (diff < 0)
      right = current;
    else
      left = current;
  }
  while ((right - left) > 1);

  if (diff != 0)
  {
    // Check the last 1 or 2 elements...
    if ((diff = cups_compare_options(&key, options + left)) <= 0)
    {
      current = left;
    }
    else
    {
      diff    = cups_compare_options(&key, options + right);
      current = right;
    }
  }

  // Return the closest destination and the difference...
  *rdiff = diff;

  return (current);
}
