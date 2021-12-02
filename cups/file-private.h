/*
 * Private file definitions for CUPS.
 *
 * Since stdio files max out at 256 files on many systems, we have to
 * write similar functions without this limit.  At the same time, using
 * our own file functions allows us to provide transparent support of
 * different line endings, gzip'd print files, PPD files, etc.
 *
 * Copyright © 2021 by OpenPrinting.
 * Copyright © 2007-2018 by Apple Inc.
 * Copyright © 1997-2007 by Easy Software Products, all rights reserved.
 *
 * Licensed under Apache License v2.0.  See the file "LICENSE" for more
 * information.
 */

#ifndef _CUPS_FILE_PRIVATE_H_
#  define _CUPS_FILE_PRIVATE_H_

/*
 * Include necessary headers...
 */

#  include "cups-private.h"
#  include <stdio.h>
#  include <stdlib.h>
#  include <stdarg.h>
#  include <fcntl.h>

#  ifdef _WIN32
#    include <io.h>
#    include <sys/locking.h>
#  endif /* _WIN32 */


/*
 * Some operating systems support large files via open flag O_LARGEFILE...
 */

#  ifndef O_LARGEFILE
#    define O_LARGEFILE 0
#  endif /* !O_LARGEFILE */


/*
 * Some operating systems don't define O_BINARY, which is used by Microsoft
 * and IBM to flag binary files...
 */

#  ifndef O_BINARY
#    define O_BINARY 0
#  endif /* !O_BINARY */


#  ifdef __cplusplus
extern "C" {
#  endif /* __cplusplus */


/*
 * Prototypes...
 */

extern int			_cupsFilePeekAhead(cups_file_t *fp, int ch);

#  ifdef __cplusplus
}
#  endif /* __cplusplus */

#endif /* !_CUPS_FILE_PRIVATE_H_ */
