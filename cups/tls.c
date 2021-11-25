/*
 * TLS routines for CUPS.
 *
 * Copyright © 2021 by OpenPrinting.
 * Copyright @ 2007-2014 by Apple Inc.
 * Copyright @ 1997-2007 by Easy Software Products, all rights reserved.
 *
 * Licensed under Apache License v2.0.  See the file "LICENSE" for more
 * information.
 */

/*
 * Include necessary headers...
 */

#include "cups-private.h"
#include "debug-internal.h"
#include <fcntl.h>
#include <math.h>
#ifdef _WIN32
#  include <tchar.h>
#else
#  include <poll.h>
#  include <signal.h>
#  include <sys/time.h>
#  include <sys/resource.h>
#endif /* _WIN32 */


/*
 * Include platform-specific TLS code...
 */

#ifdef HAVE_TLS
#  ifdef HAVE_GNUTLS
#    include "tls-gnutls.c"
#  elif defined(HAVE_CDSASSL)
#    include "tls-darwin.c"
#  else
#    include "tls-sspi.c"
#  endif /* HAVE_GNUTLS */
#endif /* HAVE_TLS */
