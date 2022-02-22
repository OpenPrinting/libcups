/*
 * Private file definitions for CUPS.
 *
 * Since stdio files max out at 256 files on many systems, we have to
 * write similar functions without this limit.  At the same time, using
 * our own file functions allows us to provide transparent support of
 * different line endings, gzip'd print files, PPD files, etc.
 *
 * Copyright © 2021-2022 by OpenPrinting.
 * Copyright © 2007-2018 by Apple Inc.
 * Copyright © 1997-2007 by Easy Software Products, all rights reserved.
 *
 * Licensed under Apache License v2.0.  See the file "LICENSE" for more
 * information.
 */

#ifndef _CUPS_FILE_PRIVATE_H_
#  define _CUPS_FILE_PRIVATE_H_
#  include "cups-private.h"
#  include <stdio.h>
#  include <stdlib.h>
#  include <stdarg.h>
#  include <fcntl.h>
#  ifdef _WIN32
#    include <io.h>
#    include <sys/locking.h>
#  endif /* _WIN32 */
#  ifndef O_LARGEFILE
#    define O_LARGEFILE 0
#  endif /* !O_LARGEFILE */
#  ifndef O_BINARY
#    define O_BINARY 0
#  endif /* !O_BINARY */
#  ifdef __cplusplus
extern "C" {
#  endif /* __cplusplus */


/*
 * Prototypes...
 */

extern bool	_cupsFilePeekAhead(cups_file_t *fp, int ch);


#  ifdef __cplusplus
}
#  endif /* __cplusplus */
#endif /* !_CUPS_FILE_PRIVATE_H_ */
