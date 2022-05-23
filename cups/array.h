/*
 * Sorted array definitions for CUPS.
 *
 * Copyright © 2021-2022 by OpenPrinting.
 * Copyright © 2007-2010 by Apple Inc.
 * Copyright © 1997-2007 by Easy Software Products.
 *
 * Licensed under Apache License v2.0.  See the file "LICENSE" for more
 * information.
 */

#ifndef _CUPS_ARRAY_H_
#  define _CUPS_ARRAY_H_
#  include "base.h"
#  include <stdlib.h>
#  include <limits.h>
#  ifdef __cplusplus
extern "C" {
#  endif /* __cplusplus */


/*
 * Types and structures...
 */

typedef struct _cups_array_s cups_array_t;
					/**** CUPS array type ****/
typedef int (*cups_array_cb_t)(void *first, void *second, void *data);
					/**** Array comparison function ****/
typedef size_t (*cups_ahash_cb_t)(void *element, void *data);
					/**** Array hash function ****/
typedef void *(*cups_acopy_cb_t)(void *element, void *data);
					/**** Array element copy function ****/
typedef void (*cups_afree_cb_t)(void *element, void *data);
					/**** Array element free function ****/


/*
 * Functions...
 */

extern bool		cupsArrayAdd(cups_array_t *a, void *e) _CUPS_PUBLIC;
extern bool		cupsArrayAddStrings(cups_array_t *a, const char *s, char delim) _CUPS_PUBLIC;
extern void		cupsArrayClear(cups_array_t *a) _CUPS_PUBLIC;
extern void		cupsArrayDelete(cups_array_t *a) _CUPS_PUBLIC;
extern cups_array_t	*cupsArrayDup(cups_array_t *a) _CUPS_PUBLIC;
extern void		*cupsArrayFind(cups_array_t *a, void *e) _CUPS_PUBLIC;
extern size_t		cupsArrayGetCount(cups_array_t *a) _CUPS_PUBLIC;
extern void		*cupsArrayGetCurrent(cups_array_t *a) _CUPS_PUBLIC;
extern void		*cupsArrayGetElement(cups_array_t *a, size_t n) _CUPS_PUBLIC;
extern void		*cupsArrayGetFirst(cups_array_t *a) _CUPS_PUBLIC;
extern size_t		cupsArrayGetIndex(cups_array_t *a) _CUPS_PUBLIC;
extern size_t		cupsArrayGetInsert(cups_array_t *a) _CUPS_PUBLIC;
extern void		*cupsArrayGetLast(cups_array_t *a) _CUPS_PUBLIC;
extern void		*cupsArrayGetNext(cups_array_t *a) _CUPS_PUBLIC;
extern void		*cupsArrayGetPrev(cups_array_t *a) _CUPS_PUBLIC;
extern void		*cupsArrayGetUserData(cups_array_t *a) _CUPS_PUBLIC;
extern bool		cupsArrayInsert(cups_array_t *a, void *e) _CUPS_PUBLIC;
extern cups_array_t	*cupsArrayNew(cups_array_cb_t f, void *d, cups_ahash_cb_t hf, size_t hsize, cups_acopy_cb_t cf, cups_afree_cb_t ff) _CUPS_PUBLIC;
extern cups_array_t	*cupsArrayNewStrings(const char *s, char delim) _CUPS_PUBLIC;
extern bool		cupsArrayRemove(cups_array_t *a, void *e) _CUPS_PUBLIC;
extern void		*cupsArrayRestore(cups_array_t *a) _CUPS_PUBLIC;
extern bool		cupsArraySave(cups_array_t *a) _CUPS_PUBLIC;

#  ifdef __cplusplus
}
#  endif /* __cplusplus */
#endif /* !_CUPS_ARRAY_H_ */
