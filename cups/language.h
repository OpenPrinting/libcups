//
// Language localization definitions for CUPS.
//
// Copyright © 2021-2022 by OpenPrinting.
// Copyright © 2007-2011 by Apple Inc.
// Copyright © 1997-2006 by Easy Software Products.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#ifndef _CUPS_LANGUAGE_H_
#  define _CUPS_LANGUAGE_H_
#  include "transcode.h"
#  include <stdio.h>
#  include <locale.h>
#  ifdef __cplusplus
extern "C" {
#  endif // __cplusplus


//
// Types...
//


typedef struct _cups_lang_s cups_lang_t;// Language Cache


//
// Prototypes...
//

extern bool		cupsLangAddStrings(const char *language, const char *strings);
extern cups_lang_t	*cupsLangDefault(void) _CUPS_PUBLIC;
extern cups_lang_t	*cupsLangFind(const char *language) _CUPS_PUBLIC;
extern const char	*cupsLangFormatString(cups_lang_t *lang, char *buffer, size_t bufsize, const char *format, ...) _CUPS_FORMAT(4,5) _CUPS_PUBLIC;
extern cups_encoding_t	cupsLangGetEncoding(void) _CUPS_PUBLIC;
extern const char	*cupsLangGetName(cups_lang_t *lang) _CUPS_PUBLIC;
extern const char	*cupsLangGetString(cups_lang_t *lang, const char *s) _CUPS_PUBLIC;
extern ssize_t		cupsLangPrintf(FILE *fp, const char *format, ...) _CUPS_FORMAT(2, 3) _CUPS_PUBLIC;
extern ssize_t		cupsLangPuts(FILE *fp, const char *message) _CUPS_PUBLIC;
extern void		cupsLangSetDirectory(const char *d) _CUPS_PUBLIC;
extern void		cupsLangSetLocale(char *argv[]) _CUPS_PUBLIC;


#  ifdef __cplusplus
}
#  endif // __cplusplus
#endif // !_CUPS_LANGUAGE_H_
