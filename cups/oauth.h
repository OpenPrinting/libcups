//
// OAuth API definitions for CUPS.
//
// Copyright © 2024 by OpenPrinting.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#ifndef _CUPS_OAUTH_H_
#  define _CUPS_OAUTH_H_
#  include "jwt.h"
#  ifdef __cplusplus
extern "C" {
#  endif /* __cplusplus */


//
// Functions...
//

extern void		cupsOAuthClearTokens(const char *auth_server, const char *res_server) _CUPS_PUBLIC;
extern char		*cupsOAuthCopyAuthToken(const char *auth_server, const char *res_server, time_t *auth_expires) _CUPS_PUBLIC;
extern cups_json_t	*cupsOAuthCopyMetadata(const char *auth_server) _CUPS_PUBLIC;
extern char		*cupsOAuthCopyRefreshToken(const char *auth_server, const char *res_server) _CUPS_PUBLIC;
extern void		cupsOAuthSetTokens(const char *auth_server, const char *res_server, const char *auth_token, time_t auth_expires, const char *refresh_token) _CUPS_PUBLIC;


#  ifdef __cplusplus
}
#  endif /* __cplusplus */
#endif // !_CUPS_OAUTH_H_
