//
// DNS-SD API definitions for CUPS.
//
// Copyright © 2022 by OpenPrinting.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#ifndef _CUPS_DNSSD_H_
#  define _CUPS_DNSSD_H_
#  include "cups.h"
#  ifdef __cplusplus
extern "C" {
#  endif // __cplusplus


//
// Types and constants...
//

#  define CUPS_DNSSD_IF_INDEX_ANY		0
#  define CUPS_DNSSD_IF_INDEX_LOCAL	((uint32_t)-1)

typedef struct _cups_dnssd_s cups_dnssd_t;	// DNS-SD context

enum cups_dnssd_flags_e			// DNS-SD callback flag values
{
  CUPS_DNSSD_FLAGS_NONE = 0,		// No flags
  CUPS_DNSSD_FLAGS_ADD = 1,		// Added
  CUPS_DNSSD_FLAGS_REMOVE = 2,		// Removed
  CUPS_DNSSD_FLAGS_ERROR = 4		// Error occurred
};
typedef unsigned cups_dnssd_flags_t;	// DNS-SD callback flag bitmask

typedef struct _cups_dnssd_browse_s cups_dnssd_browse_t;
					// DNS browse request
typedef void (*cups_dnssd_browse_cb_t)(cups_dnssd_browse_t *browse, void *cb_data, cups_dnssd_flags_t flags, uint32_t if_index, const char *name, const char *regtype, const char *domain);
					// DNS-SD browse callback

typedef void (*cups_dnssd_error_cb_t)(void *cb_data, const char *message);
					// DNS-SD error callback

typedef struct _cups_dnssd_query_s cups_dnssd_query_t;
					// DNS query request
typedef void (*cups_dnssd_query_cb_t)(cups_dnssd_query_t * query, void *cb_data, cups_dnssd_flags_t flags, uint32_t if_index, const char *name, const char *regtype, const char *domain, size_t num_txt, cups_option_t *txt);
					// DNS-SD query callback

typedef struct _cups_dnssd_resolve_s cups_dnssd_resolve_t;
					// DNS resolve request
typedef void (*cups_dnssd_resolve_cb_t)(cups_dnssd_resolve_t *res, void *cb_data, cups_dnssd_flags_t flags, uint32_t if_index, const char *name, const char *regtype, const char *domain, const char *host, uint16_t port, size_t num_txt, cups_option_t *txt);
					// DNS-SD resolve callback

typedef struct _cups_dnssd_service_s cups_dnssd_service_t;
					// DNS service registration
typedef void (*cups_dnssd_service_cb_t)(cups_dnssd_service_t *service, void *cb_data, cups_dnssd_flags_t flags, const char *name, const char *regtype, const char *domain);
					// DNS-SD service registration callback


//
// Functions...
//

extern void		cupsDNSSDDelete(cups_dnssd_t *dns) _CUPS_PUBLIC;
extern cups_dnssd_t	*cupsDNSSDNew(cups_dnssd_error_cb_t error_cb, void *cb_data) _CUPS_PUBLIC;

extern void		cupsDNSSDBrowseDelete(cups_dnssd_browse_t *browser) _CUPS_PUBLIC;
extern cups_dnssd_browse_t *cupsDNSSDBrowseNew(cups_dnssd_t *dns, uint32_t if_index, const char *types, const char *domains, cups_dnssd_browse_cb_t browse_cb, void *cb_data) _CUPS_PUBLIC;

extern void		cupsDNSSDQueryDelete(cups_dnssd_query_t *query) _CUPS_PUBLIC;
extern cups_dnssd_query_t *cupsDNSSDQueryNew(cups_dnssd_t *dns, uint32_t if_index, const char *types, const char *domains, cups_dnssd_query_cb_t query_cb, void *cb_data) _CUPS_PUBLIC;

extern void		cupsDNSSDResolveDelete(cups_dnssd_resolve_t *res) _CUPS_PUBLIC;
extern cups_dnssd_resolve_t *cupsDNSSDResolveNew(cups_dnssd_t *dns, uint32_t if_index, const char *types, const char *domains, cups_dnssd_resolve_cb_t resolve_cb, void *cb_data) _CUPS_PUBLIC;

extern bool		cupsDNSSDServiceAdd(cups_dnssd_service_t *service, uint32_t if_index, const char *types, const char *domains, const char *host, uint16_t port, size_t num_txt, cups_option_t *txt) _CUPS_PUBLIC;
extern void		cupsDNSSDServiceDelete(cups_dnssd_service_t *service) _CUPS_PUBLIC;
extern cups_dnssd_service_t *cupsDNSSDServiceNew(cups_dnssd_t *dns, const char *name, cups_dnssd_service_cb_t cb, void *cb_data) _CUPS_PUBLIC;
extern bool		cupsDNSSDServicePublish(cups_dnssd_service_t *service) _CUPS_PUBLIC;
extern bool		cupsDNSSDServiceSetLocation(cups_dnssd_service_t *service, const char *geo_uri) _CUPS_PUBLIC;


#  ifdef __cplusplus
}
#  endif // __cplusplus
#endif // !_CUPS_DNSSD_H_
