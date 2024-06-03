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

extern void		cupsOAuthClearTokens(const char *auth_uri, const char *resource_uri) _CUPS_PUBLIC;
extern char		*cupsOAuthCopyAccessToken(const char *auth_uri, const char *resource_uri, time_t *auth_expires) _CUPS_PUBLIC;
extern cups_json_t	*cupsOAuthCopyMetadata(const char *auth_uri) _CUPS_PUBLIC;
extern char		*cupsOAuthCopyRefreshToken(const char *auth_uri, const char *resource_uri) _CUPS_PUBLIC;
extern bool		cupsOAuthDoAuthorize(cups_json_t *metadata, const char *resource_uri, const char *redirect_uri, const char *client_id, const char *state, const char *code_verifier, const char *scope);
extern char		*cupsOAuthDoRefresh(cups_json_t *metadata, const char *resource_uri, const char *refresh_token, time_t *access_expires) _CUPS_PUBLIC;
extern char		*cupsOAuthDoRegisterClient(cups_json_t *metadata, const char *redirect_uri, const char *client_name, const char *client_uri, const char *software_id, const char *software_version, const char *logo_uri, const char *tos_uri) _CUPS_PUBLIC;
extern char		*cupsOAuthDoToken(cups_json_t *metadata, const char *resource_uri, const char *redirect_uri, const char *client_id, const char *code, const char *code_verifier, time_t *expires);
extern void		cupsOAuthSetTokens(const char *auth_uri, const char *resource_uri, const char *access_token, time_t access_expires, const char *refresh_token) _CUPS_PUBLIC;


#  ifdef __cplusplus
}
#  endif /* __cplusplus */
#endif // !_CUPS_OAUTH_H_
