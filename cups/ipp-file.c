//
// IPP data file functions.
//
// Copyright © 2021-2022 by OpenPrinting.
// Copyright © 2007-2019 by Apple Inc.
// Copyright © 1997-2007 by Easy Software Products.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#include "cups.h"
#include "string-private.h"
#include "debug-internal.h"


//
// Private structures...
//

struct _ipp_file_s			// IPP data file
{
  cups_file_t		*fp;		// File pointer
  const char		*filename;	// Filename
  int			linenum;	// Current line number
  ipp_tag_t		group_tag;	// Current group for attributes
  ipp_t			*attrs;		// Current attributes
  size_t		num_vars;	// Number of variables
  cups_option_t		*vars;		// Variables
  ipp_fattr_cb_t	attr_cb;	// Attribute (filter) callback
  ipp_ferror_cb_t	error_cb;	// Error reporting callback
  ipp_ftoken_cb_t	token_cb;	// Token processing callback
  void			*cb_data;	// Callback data
  char			*buffer;	// Output buffer
  size_t		alloc_buffer;	// Size of output buffer
};


//
// Local functions...
//

static bool	expand_buffer(ipp_file_t *file, size_t buffer_size);
#if 0
static ipp_t	*parse_collection(_ipp_file_t *f, _ipp_vars_t *v, void *user_data);
static int	parse_value(_ipp_file_t *f, _ipp_vars_t *v, void *user_data, ipp_t *ipp, ipp_attribute_t **attr, size_t element);
static void	report_error(_ipp_file_t *f, _ipp_vars_t *v, void *user_data, const char *message, ...) _CUPS_FORMAT(4, 5);
#endif // 0


//
// 'ippFileClose()' - Close an IPP data file.
//
// This function closes an IPP data file and frees all memory associated with
// it.
//

bool					// O - `true` on success, `false` on error
ippFileClose(ipp_file_t *file)		// I - IPP data file
{
}


//
// 'ippFileGetAttributes()' - Get the current set of attributes from an IPP data file.
//
// This function gets the current set of attributes from an IPP data file and
// clears the internal attribute pointer.  The caller must call the
// @link ippDelete@ function to free the memory used by the attributes.
//

ipp_t *					// O - IPP attributes
ippFileGetAttributes(ipp_file_t *file)	// I - IPP data file
{
}


//
// 'ippFileGetFilename()' - .
//

const char *
ippFileGetFilename(ipp_file_t *file)
{
}


//
// 'ippFileGetLineNumber()' - .
//

int
ippFileGetLineNumber(ipp_file_t *file)
{
}


//
// 'ippFileGetVar()' - .
//

const char *
ippFileGetVar(ipp_file_t *file, const char *name)
{
}


//
// 'ippFileOpen()' - .
//

ipp_file_t *
ippFileOpen(const char *filename, const char *mode, ipp_file_t *parent, ipp_fattr_cb_t attr_cb, ipp_ferror_cb_t error_cb, void *cb_data)
{
}


//
// 'ippFileRead()' - .
//

bool					// O - `true` on success, `false` on error
ippFileRead(ipp_file_t *file, ipp_ftoken_cb_t token_cb)
{
}


//
// 'ippFileReadToken()' - .
//

bool					// O - `true` on success, `false` on error
ippFileReadToken(ipp_file_t *file, char *buffer, size_t bufsize)
{
}


//
// 'ippFileSetVar()' - Set an IPP data file variable to a constant value.
//
// This function sets an IPP data file variable to a constant value.  Setting
// the "uri" variable also initializes the "scheme", "uriuser", "hostname",
// "port", and "resource" variables.
//

void
ippFileSetVar(ipp_file_t *file,		// I - IPP data file
              const char *name,		// I - Variable name
              const char *value)	// I - Value
{
}


//
// 'ippFileSetVarf()' - Set an IPP data file variable to a formatted value.
//
// This function sets an IPP data file variable to a formatted value.  Setting
// the "uri" variable also initializes the "scheme", "uriuser", "hostname",
// "port", and "resource" variables.
//

void
ippFileSetVarf(ipp_file_t *file,	// I - IPP data file
               const char *name,	// I - Variable name
               const char *value,	// I - Printf-style value
               ...)			// I - Additional arguments as needed
{
}


//
// 'ippFileWriteAttributes()' - .
//

bool					// O - `true` on success, `false` on error
ippFileWriteAttributes(
    ipp_file_t *file,			// I - IPP data file
    ipp_t      *ipp)			// I - IPP attributes to write
{
}


//
// 'ippFileWriteComment()' - .
//

bool					// O - `true` on success, `false` on error
ippFileWriteComment(ipp_file_t *file,	// I - IPP data file
                    const char *comment,// I - Printf-style comment string
                    ...)		// I - Additional arguments as needed
{
}


//
// 'ippFileWriteToken()' - .
//

bool					// O - `true` on success, `false` on error
ippFileWriteToken(ipp_file_t *file,	// I - IPP data file
                  const char *token)	// I - Token/value string
{
}


//
// 'ippFileWriteTokenf()' - .
//

bool					// O - `true` on success, `false` on error
ippFileWriteTokenf(ipp_file_t *file,	// I - IPP data file
                   const char *token,	// I - Printf-style token/value string
                   ...)			// I - Additional arguments as needed
{
  va_list	ap;			// Pointer to arguments


  // Range check input...
  if (!file || !token)
    return (false);

  //
}


//
// 'expand_buffer()' - Expand the output buffer of the IPP data file as needed.
//

static bool				// O - `true` on success, `false` on failure
expand_buffer(ipp_file_t *file,		// I - IPP data file
              size_t     buffer_size)	// I - Required size
{
  char	*buffer;			// New buffer pointer


  // If we already have enough, return right away...
  if (buffer_size <= file->alloc_buffer)
    return (true);

  // Try allocating/expanding the current buffer...
  if ((buffer = realloc(file->buffer, buffer_size)) == NULL)
    return (false);

  // Save new buffer and size...
  file->buffer       = buffer;
  file->alloc_buffer = buffer_size;

  return (true);
}


#if 0
/*
 * '_ippFileParse()' - Parse an IPP data file.
 */

ipp_t *					/* O - IPP attributes or @code NULL@ on failure */
_ippFileParse(
    _ipp_vars_t      *v,		/* I - Variables */
    const char       *filename,		/* I - Name of file to parse */
    void             *user_data)	/* I - User data pointer */
{
  _ipp_file_t	f;			/* IPP data file information */
  ipp_t		*attrs = NULL;		/* Active IPP message */
  ipp_attribute_t *attr = NULL;		/* Current attribute */
  char		token[1024];		/* Token string */
  ipp_t		*ignored = NULL;	/* Ignored attributes */


  DEBUG_printf(("_ippFileParse(v=%p, filename=\"%s\", user_data=%p)", (void *)v, filename, user_data));

 /*
  * Initialize file info...
  */

  memset(&f, 0, sizeof(f));
  f.filename = filename;
  f.linenum  = 1;

  if ((f.fp = cupsFileOpen(filename, "r")) == NULL)
  {
    DEBUG_printf(("1_ippFileParse: Unable to open \"%s\": %s", filename, strerror(errno)));
    return (0);
  }

 /*
  * Do the callback with a NULL token to setup any initial state...
  */

  (*v->tokencb)(&f, v, user_data, NULL);

 /*
  * Read data file, using the callback function as needed...
  */

  while (_ippFileReadToken(&f, token, sizeof(token)))
  {
    if (!_cups_strcasecmp(token, "DEFINE") || !_cups_strcasecmp(token, "DEFINE-DEFAULT"))
    {
      char	name[128],		/* Variable name */
		value[1024],		/* Variable value */
		temp[1024];		/* Temporary string */

      attr = NULL;

      if (_ippFileReadToken(&f, name, sizeof(name)) && _ippFileReadToken(&f, temp, sizeof(temp)))
      {
        if (_cups_strcasecmp(token, "DEFINE-DEFAULT") || !_ippVarsGet(v, name))
        {
	  _ippVarsExpand(v, value, temp, sizeof(value));
	  _ippVarsSet(v, name, value);
	}
      }
      else
      {
        report_error(&f, v, user_data, "Missing %s name and/or value on line %d of \"%s\".", token, f.linenum, f.filename);
        break;
      }
    }
    else if (f.attrs && !_cups_strcasecmp(token, "ATTR"))
    {
     /*
      * Attribute definition...
      */

      char	syntax[128],		/* Attribute syntax (value tag) */
		name[128];		/* Attribute name */
      ipp_tag_t	value_tag;		/* Value tag */

      attr = NULL;

      if (!_ippFileReadToken(&f, syntax, sizeof(syntax)))
      {
        report_error(&f, v, user_data, "Missing ATTR syntax on line %d of \"%s\".", f.linenum, f.filename);
	break;
      }
      else if ((value_tag = ippTagValue(syntax)) < IPP_TAG_UNSUPPORTED_VALUE)
      {
        report_error(&f, v, user_data, "Bad ATTR syntax \"%s\" on line %d of \"%s\".", syntax, f.linenum, f.filename);
	break;
      }

      if (!_ippFileReadToken(&f, name, sizeof(name)) || !name[0])
      {
        report_error(&f, v, user_data, "Missing ATTR name on line %d of \"%s\".", f.linenum, f.filename);
	break;
      }

      if (!v->attrcb || (*v->attrcb)(&f, user_data, name))
      {
       /*
        * Add this attribute...
        */

        attrs = f.attrs;
      }
      else
      {
       /*
        * Ignore this attribute...
        */

        if (!ignored)
          ignored = ippNew();

        attrs = ignored;
      }

      if (value_tag < IPP_TAG_INTEGER)
      {
       /*
	* Add out-of-band attribute - no value string needed...
	*/

        ippAddOutOfBand(attrs, f.group_tag, value_tag, name);
      }
      else
      {
       /*
        * Add attribute with one or more values...
        */

        attr = ippAddString(attrs, f.group_tag, value_tag, name, NULL, NULL);

        if (!parse_value(&f, v, user_data, attrs, &attr, 0))
          break;
      }

    }
    else if (attr && !_cups_strcasecmp(token, ","))
    {
     /*
      * Additional value...
      */

      if (!parse_value(&f, v, user_data, attrs, &attr, ippGetCount(attr)))
	break;
    }
    else
    {
     /*
      * Something else...
      */

      attr  = NULL;
      attrs = NULL;

      if (!(*v->tokencb)(&f, v, user_data, token))
        break;
    }
  }

 /*
  * Close the file and free ignored attributes, then return any attributes we
  * kept...
  */

  cupsFileClose(f.fp);
  ippDelete(ignored);

  return (f.attrs);
}


/*
 * '_ippFileReadToken()' - Read a token from an IPP data file.
 */

int					/* O - 1 on success, 0 on failure */
_ippFileReadToken(_ipp_file_t *f,	/* I - File to read from */
                  char        *token,	/* I - Token string buffer */
                  size_t      tokensize)/* I - Size of token string buffer */
{
  int	ch,				/* Character from file */
	quote = 0;			/* Quoting character */
  char	*tokptr = token,		/* Pointer into token buffer */
	*tokend = token + tokensize - 1;/* End of token buffer */


 /*
  * Skip whitespace and comments...
  */

  DEBUG_printf(("1_ippFileReadToken: linenum=%d, pos=%ld", f->linenum, (long)cupsFileTell(f->fp)));

  while ((ch = cupsFileGetChar(f->fp)) != EOF)
  {
    if (_cups_isspace(ch))
    {
     /*
      * Whitespace...
      */

      if (ch == '\n')
      {
        f->linenum ++;
        DEBUG_printf(("1_ippFileReadToken: LF in leading whitespace, linenum=%d, pos=%ld", f->linenum, (long)cupsFileTell(f->fp)));
      }
    }
    else if (ch == '#')
    {
     /*
      * Comment...
      */

      DEBUG_puts("1_ippFileReadToken: Skipping comment in leading whitespace...");

      while ((ch = cupsFileGetChar(f->fp)) != EOF)
      {
        if (ch == '\n')
          break;
      }

      if (ch == '\n')
      {
        f->linenum ++;
        DEBUG_printf(("1_ippFileReadToken: LF at end of comment, linenum=%d, pos=%ld", f->linenum, (long)cupsFileTell(f->fp)));
      }
      else
        break;
    }
    else
      break;
  }

  if (ch == EOF)
  {
    DEBUG_puts("1_ippFileReadToken: EOF");
    return (0);
  }

 /*
  * Read a token...
  */

  while (ch != EOF)
  {
    if (ch == '\n')
    {
      f->linenum ++;
      DEBUG_printf(("1_ippFileReadToken: LF in token, linenum=%d, pos=%ld", f->linenum, (long)cupsFileTell(f->fp)));
    }

    if (ch == quote)
    {
     /*
      * End of quoted text...
      */

      *tokptr = '\0';
      DEBUG_printf(("1_ippFileReadToken: Returning \"%s\" at closing quote.", token));
      return (1);
    }
    else if (!quote && _cups_isspace(ch))
    {
     /*
      * End of unquoted text...
      */

      *tokptr = '\0';
      DEBUG_printf(("1_ippFileReadToken: Returning \"%s\" before whitespace.", token));
      return (1);
    }
    else if (!quote && (ch == '\'' || ch == '\"'))
    {
     /*
      * Start of quoted text or regular expression...
      */

      quote = ch;

      DEBUG_printf(("1_ippFileReadToken: Start of quoted string, quote=%c, pos=%ld", quote, (long)cupsFileTell(f->fp)));
    }
    else if (!quote && ch == '#')
    {
     /*
      * Start of comment...
      */

      cupsFileSeek(f->fp, cupsFileTell(f->fp) - 1);
      *tokptr = '\0';
      DEBUG_printf(("1_ippFileReadToken: Returning \"%s\" before comment.", token));
      return (1);
    }
    else if (!quote && (ch == '{' || ch == '}' || ch == ','))
    {
     /*
      * Delimiter...
      */

      if (tokptr > token)
      {
       /*
        * Return the preceding token first...
        */

	cupsFileSeek(f->fp, cupsFileTell(f->fp) - 1);
      }
      else
      {
       /*
        * Return this delimiter by itself...
        */

        *tokptr++ = (char)ch;
      }

      *tokptr = '\0';
      DEBUG_printf(("1_ippFileReadToken: Returning \"%s\".", token));
      return (1);
    }
    else
    {
      if (ch == '\\')
      {
       /*
        * Quoted character...
        */

        DEBUG_printf(("1_ippFileReadToken: Quoted character at pos=%ld", (long)cupsFileTell(f->fp)));

        if ((ch = cupsFileGetChar(f->fp)) == EOF)
        {
	  *token = '\0';
	  DEBUG_puts("1_ippFileReadToken: EOF");
	  return (0);
	}
	else if (ch == '\n')
	{
	  f->linenum ++;
	  DEBUG_printf(("1_ippFileReadToken: quoted LF, linenum=%d, pos=%ld", f->linenum, (long)cupsFileTell(f->fp)));
	}
	else if (ch == 'a')
	  ch = '\a';
	else if (ch == 'b')
	  ch = '\b';
	else if (ch == 'f')
	  ch = '\f';
	else if (ch == 'n')
	  ch = '\n';
	else if (ch == 'r')
	  ch = '\r';
	else if (ch == 't')
	  ch = '\t';
	else if (ch == 'v')
	  ch = '\v';
      }

      if (tokptr < tokend)
      {
       /*
	* Add to current token...
	*/

	*tokptr++ = (char)ch;
      }
      else
      {
       /*
	* Token too long...
	*/

	*tokptr = '\0';
	DEBUG_printf(("1_ippFileReadToken: Too long: \"%s\".", token));
	return (0);
      }
    }

   /*
    * Get the next character...
    */

    ch = cupsFileGetChar(f->fp);
  }

  *tokptr = '\0';
  DEBUG_printf(("1_ippFileReadToken: Returning \"%s\" at EOF.", token));

  return (tokptr > token);
}


/*
 * 'parse_collection()' - Parse an IPP collection value.
 */

static ipp_t *				/* O - Collection value or @code NULL@ on error */
parse_collection(
    _ipp_file_t      *f,		/* I - IPP data file */
    _ipp_vars_t      *v,		/* I - IPP variables */
    void             *user_data)	/* I - User data pointer */
{
  ipp_t		*col = ippNew();	/* Collection value */
  ipp_attribute_t *attr = NULL;		/* Current member attribute */
  char		token[1024];		/* Token string */


 /*
  * Parse the collection value...
  */

  while (_ippFileReadToken(f, token, sizeof(token)))
  {
    if (!_cups_strcasecmp(token, "}"))
    {
     /*
      * End of collection value...
      */

      break;
    }
    else if (!_cups_strcasecmp(token, "MEMBER"))
    {
     /*
      * Member attribute definition...
      */

      char	syntax[128],		/* Attribute syntax (value tag) */
		name[128];		/* Attribute name */
      ipp_tag_t	value_tag;		/* Value tag */

      attr = NULL;

      if (!_ippFileReadToken(f, syntax, sizeof(syntax)))
      {
        report_error(f, v, user_data, "Missing MEMBER syntax on line %d of \"%s\".", f->linenum, f->filename);
	ippDelete(col);
	col = NULL;
	break;
      }
      else if ((value_tag = ippTagValue(syntax)) < IPP_TAG_UNSUPPORTED_VALUE)
      {
        report_error(f, v, user_data, "Bad MEMBER syntax \"%s\" on line %d of \"%s\".", syntax, f->linenum, f->filename);
	ippDelete(col);
	col = NULL;
	break;
      }

      if (!_ippFileReadToken(f, name, sizeof(name)) || !name[0])
      {
        report_error(f, v, user_data, "Missing MEMBER name on line %d of \"%s\".", f->linenum, f->filename);
	ippDelete(col);
	col = NULL;
	break;
      }

      if (value_tag < IPP_TAG_INTEGER)
      {
       /*
	* Add out-of-band attribute - no value string needed...
	*/

        ippAddOutOfBand(col, IPP_TAG_ZERO, value_tag, name);
      }
      else
      {
       /*
        * Add attribute with one or more values...
        */

        attr = ippAddString(col, IPP_TAG_ZERO, value_tag, name, NULL, NULL);

        if (!parse_value(f, v, user_data, col, &attr, 0))
        {
	  ippDelete(col);
	  col = NULL;
          break;
	}
      }

    }
    else if (attr && !_cups_strcasecmp(token, ","))
    {
     /*
      * Additional value...
      */

      if (!parse_value(f, v, user_data, col, &attr, ippGetCount(attr)))
      {
	ippDelete(col);
	col = NULL;
	break;
      }
    }
    else
    {
     /*
      * Something else...
      */

      report_error(f, v, user_data, "Unknown directive \"%s\" on line %d of \"%s\".", token, f->linenum, f->filename);
      ippDelete(col);
      col  = NULL;
      attr = NULL;
      break;

    }
  }

  return (col);
}


/*
 * 'parse_value()' - Parse an IPP value.
 */

static int				/* O  - 1 on success or 0 on error */
parse_value(_ipp_file_t      *f,	/* I  - IPP data file */
            _ipp_vars_t      *v,	/* I  - IPP variables */
            void             *user_data,/* I  - User data pointer */
            ipp_t            *ipp,	/* I  - IPP message */
            ipp_attribute_t  **attr,	/* IO - IPP attribute */
            size_t           element)	/* I  - Element number */
{
  char		value[2049],		/* Value string */
		*valueptr,		/* Pointer into value string */
		temp[2049],		/* Temporary string */
		*tempptr;		/* Pointer into temporary string */
  size_t	valuelen;		/* Length of value */


  if (!_ippFileReadToken(f, temp, sizeof(temp)))
  {
    report_error(f, v, user_data, "Missing value on line %d of \"%s\".", f->linenum, f->filename);
    return (0);
  }

  _ippVarsExpand(v, value, temp, sizeof(value));

  switch (ippGetValueTag(*attr))
  {
    case IPP_TAG_BOOLEAN :
        return (ippSetBoolean(ipp, attr, element, !_cups_strcasecmp(value, "true")));

    case IPP_TAG_ENUM :
    case IPP_TAG_INTEGER :
        return (ippSetInteger(ipp, attr, element, (int)strtol(value, NULL, 0)));

    case IPP_TAG_DATE :
        {
          int	year,			/* Year */
		month,			/* Month */
		day,			/* Day of month */
		hour,			/* Hour */
		minute,			/* Minute */
		second,			/* Second */
		utc_offset = 0;		/* Timezone offset from UTC */
          ipp_uchar_t date[11];		/* dateTime value */

          if (*value == 'P')
          {
           /*
            * Time period...
            */

            time_t	curtime;	/* Current time in seconds */
            int		period = 0,	/* Current period value */
			saw_T = 0;	/* Saw time separator */

            curtime = time(NULL);

            for (valueptr = value + 1; *valueptr; valueptr ++)
            {
              if (isdigit(*valueptr & 255))
              {
                period = (int)strtol(valueptr, &valueptr, 10);

                if (!valueptr || period < 0)
                {
		  report_error(f, v, user_data, "Bad dateTime value \"%s\" on line %d of \"%s\".", value, f->linenum, f->filename);
		  return (0);
		}
              }

              if (*valueptr == 'Y')
              {
                curtime += 365 * 86400 * period;
                period  = 0;
              }
              else if (*valueptr == 'M')
              {
                if (saw_T)
                  curtime += 60 * period;
                else
                  curtime += 30 * 86400 * period;

                period = 0;
              }
              else if (*valueptr == 'D')
              {
                curtime += 86400 * period;
                period  = 0;
              }
              else if (*valueptr == 'H')
              {
                curtime += 3600 * period;
                period  = 0;
              }
              else if (*valueptr == 'S')
              {
                curtime += period;
                period = 0;
              }
              else if (*valueptr == 'T')
              {
                saw_T  = 1;
                period = 0;
              }
              else
	      {
		report_error(f, v, user_data, "Bad dateTime value \"%s\" on line %d of \"%s\".", value, f->linenum, f->filename);
		return (0);
	      }
	    }

	    return (ippSetDate(ipp, attr, element, ippTimeToDate(curtime)));
          }
          else if (sscanf(value, "%d-%d-%dT%d:%d:%d%d", &year, &month, &day, &hour, &minute, &second, &utc_offset) < 6)
          {
           /*
            * Date/time value did not parse...
            */

	    report_error(f, v, user_data, "Bad dateTime value \"%s\" on line %d of \"%s\".", value, f->linenum, f->filename);
	    return (0);
          }

          date[0] = (ipp_uchar_t)(year >> 8);
          date[1] = (ipp_uchar_t)(year & 255);
          date[2] = (ipp_uchar_t)month;
          date[3] = (ipp_uchar_t)day;
          date[4] = (ipp_uchar_t)hour;
          date[5] = (ipp_uchar_t)minute;
          date[6] = (ipp_uchar_t)second;
          date[7] = 0;
          if (utc_offset < 0)
          {
            utc_offset = -utc_offset;
            date[8]    = (ipp_uchar_t)'-';
	  }
	  else
	  {
            date[8] = (ipp_uchar_t)'+';
	  }

          date[9]  = (ipp_uchar_t)(utc_offset / 100);
          date[10] = (ipp_uchar_t)(utc_offset % 100);

          return (ippSetDate(ipp, attr, element, date));
        }

    case IPP_TAG_RESOLUTION :
	{
	  int	xres,		/* X resolution */
		yres;		/* Y resolution */
	  char	*ptr;		/* Pointer into value */

	  xres = yres = (int)strtol(value, (char **)&ptr, 10);
	  if (ptr > value && xres > 0)
	  {
	    if (*ptr == 'x')
	      yres = (int)strtol(ptr + 1, (char **)&ptr, 10);
	  }

	  if (ptr <= value || xres <= 0 || yres <= 0 || !ptr || (_cups_strcasecmp(ptr, "dpi") && _cups_strcasecmp(ptr, "dpc") && _cups_strcasecmp(ptr, "dpcm") && _cups_strcasecmp(ptr, "other")))
	  {
	    report_error(f, v, user_data, "Bad resolution value \"%s\" on line %d of \"%s\".", value, f->linenum, f->filename);
	    return (0);
	  }

	  if (!_cups_strcasecmp(ptr, "dpi"))
	    return (ippSetResolution(ipp, attr, element, IPP_RES_PER_INCH, xres, yres));
	  else if (!_cups_strcasecmp(ptr, "dpc") || !_cups_strcasecmp(ptr, "dpcm"))
	    return (ippSetResolution(ipp, attr, element, IPP_RES_PER_CM, xres, yres));
	  else
	    return (ippSetResolution(ipp, attr, element, (ipp_res_t)0, xres, yres));
	}

    case IPP_TAG_RANGE :
	{
	  int	lower,			/* Lower value */
		upper;			/* Upper value */

          if (sscanf(value, "%d-%d", &lower, &upper) != 2)
          {
	    report_error(f, v, user_data, "Bad rangeOfInteger value \"%s\" on line %d of \"%s\".", value, f->linenum, f->filename);
	    return (0);
	  }

	  return (ippSetRange(ipp, attr, element, lower, upper));
	}

    case IPP_TAG_STRING :
        valuelen = strlen(value);

        if (value[0] == '<' && value[strlen(value) - 1] == '>')
        {
          if (valuelen & 1)
          {
	    report_error(f, v, user_data, "Bad octetString value on line %d of \"%s\".", f->linenum, f->filename);
	    return (0);
          }

          valueptr = value + 1;
          tempptr  = temp;

          while (*valueptr && *valueptr != '>')
          {
	    if (!isxdigit(valueptr[0] & 255) || !isxdigit(valueptr[1] & 255))
	    {
	      report_error(f, v, user_data, "Bad octetString value on line %d of \"%s\".", f->linenum, f->filename);
	      return (0);
	    }

            if (valueptr[0] >= '0' && valueptr[0] <= '9')
              *tempptr = (char)((valueptr[0] - '0') << 4);
	    else
              *tempptr = (char)((tolower(valueptr[0]) - 'a' + 10) << 4);

            if (valueptr[1] >= '0' && valueptr[1] <= '9')
              *tempptr |= (valueptr[1] - '0');
	    else
              *tempptr |= (tolower(valueptr[1]) - 'a' + 10);

            tempptr ++;
          }

          return (ippSetOctetString(ipp, attr, element, temp, (size_t)(tempptr - temp)));
        }
        else
          return (ippSetOctetString(ipp, attr, element, value, valuelen));

    case IPP_TAG_TEXTLANG :
    case IPP_TAG_NAMELANG :
    case IPP_TAG_TEXT :
    case IPP_TAG_NAME :
    case IPP_TAG_KEYWORD :
    case IPP_TAG_URI :
    case IPP_TAG_URISCHEME :
    case IPP_TAG_CHARSET :
    case IPP_TAG_LANGUAGE :
    case IPP_TAG_MIMETYPE :
        return (ippSetString(ipp, attr, element, value));

    case IPP_TAG_BEGIN_COLLECTION :
        {
          int	status;			/* Add status */
          ipp_t *col;			/* Collection value */

          if (strcmp(value, "{"))
          {
	    report_error(f, v, user_data, "Bad collection value on line %d of \"%s\".", f->linenum, f->filename);
	    return (0);
          }

          if ((col = parse_collection(f, v, user_data)) == NULL)
            return (0);

	  status = ippSetCollection(ipp, attr, element, col);
	  ippDelete(col);

	  return (status);
	}

    default :
        report_error(f, v, user_data, "Unsupported value on line %d of \"%s\".", f->linenum, f->filename);
        return (0);
  }
}


/*
 * 'report_error()' - Report an error.
 */

static void
report_error(
    _ipp_file_t *f,			/* I - IPP data file */
    _ipp_vars_t *v,			/* I - Error callback function, if any */
    void        *user_data,		/* I - User data pointer */
    const char  *message,		/* I - Printf-style message */
    ...)				/* I - Additional arguments as needed */
{
  char		buffer[8192];		/* Formatted string */
  va_list	ap;			/* Argument pointer */


  va_start(ap, message);
  vsnprintf(buffer, sizeof(buffer), message, ap);
  va_end(ap);

  if (v->errorcb)
    (*v->errorcb)(f, user_data, buffer);
  else
    fprintf(stderr, "%s\n", buffer);
}
/*
 * IPP data file parsing functions.
 *
 * Copyright © 2022 by OpenPrinting.
 * Copyright © 2007-2019 by Apple Inc.
 * Copyright © 1997-2007 by Easy Software Products.
 *
 * Licensed under Apache License v2.0.  See the file "LICENSE" for more
 * information.
 */

/*
 * Include necessary headers...
 */

#include "cups-private.h"
#include "ipp-private.h"
#include "string-private.h"
#include "debug-internal.h"


/*
 * '_ippVarsDeinit()' - Free all memory associated with the IPP variables.
 */

void
_ippVarsDeinit(_ipp_vars_t *v)		/* I - IPP variables */
{
  if (v->uri)
  {
    free(v->uri);
    v->uri = NULL;
  }

  cupsFreeOptions(v->num_vars, v->vars);
  v->num_vars = 0;
  v->vars     = NULL;
}


/*
 * '_ippVarsExpand()' - Expand variables in the source string.
 */

void
_ippVarsExpand(_ipp_vars_t *v,		/* I - IPP variables */
               char        *dst,	/* I - Destination buffer */
               const char  *src,	/* I - Source string */
               size_t      dstsize)	/* I - Destination buffer size */
{
  char		*dstptr,		/* Pointer into destination */
		*dstend,		/* End of destination */
		temp[256],		/* Temporary string */
		*tempptr;		/* Pointer into temporary string */
  const char	*value;			/* Value to substitute */


  dstptr = dst;
  dstend = dst + dstsize - 1;

  while (*src && dstptr < dstend)
  {
    if (*src == '$')
    {
     /*
      * Substitute a string/number...
      */

      if (!strncmp(src, "$$", 2))
      {
        value = "$";
	src   += 2;
      }
      else if (!strncmp(src, "$ENV[", 5))
      {
	cupsCopyString(temp, src + 5, sizeof(temp));

	for (tempptr = temp; *tempptr; tempptr ++)
	  if (*tempptr == ']')
	    break;

        if (*tempptr)
	  *tempptr++ = '\0';

	value = getenv(temp);
        src   += tempptr - temp + 5;
      }
      else
      {
        if (src[1] == '{')
	{
	  src += 2;
	  cupsCopyString(temp, src, sizeof(temp));
	  if ((tempptr = strchr(temp, '}')) != NULL)
	    *tempptr = '\0';
	  else
	    tempptr = temp + strlen(temp);
	}
	else
	{
	  cupsCopyString(temp, src + 1, sizeof(temp));

	  for (tempptr = temp; *tempptr; tempptr ++)
	    if (!isalnum(*tempptr & 255) && *tempptr != '-' && *tempptr != '_')
	      break;

	  if (*tempptr)
	    *tempptr = '\0';
        }

        value = _ippVarsGet(v, temp);

        src += tempptr - temp + 1;
      }

      if (value)
      {
        cupsCopyString(dstptr, value, (size_t)(dstend - dstptr + 1));
	dstptr += strlen(dstptr);
      }
    }
    else
      *dstptr++ = *src++;
  }

  *dstptr = '\0';
}


/*
 * '_ippVarsGet()' - Get a variable string.
 */

const char *				/* O - Value or @code NULL@ if not set */
_ippVarsGet(_ipp_vars_t *v,		/* I - IPP variables */
            const char  *name)		/* I - Variable name */
{
  if (!v)
    return (NULL);
  else if (!strcmp(name, "uri"))
    return (v->uri);
  else if (!strcmp(name, "uriuser") || !strcmp(name, "username"))
    return (v->username[0] ? v->username : NULL);
  else if (!strcmp(name, "scheme") || !strcmp(name, "method"))
    return (v->scheme);
  else if (!strcmp(name, "hostname"))
    return (v->host);
  else if (!strcmp(name, "port"))
    return (v->portstr);
  else if (!strcmp(name, "resource"))
    return (v->resource);
  else if (!strcmp(name, "user"))
    return (cupsGetUser());
  else
    return (cupsGetOption(name, v->num_vars, v->vars));
}


/*
 * '_ippVarsInit()' - Initialize .
 */

void
_ippVarsInit(_ipp_vars_t      *v,	/* I - IPP variables */
             _ipp_fattr_cb_t  attrcb,	/* I - Attribute (filter) callback */
             _ipp_ferror_cb_t errorcb,	/* I - Error callback */
             _ipp_ftoken_cb_t tokencb)	/* I - Token callback */
{
  memset(v, 0, sizeof(_ipp_vars_t));

  v->attrcb  = attrcb;
  v->errorcb = errorcb;
  v->tokencb = tokencb;
}


/*
 * '_ippVarsPasswordCB()' - Password callback using the IPP variables.
 */

const char *				/* O - Password string or @code NULL@ */
_ippVarsPasswordCB(
    const char *prompt,			/* I - Prompt string (not used) */
    http_t     *http,			/* I - HTTP connection (not used) */
    const char *method,			/* I - HTTP method (not used) */
    const char *resource,		/* I - Resource path (not used) */
    void       *user_data)		/* I - IPP variables */
{
  _ipp_vars_t	*v = (_ipp_vars_t *)user_data;
					/* I - IPP variables */


  (void)prompt;
  (void)http;
  (void)method;
  (void)resource;

  if (v->username[0] && v->password && v->password_tries < 3)
  {
    v->password_tries ++;

    cupsSetUser(v->username);

    return (v->password);
  }
  else
  {
    return (NULL);
  }
}


/*
 * '_ippVarsSet()' - Set an IPP variable.
 */

int					/* O - 1 on success, 0 on failure */
_ippVarsSet(_ipp_vars_t *v,		/* I - IPP variables */
            const char  *name,		/* I - Variable name */
            const char  *value)		/* I - Variable value */
{
  if (!strcmp(name, "uri"))
  {
    char	uri[1024];		/* New printer URI */
    char	resolved[1024];		/* Resolved mDNS URI */

    if (strstr(value, "._tcp"))
    {
     /*
      * Resolve URI...
      */

      if (!_httpResolveURI(value, resolved, sizeof(resolved), _HTTP_RESOLVE_DEFAULT, NULL, NULL))
        return (0);

      value = resolved;
    }

    if (httpSeparateURI(HTTP_URI_CODING_ALL, value, v->scheme, sizeof(v->scheme), v->username, sizeof(v->username), v->host, sizeof(v->host), &(v->port), v->resource, sizeof(v->resource)) < HTTP_URI_STATUS_OK)
      return (0);

    if (v->username[0])
    {
      if ((v->password = strchr(v->username, ':')) != NULL)
	*(v->password)++ = '\0';
    }

    snprintf(v->portstr, sizeof(v->portstr), "%d", v->port);

    if (v->uri)
      free(v->uri);

    httpAssembleURI(HTTP_URI_CODING_ALL, uri, sizeof(uri), v->scheme, NULL, v->host, v->port, v->resource);
    v->uri = strdup(uri);

    return (v->uri != NULL);
  }
  else
  {
    v->num_vars = cupsAddOption(name, value, v->num_vars, &v->vars);
    return (1);
  }
}
#endif // 0
