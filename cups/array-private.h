/*
 * Private array definitions for CUPS.
 *
 * Copyright © 2021 by OpenPrinting.
 * Copyright © 2011-2012 by Apple Inc.
 *
 * Licensed under Apache License v2.0.  See the file "LICENSE" for more
 * information.
 */

#ifndef _CUPS_ARRAY_PRIVATE_H_
#  define _CUPS_ARRAY_PRIVATE_H_
#  include <cups/array.h>
#  ifdef __cplusplus
extern "C" {
#  endif /* __cplusplus */


/*
 * Functions...
 */

extern bool	_cupsArrayAddStrings(cups_array_t *a, const char *s, char delim) _CUPS_PRIVATE;
extern cups_array_t *_cupsArrayNewStrings(const char *s, char delim) _CUPS_PRIVATE;


#  ifdef __cplusplus
}
#  endif /* __cplusplus */
#endif /* !_CUPS_ARRAY_PRIVATE_H_ */
