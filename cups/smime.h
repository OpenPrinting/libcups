//
// S/MIME API definitions for CUPS.
//
// Copyright © 2023 by OpenPrinting.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#ifndef _CUPS_SMIME_H_
#  define _CUPS_SMIME_H_
#  include "cups.h"
#  ifdef __cplusplus
extern "C" {
#  endif // __cplusplus


//
// Types...
//

typedef struct _cups_smime_s cups_smime_t;
					// S/MIME read/write stream
typedef ssize_t (*cups_smime_cb_t)(void *context, char *buffer, size_t bytes);
					// S/MIME input/output callback


//
// Functions...
//

extern bool		cupsSMIMEClose(cups_smime_t *smime) _CUPS_PUBLIC;
extern cups_smime_t	*cupsSMIMEOpen(const char *filename, const char *mode, const char *credentials, const char *key, const char *password) _CUPS_PUBLIC;
extern cups_smime_t	*cupsSMIMEOpenIO(void *context, cups_smime_cb_t cb, const char *mode, const char *credentials, const char *key, const char *password) _CUPS_PUBLIC;
extern ssize_t		cupsSMIMERead(cups_smime_t *smime, char *buffer, size_t bytes) _CUPS_PUBLIC;
extern ssize_t		cupsSMIMEWrite(cups_smime_t *smime, const char *buffer, size_t bytes) _CUPS_PUBLIC;


#  ifdef __cplusplus
}
#  endif // __cplusplus
#endif // !_CUPS_SMIME_H_
