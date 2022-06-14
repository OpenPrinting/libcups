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
#  include "language.h"
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

typedef unsigned long  cups_utf32_t;	// UTF-32 Unicode/ISO-10646 unit


//
// Functions...
//

extern ssize_t	cupsCharsetToUTF8(char *dest, const char *src, const size_t maxout, const cups_encoding_t encoding) _CUPS_PUBLIC;
extern ssize_t	cupsUTF8ToCharset(char *dest, const char *src, const size_t maxout, const cups_encoding_t encoding) _CUPS_PUBLIC;
extern ssize_t	cupsUTF8ToUTF32(cups_utf32_t *dest, const char *src, const size_t maxout) _CUPS_PUBLIC;
extern ssize_t	cupsUTF32ToUTF8(char *dest, const cups_utf32_t *src, const size_t maxout) _CUPS_PUBLIC;


#  ifdef __cplusplus
}
#  endif // __cplusplus
#endif // !_CUPS_TRANSCODE_H_
