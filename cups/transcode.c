//
// Transcoding support for CUPS.
//
// Copyright © 2022 by OpenPrinting.
// Copyright © 2007-2014 by Apple Inc.
// Copyright © 1997-2007 by Easy Software Products.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#include "cups.h"
#include "thread.h"
#include "transcode.h"
#include "debug-internal.h"
#include "string-private.h"
#include <limits.h>
#include <time.h>
#ifdef HAVE_ICONV_H
#  include <iconv.h>
#endif // HAVE_ICONV_H


//
// Local globals...
//

static const char * const map_encodings[] =
{					// Encoding strings
  "us-ascii",		"iso-8859-1",
  "iso-8859-2",		"iso-8859-3",
  "iso-8859-4",		"iso-8859-5",
  "iso-8859-6",		"iso-8859-7",
  "iso-8859-8",		"iso-8859-9",
  "iso-8859-10",	"utf-8",
  "iso-8859-13",	"iso-8859-14",
  "iso-8859-15",	"cp874",
  "cp1250",		"cp1251",
  "cp1252",		"cp1253",
  "cp1254",		"cp1255",
  "cp1256",		"cp1257",
  "cp1258",		"koi8-r",
  "koi8-u",		"iso-8859-11",
  "iso-8859-16",	"mac",
  "unknown",		"unknown",
  "unknown",		"unknown",
  "unknown",		"unknown",
  "unknown",		"unknown",
  "unknown",		"unknown",
  "unknown",		"unknown",
  "unknown",		"unknown",
  "unknown",		"unknown",
  "unknown",		"unknown",
  "unknown",		"unknown",
  "unknown",		"unknown",
  "unknown",		"unknown",
  "unknown",		"unknown",
  "unknown",		"unknown",
  "unknown",		"unknown",
  "unknown",		"unknown",
  "unknown",		"unknown",
  "cp932",		"cp936",
  "cp949",		"cp950",
  "cp1361",		"bg18030",
  "unknown",		"unknown",
  "unknown",		"unknown",
  "unknown",		"unknown",
  "unknown",		"unknown",
  "unknown",		"unknown",
  "unknown",		"unknown",
  "unknown",		"unknown",
  "unknown",		"unknown",
  "unknown",		"unknown",
  "unknown",		"unknown",
  "unknown",		"unknown",
  "unknown",		"unknown",
  "unknown",		"unknown",
  "unknown",		"unknown",
  "unknown",		"unknown",
  "unknown",		"unknown",
  "unknown",		"unknown",
  "unknown",		"unknown",
  "unknown",		"unknown",
  "unknown",		"unknown",
  "unknown",		"unknown",
  "unknown",		"unknown",
  "unknown",		"unknown",
  "unknown",		"unknown",
  "unknown",		"unknown",
  "unknown",		"unknown",
  "unknown",		"unknown",
  "unknown",		"unknown",
  "unknown",		"unknown",
  "euc-cn",		"euc-jp",
  "euc-kr",		"euc-tw",
  "shift_jisx0213"
};
#ifdef HAVE_ICONV_H
static cups_mutex_t	map_mutex = CUPS_MUTEX_INITIALIZER;
					// Mutex to control access to maps
static iconv_t		map_from_utf8 = (iconv_t)-1;
					// Convert from UTF-8 to charset
static iconv_t		map_to_utf8 = (iconv_t)-1;
					// Convert from charset to UTF-8
static cups_encoding_t	map_encoding = CUPS_ENCODING_AUTO;
					// Which charset is cached
#endif // HAVE_ICONV_H


//
// Local functions...
//

static void		flush_map(void);


//
// 'cupsCharsetToUTF8()' - Convert legacy character set to UTF-8.
//

ssize_t					// O - Number of bytes or `-1` on error
cupsCharsetToUTF8(
    char                  *dest,	// O - Target string
    const char            *src,		// I - Source string
    const size_t          maxout,	// I - Max output size in bytes
    const cups_encoding_t encoding)	// I - Encoding
{
  char		*destptr;		// Pointer into UTF-8 buffer
#ifdef HAVE_ICONV_H
  size_t	srclen,			// Length of source string
		outBytesLeft;		// Bytes remaining in output buffer
#endif // HAVE_ICONV_H


  // Check for valid arguments...
  DEBUG_printf(("2cupsCharsetToUTF8(dest=%p, src=\"%s\", maxout=%u, encoding=%d)", (void *)dest, src, (unsigned)maxout, encoding));

  if (!dest || !src || maxout < 1)
  {
    if (dest)
      *dest = '\0';

    DEBUG_puts("3cupsCharsetToUTF8: Bad arguments, returning -1");
    return (-1);
  }

  // Handle identity conversions...
  if (encoding == CUPS_ENCODING_UTF_8 || encoding <= CUPS_ENCODING_US_ASCII || encoding >= CUPS_ENCODING_VBCS_END)
  {
    cupsCopyString((char *)dest, src, maxout);
    return ((ssize_t)strlen((char *)dest));
  }

  // Handle ISO-8859-1 to UTF-8 directly...
  destptr = dest;

  if (encoding == CUPS_ENCODING_ISO8859_1)
  {
    int		ch;			// Character from string
    char	*destend;		// End of UTF-8 buffer


    destend = dest + maxout - 2;

    while (*src && destptr < destend)
    {
      ch = *src++ & 255;

      if (ch & 128)
      {
	*destptr++ = (char)(0xc0 | (ch >> 6));
	*destptr++ = (char)(0x80 | (ch & 0x3f));
      }
      else
	*destptr++ = (char)ch;
    }

    *destptr = '\0';

    return ((ssize_t)(destptr - dest));
  }

  // Convert input legacy charset to UTF-8...
#ifdef HAVE_ICONV_H
  cupsMutexLock(&map_mutex);

  if (map_encoding != encoding)
  {
    char	toset[1024];		// Destination character set

    flush_map();

    snprintf(toset, sizeof(toset), "%s//IGNORE", cupsEncodingString(encoding));

    map_encoding  = encoding;
    map_from_utf8 = iconv_open(cupsEncodingString(encoding), "UTF-8");
    map_to_utf8   = iconv_open("UTF-8", toset);
  }

  if (map_to_utf8 != (iconv_t)-1)
  {
    char *altdestptr = (char *)dest;	// Silence bogus GCC type-punned

    srclen       = strlen(src);
    outBytesLeft = maxout - 1;

    iconv(map_to_utf8, (char **)&src, &srclen, &altdestptr, &outBytesLeft);
    *altdestptr = '\0';

    cupsMutexUnlock(&map_mutex);

    return ((ssize_t)(altdestptr - (char *)dest));
  }

  cupsMutexUnlock(&map_mutex);
#endif // HAVE_ICONV_H

  // No iconv() support, so error out...
  *destptr = '\0';

  return (-1);
}


//
// 'cupsEncodingString()' - Return the character encoding name string for the
//                          given encoding enumeration.
//

const char *				// O - Character encoding string
cupsEncodingString(
    cups_encoding_t value)		// I - Encoding value
{
  if (value < CUPS_ENCODING_US_ASCII || value >= (cups_encoding_t)(sizeof(map_encodings) / sizeof(map_encodings[0])))
  {
    DEBUG_printf(("1cupsEncodingString(encoding=%d) = out of range (\"%s\")", value, map_encodings[0]));
    return (map_encodings[0]);
  }
  else
  {
    DEBUG_printf(("1cupsEncodingString(encoding=%d)=\"%s\"", value, map_encodings[value]));
    return (map_encodings[value]);
  }
}


//
// 'cupsEncodingValue()' - Return the encoding enumeration value for a given
//                         character encoding name string.
//

cups_encoding_t				// O - Encoding value
cupsEncodingValue(const char *s)	// I - Character encoding string
{
  if (s)
  {
    size_t	i;			// Looping var

    for (i = 0; i < (sizeof(map_encodings) / sizeof(map_encodings[0])); i ++)
    {
      if (!_cups_strcasecmp(s, map_encodings[i]))
        return ((cups_encoding_t)i);
    }
  }

  return (CUPS_ENCODING_US_ASCII);
}


//
// 'cupsUTF8ToCharset()' - Convert UTF-8 to legacy character set.
//

ssize_t					// O - Number of bytes or `-1` on error
cupsUTF8ToCharset(
    char                  *dest,	// O - Target string
    const char	          *src,		// I - Source string
    const size_t          maxout,	// I - Max output in bytes
    const cups_encoding_t encoding)	// I - Encoding
{
  char		*destptr;		// Pointer into destination
#ifdef HAVE_ICONV_H
  size_t	srclen,			// Length of source string
		outBytesLeft;		// Bytes remaining in output buffer
#endif // HAVE_ICONV_H


  // Check for valid arguments...
  if (!dest || !src || maxout < 1)
  {
    if (dest)
      *dest = '\0';

    return (-1);
  }

  // Handle identity conversions...
  if (encoding == CUPS_ENCODING_UTF_8 || encoding >= CUPS_ENCODING_VBCS_END)
  {
    cupsCopyString(dest, (char *)src, maxout);
    return ((ssize_t)strlen(dest));
  }

 /*
  * Handle UTF-8 to ISO-8859-1 directly...
  */

  destptr = dest;

  if (encoding == CUPS_ENCODING_ISO8859_1 || encoding <= CUPS_ENCODING_US_ASCII)
  {
    int		ch,			// Character from string
		maxch;			// Maximum character for charset
    char	*destend;		// End of ISO-8859-1 buffer

    maxch   = encoding == CUPS_ENCODING_ISO8859_1 ? 256 : 128;
    destend = dest + maxout - 1;

    while (*src && destptr < destend)
    {
      ch = *src++;

      if ((ch & 0xe0) == 0xc0)
      {
	ch = ((ch & 0x1f) << 6) | (*src++ & 0x3f);

	if (ch < maxch)
          *destptr++ = (char)ch;
	else
          *destptr++ = '?';
      }
      else if ((ch & 0xf0) == 0xe0 || (ch & 0xf8) == 0xf0)
      {
        *destptr++ = '?';
      }
      else if (!(ch & 0x80))
      {
	*destptr++ = (char)ch;
      }
    }

    *destptr = '\0';

    return ((ssize_t)(destptr - dest));
  }

#ifdef HAVE_ICONV_H
  // Convert input UTF-8 to legacy charset...
  cupsMutexLock(&map_mutex);

  if (map_encoding != encoding)
  {
    char	toset[1024];		// Destination character set

    flush_map();

    snprintf(toset, sizeof(toset), "%s//IGNORE", cupsEncodingString(encoding));

    map_encoding  = encoding;
    map_from_utf8 = iconv_open(cupsEncodingString(encoding), "UTF-8");
    map_to_utf8   = iconv_open("UTF-8", toset);
  }

  if (map_from_utf8 != (iconv_t)-1)
  {
    char *altsrc = (char *)src;		// Silence bogus GCC type-punned

    srclen       = strlen((char *)src);
    outBytesLeft = maxout - 1;

    iconv(map_from_utf8, &altsrc, &srclen, &destptr, &outBytesLeft);
    *destptr = '\0';

    cupsMutexUnlock(&map_mutex);

    return ((ssize_t)(destptr - dest));
  }

  cupsMutexUnlock(&map_mutex);
#endif // HAVE_ICONV_H

  // No iconv() support, so error out...
  *destptr = '\0';

  return (-1);
}


//
// 'cupsUTF8ToUTF32()' - Convert UTF-8 to UTF-32.
//
// This function converts a UTF-8 (8-bit encoding of Unicode) `nul`-terminated
// C string to a UTF-32 (32-bit encoding of Unicode) string.
//

ssize_t					// O - Number of words or `-1` on error
cupsUTF8ToUTF32(
    cups_utf32_t *dest,			// O - Target string
    const char   *src,			// I - Source string
    const size_t maxout)		// I - Max output in words
{
  size_t	i;			// Looping variable
  int		ch,			// Character value
		next;			// Next character value
  cups_utf32_t	ch32;			// UTF-32 character value


  // Check for valid arguments and clear output...
  DEBUG_printf(("2cupsUTF8ToUTF32(dest=%p, src=\"%s\", maxout=%u)", (void *)dest, src, (unsigned)maxout));

  if (dest)
    *dest = 0;

  if (!dest || !src || maxout < 1 || maxout > CUPS_MAX_USTRING)
  {
    DEBUG_puts("3cupsUTF8ToUTF32: Returning -1 (bad arguments)");

    return (-1);
  }

  // Convert input UTF-8 to output UTF-32...
  for (i = maxout - 1; *src && i > 0; i --)
  {
    ch = *src++;

    // Convert UTF-8 character(s) to UTF-32 character...
    if (!(ch & 0x80))
    {
      // One-octet UTF-8 <= 127 (US-ASCII)...
      *dest++ = (cups_utf32_t)ch;

      DEBUG_printf(("4cupsUTF8ToUTF32: %02x => %08X", src[-1], ch));
      continue;
    }
    else if ((ch & 0xe0) == 0xc0)
    {
      // Two-octet UTF-8 <= 2047 (Latin-x)...
      next = *src++;
      if ((next & 0xc0) != 0x80)
      {
        DEBUG_puts("3cupsUTF8ToUTF32: Returning -1 (bad UTF-8 sequence)");

	return (-1);
      }

      ch32 = (cups_utf32_t)((ch & 0x1f) << 6) | (cups_utf32_t)(next & 0x3f);

      // Check for non-shortest form (invalid UTF-8)...
      if (ch32 < 0x80)
      {
        DEBUG_puts("3cupsUTF8ToUTF32: Returning -1 (bad UTF-8 sequence)");

	return (-1);
      }

      *dest++ = ch32;

      DEBUG_printf(("4cupsUTF8ToUTF32: %02x %02x => %08X", src[-2], src[-1], (unsigned)ch32));
    }
    else if ((ch & 0xf0) == 0xe0)
    {
      // Three-octet UTF-8 <= 65535 (Plane 0 - BMP)...
      next = *src++;
      if ((next & 0xc0) != 0x80)
      {
        DEBUG_puts("3cupsUTF8ToUTF32: Returning -1 (bad UTF-8 sequence)");

	return (-1);
      }

      ch32 = (cups_utf32_t)((ch & 0x0f) << 6) | (cups_utf32_t)(next & 0x3f);

      next = *src++;
      if ((next & 0xc0) != 0x80)
      {
        DEBUG_puts("3cupsUTF8ToUTF32: Returning -1 (bad UTF-8 sequence)");

	return (-1);
      }

      ch32 = (ch32 << 6) | (cups_utf32_t)(next & 0x3f);

      // Check for non-shortest form (invalid UTF-8)...
      if (ch32 < 0x800)
      {
        DEBUG_puts("3cupsUTF8ToUTF32: Returning -1 (bad UTF-8 sequence)");

	return (-1);
      }

      *dest++ = ch32;

      DEBUG_printf(("4cupsUTF8ToUTF32: %02x %02x %02x => %08X", src[-3], src[-2], src[-1], (unsigned)ch32));
    }
    else if ((ch & 0xf8) == 0xf0)
    {
      // Four-octet UTF-8...
      next = *src++;
      if ((next & 0xc0) != 0x80)
      {
        DEBUG_puts("3cupsUTF8ToUTF32: Returning -1 (bad UTF-8 sequence)");

	return (-1);
      }

      ch32 = (cups_utf32_t)((ch & 0x07) << 6) | (cups_utf32_t)(next & 0x3f);

      next = *src++;
      if ((next & 0xc0) != 0x80)
      {
        DEBUG_puts("3cupsUTF8ToUTF32: Returning -1 (bad UTF-8 sequence)");

	return (-1);
      }

      ch32 = (ch32 << 6) | (cups_utf32_t)(next & 0x3f);

      next = *src++;
      if ((next & 0xc0) != 0x80)
      {
        DEBUG_puts("3cupsUTF8ToUTF32: Returning -1 (bad UTF-8 sequence)");

	return (-1);
      }

      ch32 = (ch32 << 6) | (cups_utf32_t)(next & 0x3f);

      // Check for non-shortest form (invalid UTF-8)...
      if (ch32 < 0x10000)
      {
        DEBUG_puts("3cupsUTF8ToUTF32: Returning -1 (bad UTF-8 sequence)");

	return (-1);
      }

      *dest++ = ch32;

      DEBUG_printf(("4cupsUTF8ToUTF32: %02x %02x %02x %02x => %08X", src[-4], src[-3], src[-2], src[-1], (unsigned)ch32));
    }
    else
    {
      // More than 4-octet (invalid UTF-8 sequence)...
      DEBUG_puts("3cupsUTF8ToUTF32: Returning -1 (bad UTF-8 sequence)");

      return (-1);
    }

    // Check for UTF-16 surrogate (illegal UTF-8)...
    if (ch32 >= 0xd800 && ch32 <= 0xdfff)
      return (-1);
  }

  *dest = 0;

  DEBUG_printf(("3cupsUTF8ToUTF32: Returning %u characters", (unsigned)(maxout - 1 - i)));

  return ((ssize_t)(maxout - 1 - i));
}


//
// 'cupsUTF32ToUTF8()' - Convert UTF-32 to UTF-8.
//
// This function converts a UTF-32 (32-bit encoding of Unicode) string to a
// UTF-8 (8-bit encoding of Unicode) `nul`-terminated C string.
//

ssize_t					// O - Number of bytes or `-1` on error
cupsUTF32ToUTF8(
    char               *dest,		// O - Target string
    const cups_utf32_t *src,		// I - Source string
    const size_t       maxout)		// I - Max output in bytes
{
  char		*start;			// Start of destination string
  size_t	i;			// Looping variable
  int		swap;			// Byte-swap input to output
  cups_utf32_t	ch;			// Character value


  // Check for valid arguments and clear output...
  DEBUG_printf(("2cupsUTF32ToUTF8(dest=%p, src=%p, maxout=%u)", (void *)dest, (void *)src, (unsigned)maxout));

  if (dest)
    *dest = '\0';

  if (!dest || !src || maxout < 1)
  {
    DEBUG_puts("3cupsUTF32ToUTF8: Returning -1 (bad args)");

    return (-1);
  }

  // Check for leading BOM in UTF-32 and inverted BOM...
  start = dest;
  swap  = *src == 0xfffe0000;

  DEBUG_printf(("4cupsUTF32ToUTF8: swap=%d", swap));

  if (*src == 0xfffe0000 || *src == 0xfeff)
    src ++;

  // Convert input UTF-32 to output UTF-8...
  for (i = maxout - 1; *src && i > 0;)
  {
    ch = *src++;

    // Byte swap input UTF-32 if necessary (only byte-swapping 24 of 32 bits)
    if (swap)
      ch = ((ch >> 24) | ((ch >> 8) & 0xff00) | ((ch << 8) & 0xff0000));

    // Check for beyond Plane 16 (invalid UTF-32)...
    if (ch > 0x10ffff)
    {
      DEBUG_puts("3cupsUTF32ToUTF8: Returning -1 (character out of range)");

      return (-1);
    }

    // Convert UTF-32 character to UTF-8 character(s)...
    if (ch < 0x80)
    {
      // One-octet UTF-8 <= 127 (US-ASCII)...
      *dest++ = (char)ch;
      i --;

      DEBUG_printf(("4cupsUTF32ToUTF8: %08x => %02x", (unsigned)ch, dest[-1]));
    }
    else if (ch < 0x800)
    {
      // Two-octet UTF-8 <= 2047 (Latin-x)...
      if (i < 2)
      {
        DEBUG_puts("3cupsUTF32ToUTF8: Returning -1 (too long 2)");

        return (-1);
      }

      *dest++ = (char)(0xc0 | ((ch >> 6) & 0x1f));
      *dest++ = (char)(0x80 | (ch & 0x3f));
      i -= 2;

      DEBUG_printf(("4cupsUTF32ToUTF8: %08x => %02x %02x", (unsigned)ch, dest[-2], dest[-1]));
    }
    else if (ch < 0x10000)
    {
      // Three-octet UTF-8 <= 65535 (Plane 0 - BMP)...
      if (i < 3)
      {
        DEBUG_puts("3cupsUTF32ToUTF8: Returning -1 (too long 3)");

        return (-1);
      }

      *dest++ = (char)(0xe0 | ((ch >> 12) & 0x0f));
      *dest++ = (char)(0x80 | ((ch >> 6) & 0x3f));
      *dest++ = (char)(0x80 | (ch & 0x3f));
      i -= 3;

      DEBUG_printf(("4cupsUTF32ToUTF8: %08x => %02x %02x %02x", (unsigned)ch, dest[-3], dest[-2], dest[-1]));
    }
    else
    {
      // Four-octet UTF-8...
      if (i < 4)
      {
        DEBUG_puts("3cupsUTF32ToUTF8: Returning -1 (too long 4)");

        return (-1);
      }

      *dest++ = (char)(0xf0 | ((ch >> 18) & 0x07));
      *dest++ = (char)(0x80 | ((ch >> 12) & 0x3f));
      *dest++ = (char)(0x80 | ((ch >> 6) & 0x3f));
      *dest++ = (char)(0x80 | (ch & 0x3f));
      i -= 4;

      DEBUG_printf(("4cupsUTF32ToUTF8: %08x => %02x %02x %02x %02x", (unsigned)ch, dest[-4], dest[-3], dest[-2], dest[-1]));
    }
  }

  *dest = '\0';

  DEBUG_printf(("3cupsUTF32ToUTF8: Returning %d", (int)(dest - start)));

  return ((ssize_t)(dest - start));
}


//
// 'flush_map()' - Flush all character set maps out of cache.
//

static void
flush_map(void)
{
#ifdef HAVE_ICONV_H
  if (map_from_utf8 != (iconv_t)-1)
  {
    iconv_close(map_from_utf8);
    map_from_utf8 = (iconv_t)-1;
  }

  if (map_to_utf8 != (iconv_t)-1)
  {
    iconv_close(map_to_utf8);
    map_to_utf8 = (iconv_t)-1;
  }

  map_encoding = CUPS_ENCODING_AUTO;
#endif // HAVE_ICONV_H
}
