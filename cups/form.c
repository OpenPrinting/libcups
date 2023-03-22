//
// Form API functions for CUPS.
//
// Copyright © 2023 by OpenPrinting.
// Copyright © 2017-2022 by Michael R Sweet
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#include "cups-private.h"


//
// Local functions...
//

static const char	*decode_string(const char *data, char term, char *buffer, size_t bufsize);
static char		*encode_string(const char *s, char *bufptr, char *bufend);


//
// 'cupsFormDecode()' - Decode URL-encoded form data.
//
// This function decodes URL-encoded form data, returning the number of
// variables and a pointer to a @link cups_option_t@ array of the variables.
//
// Use the @link cupsFreeOptions@ function to return the memory used when you
// are done with the form variables.
//

size_t					// O - Number of variables
cupsFormDecode(const char    *data,	// I - URL-encoded form data
               cups_option_t **vars)	// O - Array of variables
{
  size_t	num_vars = 0;		// Number of variables
  char		name[1024],		// Variable name
		value[4096];		// Variable value


  // Range check...
  if (vars)
    *vars = NULL;

  if (!data || !*data || !vars)
    return (0);

  // Scan the string for "name=value" pairs, unescaping values as needed.
  while (*data)
  {
    // Get the name and value...
    data = decode_string(data, '=', name, sizeof(name));

    if (!data || *data != '=')
      goto decode_error;

    data ++;

    data = decode_string(data, '&', value, sizeof(value));

    if (!data || (*data && *data != '&'))
    {
      goto decode_error;
    }
    else if (*data)
    {
      data ++;

      if (!*data)
        goto decode_error;
    }

    // Add the variable...
    num_vars = cupsAddOption(name, value, num_vars, vars);
  }

  return (num_vars);

  // If we get here there was an error in the form data...
  decode_error:

  cupsFreeOptions(num_vars, *vars);

  _cupsSetError(IPP_STATUS_ERROR_INTERNAL, _("Invalid form data."), 1);
  *vars = NULL;

  return (0);
}


//
// 'cupsFormEncode()' - Encode options as URL-encoded form data.
//
// This function encodes a CUPS options array as URL-encoded form data,
// returning an allocated string.
//
// Use `free` to return the memory used for the string.
//

char *					// O - URL-encoded form data
cupsFormEncode(size_t        num_vars,	// I - Number of variables
               cups_option_t *vars)	// I - Variables
{
  char	buffer[65536],			// Temporary buffer
	*bufptr = buffer,		// Current position in buffer
	*bufend = buffer + sizeof(buffer) - 1;
					// End of buffer


  // Loop through the variables...
  while (num_vars > 0)
  {
    // Encode the name
    bufptr = encode_string(vars->name, bufptr, bufend);

    if (bufptr >= bufend)
      goto encode_error;

    // 'name='
    *bufptr++ = '=';

    // Encode the value...
    bufptr = encode_string(vars->value, bufptr, bufend);

    if (bufptr >= bufend)
      goto encode_error;

    // Next variable
    num_vars --;
    vars ++;

    if (num_vars > 0)
    {
      // 'name=value&name2=value2...'
      *bufptr++ = '&';
    }
  }

  // Nul-terminate and return a copy...
  *bufptr = '\0';

  return (strdup(buffer));

  // Report encoding errors here...
  encode_error:

  _cupsSetError(IPP_STATUS_ERROR_INTERNAL, _("Form data too large."), 1);

  return (NULL);
}


//
// 'decode_string()' - Decode a URL-encoded string.
//

static const char *                     // O - New pointer into string or `NULL` on error
decode_string(const char *data,         // I - Pointer into data string
              char       term,          // I - Terminating character
              char       *buffer,       // I - String buffer
              size_t     bufsize)       // I - Size of string buffer
{
  int	ch;				// Current character
  char	*ptr,				// Pointer info buffer
	*end;				// Pointer to end of buffer


  for (ptr = buffer, end = buffer + bufsize - 1; *data && *data != term; data ++)
  {
    if ((ch = *data) == '+')
    {
      // "+" is an escaped space...
      ch = ' ';
    }
    else if (ch == '%')
    {
      // "%HH" is a hex-escaped character...
      if (isxdigit(data[1] & 255) && isxdigit(data[2] & 255))
      {
        data ++;
        if (isalpha(*data & 255))
          ch = (tolower(*data & 255) - 'a' + 10) << 4;
        else
          ch = (*data - '0') << 4;

        data ++;
        if (isalpha(*data & 255))
          ch += tolower(*data & 255) - 'a' + 10;
        else
          ch += *data - '0';
      }
      else
      {
        return (NULL);
      }
    }

    if (ch && ptr < end)
      *ptr++ = (char)ch;
  }

  *ptr = '\0';

  return (data);
}


//
// 'encode_string()' - URL-encode a string.
//
// The new buffer pointer can go past bufend, but we don't write past there...
//

static char *                           // O - New buffer pointer
encode_string(const char *s,            // I - String to encode
              char       *bufptr,       // I - Pointer into buffer
              char       *bufend)       // I - End of buffer
{
  static const char *hex = "0123456789ABCDEF";
                                        // Hex digits


  // Loop through the string and encode it as needed...
  while (*s)
  {
    if (*s == ' ')
    {
      // Space is encoded as '+'
      *bufptr++ = '+';
    }
    else if (*s == '\n')
    {
      // Newline is encoded as percent-encoded CR & LF
      *bufptr++ = '%';
      if (bufptr < bufend)
        *bufptr++ = '0';
      else
        bufptr ++;
      if (bufptr < bufend)
        *bufptr++ = 'D';
      else
        bufptr ++;
      if (bufptr < bufend)
        *bufptr++ = '%';
      else
        bufptr ++;
      if (bufptr < bufend)
        *bufptr++ = '0';
      else
        bufptr ++;
      if (bufptr < bufend)
        *bufptr++ = 'A';
      else
        bufptr ++;
    }
    else if (*s < ' ' || *s == '&' || *s == '%' || *s == '=' || *s == '+' || *s == '\"' || (*s & 128))
    {
      // Other control characters and special URL characters get percent-encoded
      *bufptr++ = '%';
      if (bufptr < bufend)
        *bufptr++ = hex[(*s >> 4) & 15];
      else
        bufptr ++;
      if (bufptr < bufend)
        *bufptr++ = hex[*s & 15];
      else
        bufptr ++;
    }
    else if (bufptr < bufend)
    {
      // Otherwise this character can be used literally...
      *bufptr++ = *s;
    }
    else
    {
      bufptr ++;
    }

    s ++;
  }

  // Nul-terminate the output string...
  if (bufptr <= bufend)
    *bufptr = '\0';
  else
    *bufend = '\0';

  return (bufptr);
}
