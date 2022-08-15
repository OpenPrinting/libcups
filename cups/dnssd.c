//
// DNS-SD API definitions for CUPS.
//
// Copyright © 2022 by OpenPrinting.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#include <config.h>
#include "cups.h"
#include "dnssd.h"
#include "debug-internal.h"

#if _WIN32
#  include <windns.h>
#elif defined(HAVE_MDNSRESPONDER)
#  include <dns_sd.h>
#  include <poll.h>
#else // HAVE_AVAHI
#  include <avahi-client/client.h>
#  include <avahi-client/lookup.h>
#  include <avahi-client/publish.h>
#  include <avahi-common/alternative.h>
#  include <avahi-common/error.h>
#  include <avahi-common/malloc.h>
#  include <avahi-common/thread-watch.h>

struct _cups_dns_srv_s			// DNS SRV record
{
  AvahiEntryGroup	*srv;		// SRV record
  AvahiStringList	*txt;		// TXT record
};
#endif // _WIN32


//
// Private structures...
//

struct _cups_dnssd_s			// DNS-SD context
{
};

struct _cups_dnssd_browse_s		// DNS-SD browse request
{
  cups_dnssd_t		*dnssd;		// DNS-SD context
  cups_dnssd_browse_cb_t cb;		// Browse callback
  void			*cb_data;	// Browse callback data

#if _WIN32
#elif defined(HAVE_MDNSRESPONDER)
#else // HAVE_AVAHI
#endif // _WIN32
};

struct _cups_dns_query_s		// DNS-SD query request
{
  cups_dnssd_t		*dnssd;		// DNS-SD context
  cups_dnssd_query_cb_t	cb;		// Query callback
  void			*cb_data;	// Query callback data

#if _WIN32
#elif defined(HAVE_MDNSRESPONDER)
#else // HAVE_AVAHI
#endif // _WIN32
};

struct _cups_dns_resolve_s		// DNS-SD resolve request
{
  cups_dnssd_t		*dnssd;		// DNS-SD context
  cups_dnssd_resolve_cb_t cb;		// Resolve callback
  void			*cb_data;	// Resolve callback data

#if _WIN32
#elif defined(HAVE_MDNSRESPONDER)
#else // HAVE_AVAHI
#endif // _WIN32
};

struct _cups_dns_service_s		// DNS-SD service registration
{
  cups_dnssd_t		*dnssd;		// DNS-SD context
  char			*name;		// Service name
  cups_dnssd_service_cb_t cb;		// Service callback
  void			*cb_data;	// Service callback data

#if _WIN32
#elif defined(HAVE_MDNSRESPONDER)
#else // HAVE_AVAHI
#endif // _WIN32
};

