//
// Transcoding definitions for CUPS.
//
// Copyright © 2022 by OpenPrinting.
// Copyright © 2007-2011 by Apple Inc.
// Copyright © 1997-2006 by Easy Software Products.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#ifndef _CUPS_TRANSCODE_H_
#  define _CUPS_TRANSCODE_H_
#  include "base.h"
#  ifdef __cplusplus
extern "C" {
#  endif // __cplusplus


//
// Constants...
//

#  define CUPS_MAX_USTRING	8192	// Max size of Unicode string


//
// Types...
//

typedef enum cups_encoding_e		// Language Encodings
{
  CUPS_ENCODING_AUTO = -1,		// Auto-detect the encoding @private@
  CUPS_ENCODING_US_ASCII,		// US ASCII
  CUPS_ENCODING_ISO8859_1,		// ISO-8859-1
  CUPS_ENCODING_ISO8859_2,		// ISO-8859-2
  CUPS_ENCODING_ISO8859_3,		// ISO-8859-3
  CUPS_ENCODING_ISO8859_4,		// ISO-8859-4
  CUPS_ENCODING_ISO8859_5,		// ISO-8859-5
  CUPS_ENCODING_ISO8859_6,		// ISO-8859-6
  CUPS_ENCODING_ISO8859_7,		// ISO-8859-7
  CUPS_ENCODING_ISO8859_8,		// ISO-8859-8
  CUPS_ENCODING_ISO8859_9,		// ISO-8859-9
  CUPS_ENCODING_ISO8859_10,		// ISO-8859-10
  CUPS_ENCODING_UTF_8,			// UTF-8
  CUPS_ENCODING_ISO8859_13,		// ISO-8859-13
  CUPS_ENCODING_ISO8859_14,		// ISO-8859-14
  CUPS_ENCODING_ISO8859_15,		// ISO-8859-15
  CUPS_ENCODING_WINDOWS_874,		// CP-874
  CUPS_ENCODING_WINDOWS_1250,		// CP-1250
  CUPS_ENCODING_WINDOWS_1251,		// CP-1251
  CUPS_ENCODING_WINDOWS_1252,		// CP-1252
  CUPS_ENCODING_WINDOWS_1253,		// CP-1253
  CUPS_ENCODING_WINDOWS_1254,		// CP-1254
  CUPS_ENCODING_WINDOWS_1255,		// CP-1255
  CUPS_ENCODING_WINDOWS_1256,		// CP-1256
  CUPS_ENCODING_WINDOWS_1257,		// CP-1257
  CUPS_ENCODING_WINDOWS_1258,		// CP-1258
  CUPS_ENCODING_KOI8_R,			// KOI-8-R
  CUPS_ENCODING_KOI8_U,			// KOI-8-U
  CUPS_ENCODING_ISO8859_11,		// ISO-8859-11
  CUPS_ENCODING_ISO8859_16,		// ISO-8859-16
  CUPS_ENCODING_MAC_ROMAN,		// MacRoman
  CUPS_ENCODING_SBCS_END = 63,		// End of single-byte encodings @private@

  CUPS_ENCODING_WINDOWS_932,		// Japanese JIS X0208-1990
  CUPS_ENCODING_WINDOWS_936,		// Simplified Chinese GB 2312-80
  CUPS_ENCODING_WINDOWS_949,		// Korean KS C5601-1992
  CUPS_ENCODING_WINDOWS_950,		// Traditional Chinese Big Five
  CUPS_ENCODING_WINDOWS_1361,		// Korean Johab
  CUPS_ENCODING_BG18030,		// Chinese GB 18030
  CUPS_ENCODING_DBCS_END = 127,		// End of double-byte encodings @private@

  CUPS_ENCODING_EUC_CN,			// EUC Simplified Chinese
  CUPS_ENCODING_EUC_JP,			// EUC Japanese
  CUPS_ENCODING_EUC_KR,			// EUC Korean
  CUPS_ENCODING_EUC_TW,			// EUC Traditional Chinese
  CUPS_ENCODING_JIS_X0213,		// JIS X0213 aka Shift JIS
  CUPS_ENCODING_VBCS_END = 191		// End of variable-length encodings @private@
} cups_encoding_t;

typedef unsigned long cups_utf32_t;	// UTF-32 Unicode/ISO-10646 unit


//
// Functions...
//

extern ssize_t		cupsCharsetToUTF8(char *dest, const char *src, const size_t maxout, const cups_encoding_t encoding) _CUPS_PUBLIC;
extern const char	*cupsEncodingString(cups_encoding_t value) _CUPS_PUBLIC;
extern cups_encoding_t	cupsEncodingValue(const char *s) _CUPS_PUBLIC;
extern ssize_t		cupsUTF8ToCharset(char *dest, const char *src, const size_t maxout, const cups_encoding_t encoding) _CUPS_PUBLIC;
extern ssize_t		cupsUTF8ToUTF32(cups_utf32_t *dest, const char *src, const size_t maxout) _CUPS_PUBLIC;
extern ssize_t		cupsUTF32ToUTF8(char *dest, const cups_utf32_t *src, const size_t maxout) _CUPS_PUBLIC;


#  ifdef __cplusplus
}
#  endif // __cplusplus
#endif // !_CUPS_TRANSCODE_H_
