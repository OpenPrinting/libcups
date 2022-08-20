//
// DNS-SD API definitions for CUPS.
//
// Copyright © 2022 by OpenPrinting.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#include <config.h>
#include "cups-private.h"
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
  cups_mutex_t		mutex;		// Mutex for context
  size_t		name_changes;	// Number of hostname changes
  cups_dnssd_error_cb_t	cb;		// Error callback function
  void			*cb_data;	// Error callback data
  cups_array_t		*browses,	// Browse requests
			*queries,	// Query requests
			*resolves,	// Resolve requests
			*services;	// Registered services

#if _WIN32

#elif defined(HAVE_MDNSRESPONDER)
  DNSServiceRef		ref;		// Master service reference
  cups_thread_t		monitor;	// Monitoring thread

#else // HAVE_AVAHI
  AvahiClient		*client;	// Avahi client connection
  AvahiThreadedPoll	*monitor;	// Monitoring thread
#endif // _WIN32
};

struct _cups_dnssd_browse_s		// DNS-SD browse request
{
  cups_dnssd_t		*dnssd;		// DNS-SD context
  cups_dnssd_browse_cb_t cb;		// Browse callback
  void			*cb_data;	// Browse callback data

#if _WIN32
#elif defined(HAVE_MDNSRESPONDER)
  DNSServiceRef		ref;		// Browse reference
#else // HAVE_AVAHI
#endif // _WIN32
};

struct _cups_dnssd_query_s		// DNS-SD query request
{
  cups_dnssd_t		*dnssd;		// DNS-SD context
  cups_dnssd_query_cb_t	cb;		// Query callback
  void			*cb_data;	// Query callback data

#if _WIN32
#elif defined(HAVE_MDNSRESPONDER)
  DNSServiceRef		ref;		// Query reference
#else // HAVE_AVAHI
#endif // _WIN32
};

struct _cups_dnssd_resolve_s		// DNS-SD resolve request
{
  cups_dnssd_t		*dnssd;		// DNS-SD context
  cups_dnssd_resolve_cb_t cb;		// Resolve callback
  void			*cb_data;	// Resolve callback data

#if _WIN32
#elif defined(HAVE_MDNSRESPONDER)
  DNSServiceRef		ref;		// Resolve reference
#else // HAVE_AVAHI
#endif // _WIN32
};

struct _cups_dnssd_service_s		// DNS-SD service registration
{
  cups_dnssd_t		*dnssd;		// DNS-SD context
  char			*name;		// Service name
  cups_dnssd_service_cb_t cb;		// Service callback
  void			*cb_data;	// Service callback data
  unsigned char		loc[16];	// LOC record data
  bool			loc_set;	// Is the location data set?

#if _WIN32
#elif defined(HAVE_MDNSRESPONDER)
  size_t		num_refs;	// Number of service references
  DNSServiceRef		refs[16];	// Service references
  DNSRecordRef		loc_refs[16];	// Service location records
#else // HAVE_AVAHI
#endif // _WIN32
};


//
// Local functions...
//

static void		delete_browse(cups_dnssd_browse_t *browse);
static void		delete_query(cups_dnssd_query_t *query);
static void		delete_resolve(cups_dnssd_resolve_t *resolve);
static void		delete_service(cups_dnssd_service_t *service);

#ifdef HAVE_MDNSRESPONDER
static void		*mdns_monitor(cups_dnssd_t *dnssd);
static void		mdns_browse_cb(DNSServiceRef ref, DNSServiceFlags flags, uint32_t interfaceIndex, DNSServiceErrorType error, const char *name, const char *regtype, const char *domain, cups_dnssd_browse_t *browse);
static void		mdns_query_cb(DNSServiceRef ref, DNSServiceFlags flags, uint32_t if_index, DNSServiceErrorType error, const char *name, uint16_t rrtype, uint16_t rrclass, uint16_t rdlen, const void *rdata, uint32_t ttl, cups_dnssd_query_t *query);
static void		mdns_resolve_cb(DNSServiceRef ref, DNSServiceFlags flags, uint32_t if_index, DNSServiceErrorType error, const char *fullname, const char *host, uint16_t port, uint16_t txtlen, const unsigned char *txt, cups_dnssd_resolve_t *resolve);
static void		mdns_service_cb(DNSServiceRef ref, DNSServiceFlags flags, DNSServiceErrorType error, const char *name, const char *regtype, const char *domain, cups_dnssd_service_t *service);
static const char	*mdns_strerror(DNSServiceErrorType errorCode);
static cups_dnssd_flags_t mdns_to_cups(DNSServiceFlags flags, DNSServiceErrorType error);
#endif // HAVE_MDNSRESPONDER
static void		report_error(cups_dnssd_t *dnssd, const char *message, ...) _CUPS_FORMAT(2,3);


//
// 'cupsDNSSDAssembleFullName()' - Create a full service name from the instance
//                                 name, registration type, and domain.
//

bool					// O - `true` on success, `false` on failure
cupsDNSSDAssembleFullName(
    char       *fullname,		// I - Buffer for full name
    size_t     fullsize,		// I - Size of buffer
    const char *name,			// I - Service instance name
    const char *type,			// I - Registration type
    const char *domain)			// I - Domain
{
  if (!fullname || !fullsize || !name || !type)
    return (false);

#if _WIN32
  return (false);

#elif defined(HAVE_MDNSRESPONDER)
  if (fullsize < kDNSServiceMaxDomainName)
    return (false);

  return (DNSServiceConstructFullName(fullname, name, type, domain) == kDNSServiceErr_NoError);
#else // HAVE_AVAHI

  return (false);
#endif // _WIN32
}


//
// 'cupsDNSSDDecodeTXT()' - Decode a TXT record into key/value pairs.
//
// This function converts the DNS TXT record encoding of key/value pairs into
// `cups_option_t` elements that can be accessed using the @link cupsGetOption@
// function and freed using the @link cupsFreeOptions@ function.
//

size_t					// O - Number of key/value pairs
cupsDNSSDDecodeTXT(
    const unsigned char *txtrec,	// I - TXT record data
    uint16_t            txtlen,		// I - TXT record length
    cups_option_t       **txt)		// O - Key/value pairs
{
  size_t	num_txt = 0;		// Number of key/value pairs
  unsigned char	keylen;			// Length of key/value
  char		key[256],		// Key/value buffer
		*value;			// Pointer to value
  const unsigned char *txtptr,		// Pointer into TXT record data
		*txtend;		// End of TXT record data


  // Range check input...
  if (txt)
    *txt = NULL;
  if (!txtrec || !txtlen || !txt)
    return (0);

  // Loop through the record...
  for (txtptr = txtrec, txtend = txtrec + txtlen; txtptr < txtend; txtptr += keylen)
  {
    // Format is a length byte followed by "key=value"
    keylen = *txtptr++;
    if (keylen == 0 || (txtptr + keylen) > txtend)
      break;				// Bogus length

    // Copy the data to a C string...
    memcpy(key, txtptr, keylen);
    key[keylen] = '\0';

    if ((value = strchr(key, '=')) != NULL)
    {
      // Got value separator, add it...
      *value++ = '\0';

      num_txt = cupsAddOption(key, value, num_txt, txt);
    }
    else
    {
      // No value, stop...
      break;
    }
  }

  // Return the number of pairs we parsed...
  return (num_txt);

}


//
// 'cupsDNSSDSeparateFullName()' - Separate a full service name into an instance
//                                 name, registration type, and domain.
//

bool					// O - `true` on success, `false` on error
cupsDNSSDSeparateFullName(
    const char *fullname,		// I - Full service name
    char       *name,			// I - Instance name buffer
    size_t     namesize,		// I - Size of instance name buffer
    char       *type,			// I - Registration type buffer
    size_t     typesize,		// I - Size of registration type buffer
    char       *domain,			// I - Domain name buffer
    size_t     domainsize)		// I - Size of domain name buffer
{
  // TODO: Implement cupsDNSSDSeparateFullName
  (void)fullname;
  (void)name;
  (void)namesize;
  (void)type;
  (void)typesize;
  (void)domain;
  (void)domainsize;

  return (false);
}


//
// 'cupsDNSSDDelete()' - Delete a DNS-SD context and all its requests.
//

void
cupsDNSSDDelete(cups_dnssd_t *dnssd)	// I - DNS-SD context
{
  if (!dnssd)
    return;

  cupsMutexLock(&dnssd->mutex);

  cupsArrayDelete(dnssd->browses);
  cupsArrayDelete(dnssd->queries);
  cupsArrayDelete(dnssd->resolves);
  cupsArrayDelete(dnssd->services);

#if _WIN32
  cupsThreadCancel(&dnssd->monitor);
  DNSServiceRefDeallocate(dnssd->ref);

#elif defined(HAVE_MDNSRESPONDER)

#else // HAVE_AVAHI

#endif // _WIN32

  cupsMutexUnlock(&dnssd->mutex);
  cupsMutexDestroy(&dnssd->mutex);
  free(dnssd);
}


//
// 'cupsDNSSDNew()' - Create a new DNS-SD context.
//
// This function creates a new DNS-SD context for browsing, querying, resolving,
// and/or registering services.
//

cups_dnssd_t *				// O - DNS-SD context
cupsDNSSDNew(
    cups_dnssd_error_cb_t error_cb,	// I - Error callback function
    void                  *cb_data)	// I - Error callback data
{
  cups_dnssd_t	*dnssd;			// DNS-SD context


  // Allocate memory...
  if ((dnssd = (cups_dnssd_t *)calloc(1, sizeof(cups_dnssd_t))) == NULL)
    return (NULL);

  // Save the error callback...
  dnssd->cb      = error_cb;
  dnssd->cb_data = cb_data;

  // Initialize the mutex...
  cupsMutexInit(&dnssd->mutex);

  // Setup the DNS-SD connection and monitor thread...
#if _WIN32

#elif defined(HAVE_MDNSRESPONDER)
  DNSServiceErrorType error;		// Error code

  if ((error = DNSServiceCreateConnection(&dnssd->ref)) == kDNSServiceErr_NoError)
  {
    if ((dnssd->monitor = cupsThreadCreate((void *(*)(void *))mdns_monitor, dnssd)) == 0)
    {
      report_error(dnssd, "Unable to create DNS-SD thread: %s", strerror(errno));
      cupsDNSSDDelete(dnssd);
      return (NULL);
    }

    cupsThreadDetach(dnssd->monitor);
  }
  else
  {
    report_error(dnssd, "Unable to initialize DNS-SD: %s", mdns_strerror(error));
    cupsDNSSDDelete(dnssd);
    return (NULL);
  }

#else // HAVE_AVAHI

#endif // _WIN32

  return (dnssd);
}



//
// 'cupsDNSSDBrowseDelete()' - Cancel and delete a browse request.
//

void
cupsDNSSDBrowseDelete(
    cups_dnssd_browse_t *browse)	// I - Browse request
{
  if (browse)
  {
    cups_dnssd_t *dnssd = browse->dnssd;

    cupsMutexLock(&dnssd->mutex);
    cupsArrayRemove(dnssd->browses, browse);
    cupsMutexUnlock(&dnssd->mutex);
  }
}


//
// 'cupsDNSSDBrowseGetContext()' - Get the DNS-SD context for the browse request.
//

cups_dnssd_t *				// O - Context or `NULL`
cupsDNSSDBrowseGetContext(
    cups_dnssd_browse_t *browse)	// I - Browse request
{
  return (browse ? browse->dnssd : NULL);
}


//
// 'cupsDNSSDBrowseNew()' - Create a new DNS-SD browse request.
//

cups_dnssd_browse_t *			// O - Browse request or `NULL` on error
cupsDNSSDBrowseNew(
    cups_dnssd_t           *dnssd,	// I - DNS-SD context
    uint32_t               if_index,	// I - Interface index or `CUPS_DNSSD_IF_ANY` or `CUPS_DNSSD_IF_LOCAL`
    const char             *types,	// I - Service types
    const char             *domain,	// I - Domain name or `NULL` for default
    cups_dnssd_browse_cb_t browse_cb,	// I - Browse callback function
    void                   *cb_data)	// I - Browse callback data
{
  cups_dnssd_browse_t	*browse;	// Browse request


  // Range check input...
  if (!dnssd || !types || !browse_cb)
    return (NULL);

  // Allocate memory for the browser...
  if ((browse = (cups_dnssd_browse_t *)calloc(1, sizeof(cups_dnssd_browse_t))) == NULL)
    return (NULL);

  browse->dnssd   = dnssd;
  browse->cb      = browse_cb;
  browse->cb_data = cb_data;

  cupsMutexLock(&dnssd->mutex);

  if (!dnssd->browses)
  {
    // Create an array of browsers...
    if ((dnssd->browses = cupsArrayNew(NULL, NULL, NULL, 0, NULL, (cups_afree_cb_t)delete_browse)) == NULL)
    {
      // Unable to create...
      free(browse);
      browse = NULL;
      goto done;
    }
  }

#if _WIN32

#elif defined(HAVE_MDNSRESPONDER)
  DNSServiceErrorType error;		// Error, if any

  browse->ref = dnssd->ref;
  if ((error = DNSServiceBrowse(&browse->ref, kDNSServiceFlagsShareConnection, if_index, types, domain, (DNSServiceBrowseReply)mdns_browse_cb, browse)) != kDNSServiceErr_NoError)
  {
    report_error(dnssd, "Unable to create DNS-SD browse request: %s", mdns_strerror(error));
    free(browse);
    browse = NULL;
    goto done;
  }

#else // HAVE_AVAHI

#endif // _WIN32

  cupsArrayAdd(dnssd->browses, browse);

  done:

  cupsMutexUnlock(&dnssd->mutex);

  return (browse);
}



//
// 'cupsDNSSDQueryDelete()' - Cancel and delete a query request.
//

void
cupsDNSSDQueryDelete(
    cups_dnssd_query_t *query)		// I - Query request
{
  if (query)
  {
    cups_dnssd_t *dnssd = query->dnssd;

    cupsMutexLock(&dnssd->mutex);
    cupsArrayRemove(dnssd->queries, query);
    cupsMutexUnlock(&dnssd->mutex);
  }
}


//
// 'cupsDNSSDQueryGetContext()' - Get the DNS-SD context for the query request.
//

cups_dnssd_t *				// O - DNS-SD context or `NULL`
cupsDNSSDQueryGetContext(
    cups_dnssd_query_t *query)		// I - Query request
{
  return (query ? query->dnssd : NULL);
}


//
// 'cupsDNSSDQueryNew()' - Create a new query request.
//
// This function creates a new DNS-SD query request for the specified record
// type.  The "fullname" parameter specifies the full DNS name of the service
// (instance name, type, and domain) being queried.
//

cups_dnssd_query_t *			// O - Query request or `NULL` on error
cupsDNSSDQueryNew(
    cups_dnssd_t          *dnssd,	// I - DNS-SD context
    uint32_t              if_index,	// I - Interface index or `CUPS_DNSSD_IF_ANY` or `CUPS_DNSSD_IF_LOCAL`
    const char            *fullname,	// I - Full DNS name including types and domain
    uint16_t              rrtype,	// I - Record type to query (`CUPS_DNSSD_RRTYPE_TXT`, etc.)
    cups_dnssd_query_cb_t query_cb,	// I - Query callback function
    void                  *cb_data)	// I - Query callback data
{
  cups_dnssd_query_t	*query;		// Query request


  // Range check input...
  if (!dnssd || !fullname || !query_cb)
    return (NULL);

  // Allocate memory for the queryr...
  if ((query = (cups_dnssd_query_t *)calloc(1, sizeof(cups_dnssd_query_t))) == NULL)
    return (NULL);

  query->dnssd   = dnssd;
  query->cb      = query_cb;
  query->cb_data = cb_data;

  cupsMutexLock(&dnssd->mutex);

  if (!dnssd->queries)
  {
    // Create an array of queryrs...
    if ((dnssd->queries = cupsArrayNew(NULL, NULL, NULL, 0, NULL, (cups_afree_cb_t)delete_query)) == NULL)
    {
      // Unable to create...
      free(query);
      query = NULL;
      goto done;
    }
  }

#if _WIN32

#elif defined(HAVE_MDNSRESPONDER)
  DNSServiceErrorType error;		// Error, if any

  query->ref = dnssd->ref;
  if ((error = DNSServiceQueryRecord(&query->ref, kDNSServiceFlagsShareConnection, if_index, fullname, rrtype, kDNSServiceClass_IN, (DNSServiceQueryRecordReply)mdns_query_cb, query)) != kDNSServiceErr_NoError)
  {
    report_error(dnssd, "Unable to create DNS-SD query request: %s", mdns_strerror(error));
    free(query);
    query = NULL;
    goto done;
  }

#else // HAVE_AVAHI

#endif // _WIN32

  cupsArrayAdd(dnssd->queries, query);

  done:

  cupsMutexUnlock(&dnssd->mutex);

  return (query);
}



//
// 'cupsDNSSDResolveDelete()' - Cancel and free a resolve request.
//

void
cupsDNSSDResolveDelete(
    cups_dnssd_resolve_t *res)		// I - Resolve request
{
  if (res)
  {
    cups_dnssd_t *dnssd = res->dnssd;

    cupsMutexLock(&dnssd->mutex);
    cupsArrayRemove(dnssd->resolves, res);
    cupsMutexUnlock(&dnssd->mutex);
  }
}


//
// 'cupsDNSSDResolveGetContext()' - Get the DNS-SD context for the resolve request.
//

cups_dnssd_t *				// O - DNS-SD context or `NULL`
cupsDNSSDResolveGetContext(
    cups_dnssd_resolve_t *resolve)	// I - Resolve request
{
  return (resolve ? resolve->dnssd : NULL);
}


//
// 'cupsDNSSDResolveNew()' - Create a new DNS-SD resolve request.
//

cups_dnssd_resolve_t *			// O - Resolve request or `NULL` on error
cupsDNSSDResolveNew(
    cups_dnssd_t            *dnssd,	// I - DNS-SD context
    uint32_t                if_index,	// I - Interface index or `CUPS_DNSSD_IF_ANY` or `CUPS_DNSSD_IF_LOCAL`
    const char              *name,	// I - Service name
    const char              *type,	// I - Service type
    const char              *domain,	// I - Domain name or `NULL` for default
    cups_dnssd_resolve_cb_t resolve_cb,	// I - Resolve callback function
    void                    *cb_data)	// I - Resolve callback data
{
  cups_dnssd_resolve_t	*resolve;	// Resolve request


  // Range check input...
  if (!dnssd || !name || !type || !resolve_cb)
    return (NULL);

  // Allocate memory for the queryr...
  if ((resolve = (cups_dnssd_resolve_t *)calloc(1, sizeof(cups_dnssd_resolve_t))) == NULL)
    return (NULL);

  resolve->dnssd   = dnssd;
  resolve->cb      = resolve_cb;
  resolve->cb_data = cb_data;

  cupsMutexLock(&dnssd->mutex);

  if (!dnssd->resolves)
  {
    // Create an array of queryrs...
    if ((dnssd->resolves = cupsArrayNew(NULL, NULL, NULL, 0, NULL, (cups_afree_cb_t)delete_resolve)) == NULL)
    {
      // Unable to create...
      free(resolve);
      resolve = NULL;
      goto done;
    }
  }

#if _WIN32

#elif defined(HAVE_MDNSRESPONDER)
  DNSServiceErrorType error;		// Error, if any

  resolve->ref = dnssd->ref;
  if ((error = DNSServiceResolve(&resolve->ref, kDNSServiceFlagsShareConnection, if_index, name, type, domain, (DNSServiceResolveReply)mdns_resolve_cb, resolve)) != kDNSServiceErr_NoError)
  {
    report_error(dnssd, "Unable to create DNS-SD query request: %s", mdns_strerror(error));
    free(resolve);
    resolve = NULL;
    goto done;
  }

#else // HAVE_AVAHI

#endif // _WIN32

  cupsArrayAdd(dnssd->resolves, resolve);

  done:

  cupsMutexUnlock(&dnssd->mutex);

  return (resolve);
}


//
// 'cupsDNSSDServiceAdd()' - Add a service instance.
//

bool					// O - `true` on success, `false` on failure
cupsDNSSDServiceAdd(
    cups_dnssd_service_t *service,	// I - Service
    uint32_t             if_index,	// I - Interface index or `CUPS_DNSSD_IF_ANY` or `CUPS_DNSSD_IF_LOCAL`
    const char           *types,	// I - Service types
    const char           *domain,	// I - Domain name or `NULL` for default
    const char           *host,		// I - Host name or `NULL` for default
    uint16_t             port,		// I - Port number or `0` for none
    size_t               num_txt,	// I - Number of TXT record values
    cups_option_t        *txt)		// I - TXT record values
{
  bool		ret = true;		// Return value
  size_t	i;			// Looping var


  // Range check input...
  if (!service || !types)
    return (false);

#if _WIN32

#elif defined(HAVE_MDNSRESPONDER)
  DNSServiceErrorType	error;		// Error, if any
  TXTRecordRef		txtrec,		// TXT record
			*txtptr = NULL;	// Pointer to TXT record, if any

  // Limit number of services with this name...
  if (service->num_refs >= (sizeof(service->refs) / sizeof(service->refs[0])))
  {
    report_error(service->dnssd, "Unable to create DNS-SD service registration: Too many services with this name.");
    ret = false;
    goto done;
  }

  // Create the TXT record as needed...
  if (num_txt)
  {
    TXTRecordCreate(&txtrec, 1024, NULL);
    for (i = 0; i < num_txt; i ++)
      TXTRecordSetValue(&txtrec, txt[i].name, (uint8_t)strlen(txt[i].value), txt[i].value);

    txtptr = &txtrec;
  }

  service->refs[service->num_refs] = service->dnssd->ref;
  if ((error = DNSServiceRegister(service->refs + service->num_refs, kDNSServiceFlagsShareConnection | kDNSServiceFlagsNoAutoRename, if_index, service->name, types, domain, host, htons(port), txtptr ? TXTRecordGetLength(txtptr) : 0, txtptr ? TXTRecordGetBytesPtr(txtptr) : NULL, (DNSServiceRegisterReply)mdns_service_cb, service)) != kDNSServiceErr_NoError)
  {
    if (txtptr)
      TXTRecordDeallocate(txtptr);

    report_error(service->dnssd, "Unable to create DNS-SD service registration: %s", mdns_strerror(error));
    ret = false;
    goto done;
  }

  if (txtptr)
    TXTRecordDeallocate(txtptr);

  if (service->loc_set)
  {
    if ((error = DNSServiceAddRecord(service->refs[service->num_refs], service->loc_refs + service->num_refs, 0, kDNSServiceType_LOC, sizeof(service->loc), service->loc, 0)) != kDNSServiceErr_NoError)
    {
      report_error(service->dnssd, "Unable to add DNS-SD service location data: %s", mdns_strerror(error));
    }
  }

  service->num_refs ++;

#else // HAVE_AVAHI

#endif // _WIN32

  done:

  return (ret);
}


//
// 'cupsDNSSDServiceDelete()' - Cancel and free a service registration.
//

void
cupsDNSSDServiceDelete(
    cups_dnssd_service_t *service)	// I - Service
{
  if (service)
  {
    cups_dnssd_t *dnssd = service->dnssd;

    cupsMutexLock(&dnssd->mutex);
    cupsArrayRemove(dnssd->services, service);
    cupsMutexUnlock(&dnssd->mutex);
  }
}


//
// 'cupsDNSSDServiceGetContext()' - Get the DNS-SD context for the service registration.
//

cups_dnssd_t *				// O - DNS-SD context or `NULL`
cupsDNSSDServiceGetContext(
    cups_dnssd_service_t *service)	// I - Service registration
{
  return (service ? service->dnssd : NULL);
}


//
// 'cupsDNSSDServiceNew()' - Create a new named service.
//

cups_dnssd_service_t *			// O - Service or `NULL` on error
cupsDNSSDServiceNew(
    cups_dnssd_t            *dnssd,	// I - DNS-SD context
    const char              *name,	// I - Name of service
    cups_dnssd_service_cb_t cb,		// I - Service registration callback function
    void                    *cb_data)	// I - Service registration callback data
{
  cups_dnssd_service_t	*service;	// Service registration


  // Range check input...
  if (!dnssd || !name || !cb)
    return (NULL);

  // Allocate memory for the queryr...
  if ((service = (cups_dnssd_service_t *)calloc(1, sizeof(cups_dnssd_service_t))) == NULL)
    return (NULL);

  service->dnssd   = dnssd;
  service->cb      = cb;
  service->cb_data = cb_data;
  service->name    = strdup(name);

  cupsMutexLock(&dnssd->mutex);

  if (!dnssd->services)
  {
    // Create an array of queryrs...
    if ((dnssd->services = cupsArrayNew(NULL, NULL, NULL, 0, NULL, (cups_afree_cb_t)delete_service)) == NULL)
    {
      // Unable to create...
      free(service);
      service = NULL;
      goto done;
    }
  }

  cupsArrayAdd(dnssd->services, service);

  done:

  cupsMutexUnlock(&dnssd->mutex);

  return (service);
}


//
// 'cupsDNSSDServicePublish()' - Publish a service.
//

bool					// O - `true` on success, `false` on failure
cupsDNSSDServicePublish(
    cups_dnssd_service_t *service)	// I - Service
{
  bool		ret = true;		// Return value


  (void)service;

  return (ret);
}


//
// 'cupsDNSSDServiceSetLocation()' - Set the geolocation (LOC record) of a
//                                   service.
//
// This function sets the geolocation of a service using a 'geo:' URI (RFC 5870)
// of the form
// 'geo:LATITUDE,LONGITUDE[,ALTITUDE][;crs=CRSLABEL][;u=UNCERTAINTY]'.  The
// specified coordinates and uncertainty are converted into a DNS LOC record
// for the service name label.
//
// Only the "wgs84" CRSLABEL string is supported.
//

bool					// O - `true` on success, `false` on failure
cupsDNSSDServiceSetLocation(
    cups_dnssd_service_t *service,	// I - Service
    const char           *geo_uri)	// I - Geolocation as a 'geo:' URI
{
  const char	*geo_ptr;		// Pointer into 'geo;' URI
  double	lat = 0.0, lon = 0.0;	// Latitude and longitude in degrees
  double	alt = 0.0;		// Altitude in meters
  double	u = 5.0;		// Uncertainty in meters
  unsigned int	lat_ksec, lon_ksec;	// Latitude and longitude in thousandths of arc seconds, biased by 2^31
  unsigned int	alt_cm;			// Altitude in centimeters, biased by 10,000,000cm
  unsigned char	prec;			// Precision value


  // See if this is a WGS-84 coordinate...
  if ((geo_ptr = strstr(geo_uri, ";crs=")) != NULL && strncmp(geo_ptr + 5, "wgs84", 5))
  {
    // Not WGS-84...
    _cupsSetError(IPP_STATUS_ERROR_INTERNAL, _("Only WGS-84 coordinates are supported."), true);
    return (false);
  }

  // Pull apart the "geo:" URI and convert to the integer representation for
  // the LOC record...
  sscanf(geo_uri, "geo:%lf,%lf,%lf", &lat, &lon, &alt);
  lat_ksec = (unsigned)((int)(lat * 3600000.0) + 0x40000000 + 0x40000000);
  lon_ksec = (unsigned)((int)(lon * 3600000.0) + 0x40000000 + 0x40000000);
  alt_cm   = (unsigned)((int)(alt * 100.0) + 10000000);

  if ((geo_ptr = strstr(geo_uri, ";u=")) != NULL)
    u = strtod(geo_ptr + 3, NULL);

  if (u < 0.0)
    u = 0.0;

  for (prec = 0, u = u * 100.0; u >= 10.0 && prec < 15; u = u * 0.01)
    prec ++;

  if (u < 10.0)
    prec |= (unsigned char)((int)u << 4);
  else
    prec |= (unsigned char)0x90;

  // Build the LOC record...
  service->loc[0]  = 0x00;		// Version
  service->loc[1]  = 0x51;		// Size (50cm)
  service->loc[2]  = prec;		// Horizontal precision
  service->loc[3]  = prec;		// Vertical precision

  service->loc[4]  = (unsigned char)(lat_ksec >> 24);
					// Latitude (32-bit big-endian)
  service->loc[5]  = (unsigned char)(lat_ksec >> 16);
  service->loc[6]  = (unsigned char)(lat_ksec >> 8);
  service->loc[7]  = (unsigned char)(lat_ksec);

  service->loc[8]  = (unsigned char)(lon_ksec >> 24);
					// Latitude (32-bit big-endian)
  service->loc[9]  = (unsigned char)(lon_ksec >> 16);
  service->loc[10] = (unsigned char)(lon_ksec >> 8);
  service->loc[11] = (unsigned char)(lon_ksec);

  service->loc[12] = (unsigned char)(alt_cm >> 24);
					// Altitude (32-bit big-endian)
  service->loc[13] = (unsigned char)(alt_cm >> 16);
  service->loc[14] = (unsigned char)(alt_cm >> 8);
  service->loc[15] = (unsigned char)(alt_cm);

  service->loc_set = true;

  return (true);
}


//
// 'delete_browse()' - Delete a browse request.
//

static void
delete_browse(
    cups_dnssd_browse_t *browse)	// I - Browse request
{
#if _WIN32
#elif defined(HAVE_MDNSRESPONDER)
  DNSServiceRefDeallocate(browse->ref);

#else // HAVE_AVAHI
#endif // _WIN32

  free(browse);
}


//
// 'delete_query()' - Delete a query request.
//

static void
delete_query(
    cups_dnssd_query_t *query)		// I - Query request
{
#if _WIN32

#elif defined(HAVE_MDNSRESPONDER)
  DNSServiceRefDeallocate(query->ref);

#else // HAVE_AVAHI

#endif // _WIN32

}


//
// 'delete_resolve()' - Delete a resolve request.
//

static void
delete_resolve(
    cups_dnssd_resolve_t *resolve)	// I - Resolve request
{
#if _WIN32

#elif defined(HAVE_MDNSRESPONDER)
  DNSServiceRefDeallocate(resolve->ref);

#else // HAVE_AVAHI

#endif // _WIN32

}


//
// 'delete_service()' - Delete a service registration.
//

static void
delete_service(
    cups_dnssd_service_t *service)	// I - Service
{
  free(service->name);

#if _WIN32
#elif defined(HAVE_MDNSRESPONDER)
  size_t	i;			// Looping var

  for (i = 0; i < service->num_refs; i ++)
    DNSServiceRefDeallocate(service->refs[i]);
#else // HAVE_AVAHI
#endif // _WIN32

  free(service);
}


#ifdef HAVE_MDNSRESPONDER
//
// 'mdns_browse_cb()' - Handle DNS-SD browse callbacks from mDNSResponder.
//

static void
mdns_browse_cb(
    DNSServiceRef       ref,		// I - Service reference
    DNSServiceFlags     flags,		// I - Browse flags
    uint32_t            if_index,	// I - Interface index
    DNSServiceErrorType error,		// I - Error code, if any
    const char          *name,		// I - Service name
    const char          *regtype,	// I - Registration type
    const char          *domain,	// I - Domain
    cups_dnssd_browse_t *browse)	// I - Browse request
{
  (void)ref;

  if (error != kDNSServiceErr_NoError)
    report_error(browse->dnssd, "DNS-SD browse error: %s", mdns_strerror(error));

  (browse->cb)(browse, browse->cb_data, mdns_to_cups(flags, error), if_index, name, regtype, domain);
}


//
// 'mdns_monitor()' - Monitor DNS-SD messages from mDNSResponder.
//

static void *				// O - Return value (always `NULL`)
mdns_monitor(cups_dnssd_t *dnssd)	// I - DNS-SD context
{
  DNSServiceErrorType	error;		// Current error

  for (;;)
  {
    if ((error = DNSServiceProcessResult(dnssd->ref)) != kDNSServiceErr_NoError)
    {
      report_error(dnssd, "Unable to read response from DNS-SD service: %s", mdns_strerror(error));
      break;
    }
  }

  return (NULL);
}


//
// 'mdns_query_cb()' - Handle DNS-SD query callbacks from mDNSResponder.
//

static void
mdns_query_cb(
    DNSServiceRef       ref,		// I - Service reference
    DNSServiceFlags     flags,		// I - Query flags
    uint32_t            if_index,	// I - Interface index
    DNSServiceErrorType error,		// I - Error code, if any
    const char          *name,		// I - Service name
    uint16_t            rrtype,		// I - Record type
    uint16_t            rrclass,	// I - Record class
    uint16_t            rdlen,		// I - Response length
    const void          *rdata,		// I - Response data
    uint32_t            ttl,		// I - Time-to-live value
    cups_dnssd_query_t  *query)		// I - Query request
{
  (void)ref;
  (void)rrclass;
  (void)ttl;

  if (error != kDNSServiceErr_NoError)
    report_error(query->dnssd, "DNS-SD query error: %s", mdns_strerror(error));

  (query->cb)(query, query->cb_data, mdns_to_cups(flags, error), if_index, name, rrtype, rdata, rdlen);
}


//
// 'mdns_resolve_cb()' - Handle DNS-SD resolution callbacks from mDNSResponder.
//

static void
mdns_resolve_cb(
    DNSServiceRef        ref,		// I - Service reference
    DNSServiceFlags      flags,		// I - Registration flags
    uint32_t             if_index,	// I - Interface index
    DNSServiceErrorType  error,		// I - Error code, if any
    const char           *fullname,	// I - Full name of service
    const char           *host,		// I - Hostname of service
    uint16_t             port,		// I - Port number in network byte order
    uint16_t             txtlen,	// I - Length of TXT record
    const unsigned char  *txtrec,	// I - TXT record
    cups_dnssd_resolve_t *resolve)	// I - Resolve request
{
  size_t	num_txt;		// Number of TXT key/value pairs
  cups_option_t	*txt;			// TXT key/value pairs


  (void)ref;

  if (error != kDNSServiceErr_NoError)
    report_error(resolve->dnssd, "DNS-SD resolve error: %s", mdns_strerror(error));

  num_txt = cupsDNSSDDecodeTXT(txtrec, txtlen, &txt);

  (resolve->cb)(resolve, resolve->cb_data, mdns_to_cups(flags, error), if_index, fullname, host, ntohs(port), num_txt, txt);

  cupsFreeOptions(num_txt, txt);
}


//
// 'mdns_service_cb()' - Handle DNS-SD service registration callbacks from
//                       mDNSResponder.
//

static void
mdns_service_cb(
    DNSServiceRef        ref,		// I - Service reference
    DNSServiceFlags      flags,		// I - Registration flags
    DNSServiceErrorType  error,		// I - Error code, if any
    const char           *name,		// I - Service name
    const char           *regtype,	// I - Registration type
    const char           *domain,	// I - Domain
    cups_dnssd_service_t *service)	// I - Service registration
{
  (void)ref;

  if (error != kDNSServiceErr_NoError)
    report_error(service->dnssd, "DNS-SD service registration error: %s", mdns_strerror(error));

  (service->cb)(service, service->cb_data, mdns_to_cups(flags, error), name, regtype, domain);
}


//
// 'mdns_strerror()' - Convert an error code to a string.
//

static const char *			// O - Error message
mdns_strerror(
    DNSServiceErrorType errorCode)	// I - Error code
{
  switch (errorCode)
  {
    case kDNSServiceErr_NoError :
        return ("No error");

    case kDNSServiceErr_Unknown :
    default :
        return ("Unknown error");

    case kDNSServiceErr_NoSuchName :
        return ("Name not found");

    case kDNSServiceErr_NoMemory :
        return ("Out of memory");

    case kDNSServiceErr_BadParam :
        return ("Bad parameter");

    case kDNSServiceErr_BadReference :
        return ("Bad service reference");

    case kDNSServiceErr_BadState :
        return ("Bad state");

    case kDNSServiceErr_BadFlags :
        return ("Bad flags argument");

    case kDNSServiceErr_Unsupported :
        return ("Unsupported feature");

    case kDNSServiceErr_NotInitialized :
        return ("Not initialized");

    case kDNSServiceErr_AlreadyRegistered :
        return ("Name already registered");

    case kDNSServiceErr_NameConflict :
        return ("Name conflicts");

    case kDNSServiceErr_Invalid :
        return ("Invalid argument");

    case kDNSServiceErr_Firewall :
        return ("Firewall prevents access");

    case kDNSServiceErr_Incompatible :
        return ("Client library incompatible with background daemon");

    case kDNSServiceErr_BadInterfaceIndex :
        return ("Bad interface index");

    case kDNSServiceErr_Refused :
        return ("Connection refused");

    case kDNSServiceErr_NoSuchRecord :
        return ("DNS record not found");

    case kDNSServiceErr_NoAuth :
        return ("No authoritative answer");

    case kDNSServiceErr_NoSuchKey :
        return ("TXT record key not found");

    case kDNSServiceErr_NATTraversal :
        return ("Unable to traverse via NAT");

    case kDNSServiceErr_DoubleNAT :
        return ("Double NAT is in use");

    case kDNSServiceErr_BadTime :
        return ("Bad time value");

    case kDNSServiceErr_BadSig :
        return ("Bad signal");

    case kDNSServiceErr_BadKey :
        return ("Bad TXT record key");

    case kDNSServiceErr_Transient :
        return ("Transient error");

    case kDNSServiceErr_ServiceNotRunning :
        return ("Background daemon not running");

    case kDNSServiceErr_NATPortMappingUnsupported :
        return ("NAT doesn't support PCP, NAT-PMP or UPnP");

    case kDNSServiceErr_NATPortMappingDisabled :
        return ("NAT supports PCP, NAT-PMP or UPnP, but it's disabled by the administrator");

    case kDNSServiceErr_NoRouter :
        return ("No router configured, probably no network connectivity");

    case kDNSServiceErr_PollingMode :
        return ("Polling error");

    case kDNSServiceErr_Timeout :
        return ("Timeout");

    case kDNSServiceErr_DefunctConnection :
        return ("Connection lost");
  }
}


//
// 'mdns_to_cups()' - Convert mDNSResponder flags to CUPS DNS-SD flags...
//

static cups_dnssd_flags_t		// O - CUPS DNS-SD flags
mdns_to_cups(
    DNSServiceFlags     flags,		// I - mDNSResponder flags
    DNSServiceErrorType error)		// I - mDNSResponder error code
{
  cups_dnssd_flags_t	cups_flags = CUPS_DNSSD_FLAGS_NONE;
					// CUPS DNS-SD flags


  if (flags & kDNSServiceFlagsAdd)
    cups_flags |= CUPS_DNSSD_FLAGS_ADD;
  if (flags & kDNSServiceFlagsMoreComing)
    cups_flags |= CUPS_DNSSD_FLAGS_MORE;
  if (error != kDNSServiceErr_NoError)
    cups_flags |= CUPS_DNSSD_FLAGS_ERROR;

  return (cups_flags);
}
#endif // HAVE_MDNSRESPONDER


//
// 'report_error()' - Report an error.
//

static void
report_error(cups_dnssd_t *dnssd,	// I - DNS-SD context
             const char   *message,	// I - printf-style message string
             ...)			// I - Additional arguments as needed
{
  va_list	ap;			// Pointer to arguments
  char		buffer[8192];		// Formatted message


  // Format the message...
  va_start(ap, message);
  vsnprintf(buffer, sizeof(buffer), message, ap);
  va_end(ap);

  // Send it...
  if (dnssd->cb)
    (dnssd->cb)(dnssd->cb_data, buffer);
  else
    fprintf(stderr, "%s\n", buffer);
}
