//
// User-defined destination (and option) support for CUPS.
//
// Copyright © 2021-2023 by OpenPrinting.
// Copyright © 2007-2019 by Apple Inc.
// Copyright © 1997-2007 by Easy Software Products.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#include "cups-private.h"
#include "dir.h"

#ifdef __APPLE__
#  include <notify.h>
#endif // __APPLE__

#ifndef _WIN32
#  include <poll.h>
#endif // !_WIN32

#include "dnssd.h"


//
// Constants...
//

#ifdef __APPLE__
#  if HAVE_SCDYNAMICSTORECOPYCOMPUTERNAME
#    include <SystemConfiguration/SystemConfiguration.h>
#    define _CUPS_LOCATION_DEFAULTS 1
#  endif // HAVE_SCDYNAMICSTORECOPYCOMPUTERNAME
#  define kCUPSPrintingPrefs	CFSTR("org.cups.PrintingPrefs")
#  define kDefaultPaperIDKey	CFSTR("DefaultPaperID")
#  define kLastUsedPrintersKey	CFSTR("LastUsedPrinters")
#  define kLocationNetworkKey	CFSTR("Network")
#  define kLocationPrinterIDKey	CFSTR("PrinterID")
#  define kUseLastPrinter	CFSTR("UseLastPrinter")
#endif // __APPLE__

#define _CUPS_DNSSD_GET_DESTS	250	// Milliseconds for cupsGetDests
#define _CUPS_DNSSD_MAXTIME	50	// Milliseconds for maximum quantum of time


//
// Types...
//

typedef enum _cups_dnssd_state_e	// Enumerated device state
{
  _CUPS_DNSSD_NEW,
  _CUPS_DNSSD_QUERY,
  _CUPS_DNSSD_PENDING,
  _CUPS_DNSSD_ACTIVE,
  _CUPS_DNSSD_INCOMPATIBLE,
  _CUPS_DNSSD_ERROR
} _cups_dnssd_state_t;

typedef struct _cups_dnssd_data_s	// Enumeration data
{
  cups_rwlock_t		rwlock;		// Reader/writer lock
  cups_dnssd_t		*dnssd;		// DNS-SD context
  cups_dest_cb_t	cb;		// Callback
  void			*user_data;	// User data pointer
  cups_ptype_t		type,		// Printer type filter
			mask;		// Printer type mask
  cups_array_t		*devices;	// Devices found so far
  size_t		num_dests;	// Number of lpoptions destinations
  cups_dest_t		*dests;		// lpoptions destinations
  char			def_name[1024],	// Default printer name, if any
			*def_instance;	// Default printer instance, if any
} _cups_dnssd_data_t;

typedef struct _cups_dnssd_device_s	// Enumerated device
{
  _cups_dnssd_state_t	state;		// State of device listing
  cups_dnssd_query_t	*query;		// DNS-SD query request
  char			*fullname,	// Full name
			*regtype,	// Registration type
			*domain;	// Domain name
  cups_ptype_t		type;		// Device registration type
  cups_dest_t		dest;		// Destination record
} _cups_dnssd_device_t;

typedef struct _cups_dnssd_resdata_s	// Data for resolving URI
{
  int			*cancel;	// Pointer to "cancel" variable
  struct timeval	end_time;	// Ending time
} _cups_dnssd_resdata_t;

typedef struct _cups_getdata_s
{
  size_t		num_dests;	// Number of destinations
  cups_dest_t		*dests;		// Destinations
  char			def_name[1024],	// Default printer name, if any
			*def_instance;	// Default printer instance, if any
} _cups_getdata_t;

typedef struct _cups_namedata_s
{
  const char  *name;                    // Named destination
  cups_dest_t *dest;                    // Destination
} _cups_namedata_t;


//
// Local functions...
//

#if _CUPS_LOCATION_DEFAULTS
static CFArrayRef	appleCopyLocations(void);
static CFStringRef	appleCopyNetwork(void);
#endif // _CUPS_LOCATION_DEFAULTS
#ifdef __APPLE__
static char		*appleGetPaperSize(char *name, size_t namesize);
#endif // __APPLE__
#if _CUPS_LOCATION_DEFAULTS
static CFStringRef	appleGetPrinter(CFArrayRef locations, CFStringRef network, CFIndex *locindex);
#endif // _CUPS_LOCATION_DEFAULTS
static cups_dest_t	*cups_add_dest(const char *name, const char *instance, size_t *num_dests, cups_dest_t **dests);
static int		cups_compare_dests(cups_dest_t *a, cups_dest_t *b);
static void		cups_dest_browse_cb(cups_dnssd_browse_t *browse, void *cb_data, cups_dnssd_flags_t flags, uint32_t if_index, const char *name, const char *regtype, const char *domain);
static int		cups_dnssd_compare_devices(_cups_dnssd_device_t *a, _cups_dnssd_device_t *b);
static void		cups_dnssd_free_device(_cups_dnssd_device_t *device, _cups_dnssd_data_t *data);
static _cups_dnssd_device_t *cups_dnssd_get_device(_cups_dnssd_data_t *data, const char *serviceName, const char *regtype, const char *replyDomain);
static void		cups_dest_query_cb(cups_dnssd_query_t *query, void *cb_data, cups_dnssd_flags_t flags, uint32_t if_index, const char *fullname, uint16_t rrtype, const void *qdata, uint16_t qlen);
static const char	*cups_dest_resolve(cups_dest_t *dest, const char *uri, int msec, int *cancel, cups_dest_cb_t cb, void *user_data);
static bool		cups_dest_resolve_cb(void *context);
static void		cups_dnssd_unquote(char *dst, const char *src, size_t dstsize);
static int		cups_elapsed(struct timeval *t);
static bool		cups_enum_dests(http_t *http, unsigned flags, int msec, int *cancel, cups_ptype_t type, cups_ptype_t mask, cups_dest_cb_t cb, void *user_data);
static size_t		cups_find_dest(const char *name, const char *instance, size_t num_dests, cups_dest_t *dests, size_t prev, int *rdiff);
static bool		cups_get_cb(_cups_getdata_t *data, unsigned flags, cups_dest_t *dest);
static char		*cups_get_default(const char *filename, char *namebuf, size_t namesize, const char **instance);
static size_t		cups_get_dests(const char *filename, const char *match_name, const char *match_inst, bool load_all, bool user_default_set, size_t num_dests, cups_dest_t **dests);
static void		cups_init_profiles(_cups_profiles_t *profiles);
static void		cups_load_profile(_cups_profiles_t *profiles, const char *filename);
static char		*cups_make_string(ipp_attribute_t *attr, char *buffer, size_t bufsize);
static bool		cups_name_cb(_cups_namedata_t *data, unsigned flags, cups_dest_t *dest);
static void		cups_queue_name(char *name, const char *serviceName, size_t namesize);
static void		cups_scan_profiles(cups_array_t *proflist, const char *path, time_t *mtime);
static void		cups_update_profiles(_cups_globals_t *cg, int *cancel);
static void		dnssd_error_cb(void *cb_data, const char *message);


//
// 'cupsAddDest()' - Add a destination to the list of destinations.
//
// This function cannot be used to add a new class or printer queue,
// it only adds a new container of saved options for the named
// destination or instance.
//
// If the named destination already exists, the destination list is
// returned unchanged.  Adding a new instance of a destination creates
// a copy of that destination's options.
//
// Use the @link cupsSaveDests@ function to save the updated list of
// destinations to the user's lpoptions file.
//

size_t					// O  - New number of destinations
cupsAddDest(const char  *name,		// I  - Destination name
            const char	*instance,	// I  - Instance name or `NULL` for none/primary
            size_t      num_dests,	// I  - Number of destinations
            cups_dest_t **dests)	// IO - Destinations
{
  int		i;			// Looping var
  cups_dest_t	*dest;			// Destination pointer
  cups_dest_t	*parent = NULL;		// Parent destination
  cups_option_t	*doption,		// Current destination option
		*poption;		// Current parent option


  if (!name || !dests)
    return (0);

  if (!cupsGetDest(name, instance, num_dests, *dests))
  {
    if (instance && !cupsGetDest(name, NULL, num_dests, *dests))
    {
      // Add destination first...
      if (!cups_add_dest(name, NULL, &num_dests, dests))
        return (num_dests);
    }

    if ((dest = cups_add_dest(name, instance, &num_dests, dests)) == NULL)
      return (num_dests);

    // Find the base dest again now the array has been realloc'd.
    parent = cupsGetDest(name, NULL, num_dests, *dests);

    if (instance && parent && parent->num_options > 0)
    {
      // Copy options from parent...
      dest->options = calloc(sizeof(cups_option_t), (size_t)parent->num_options);

      if (dest->options)
      {
        dest->num_options = parent->num_options;

	for (i = dest->num_options, doption = dest->options, poption = parent->options; i > 0; i --, doption ++, poption ++)
	{
	  doption->name  = _cupsStrRetain(poption->name);
	  doption->value = _cupsStrRetain(poption->value);
	}
      }
    }
  }

  return (num_dests);
}


#ifdef __APPLE__
//
// '_cupsAppleCopyDefaultPaperID()' - Get the default paper ID.
//

CFStringRef				// O - Default paper ID
_cupsAppleCopyDefaultPaperID(void)
{
  return (CFPreferencesCopyAppValue(kDefaultPaperIDKey,
                                    kCUPSPrintingPrefs));
}


//
// '_cupsAppleCopyDefaultPrinter()' - Get the default printer at this location.
//

CFStringRef				// O - Default printer name
_cupsAppleCopyDefaultPrinter(void)
{
#  if _CUPS_LOCATION_DEFAULTS
  CFStringRef	network;		// Network location
  CFArrayRef	locations;		// Location array
  CFStringRef	locprinter;		// Current printer


  // Use location-based defaults only if "use last printer" is selected in the
  // system preferences...
  if (!_cupsAppleGetUseLastPrinter())
  {
    DEBUG_puts("1_cupsAppleCopyDefaultPrinter: Not using last printer as default.");
    return (NULL);
  }

  // Get the current location...
  if ((network = appleCopyNetwork()) == NULL)
  {
    DEBUG_puts("1_cupsAppleCopyDefaultPrinter: Unable to get current network.");
    return (NULL);
  }

  // Lookup the network in the preferences...
  if ((locations = appleCopyLocations()) == NULL)
  {
    // Missing or bad location array, so no location-based default...
    DEBUG_puts("1_cupsAppleCopyDefaultPrinter: Missing or bad last used printer array.");

    CFRelease(network);

    return (NULL);
  }

  DEBUG_printf("1_cupsAppleCopyDefaultPrinter: Got locations, %d entries.", (int)CFArrayGetCount(locations));

  if ((locprinter = appleGetPrinter(locations, network, NULL)) != NULL)
    CFRetain(locprinter);

  CFRelease(network);
  CFRelease(locations);

  return (locprinter);

#  else
  return (NULL);
#  endif // _CUPS_LOCATION_DEFAULTS
}


//
// '_cupsAppleGetUseLastPrinter()' - Get whether to use the last used printer.
//

bool					// O - `true` to use last printer, `false` otherwise
_cupsAppleGetUseLastPrinter(void)
{
  Boolean	uselast,		// Use last printer preference value
		uselast_set;		// Valid is set?


  if (getenv("CUPS_DISABLE_APPLE_DEFAULT"))
    return (false);

  uselast = CFPreferencesGetAppBooleanValue(kUseLastPrinter, kCUPSPrintingPrefs, &uselast_set);
  if (!uselast_set)
    return (true);
  else
    return ((bool)uselast);
}


//
// '_cupsAppleSetDefaultPaperID()' - Set the default paper id.
//

void
_cupsAppleSetDefaultPaperID(
    CFStringRef name)			// I - New paper ID
{
  CFPreferencesSetAppValue(kDefaultPaperIDKey, name, kCUPSPrintingPrefs);
  CFPreferencesAppSynchronize(kCUPSPrintingPrefs);

  notify_post("com.apple.printerPrefsChange");
}


//
// '_cupsAppleSetDefaultPrinter()' - Set the default printer for this location.
//

void
_cupsAppleSetDefaultPrinter(
    CFStringRef name)			// I - Default printer/class name
{
#  if _CUPS_LOCATION_DEFAULTS
  CFStringRef		network;	// Current network
  CFArrayRef		locations;	// Old locations array
  CFIndex		locindex;	// Index in locations array
  CFStringRef		locprinter;	// Current printer
  CFMutableArrayRef	newlocations;	// New locations array
  CFMutableDictionaryRef newlocation;	// New location


  // Get the current location...
  if ((network = appleCopyNetwork()) == NULL)
  {
    DEBUG_puts("1_cupsAppleSetDefaultPrinter: Unable to get current network...");
    return;
  }

  // Lookup the network in the preferences...
  if ((locations = appleCopyLocations()) != NULL)
  {
    locprinter = appleGetPrinter(locations, network, &locindex);
  }
  else
  {
    locprinter = NULL;
    locindex   = -1;
  }

  if (!locprinter || CFStringCompare(locprinter, name, 0) != kCFCompareEqualTo)
  {
    // Need to change the locations array...
    if (locations)
    {
      newlocations = CFArrayCreateMutableCopy(kCFAllocatorDefault, 0, locations);

      if (locprinter)
        CFArrayRemoveValueAtIndex(newlocations, locindex);
    }
    else
    {
      newlocations = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
    }

    newlocation = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

    if (newlocation && newlocations)
    {
      // Put the new location at the front of the array...
      CFDictionaryAddValue(newlocation, kLocationNetworkKey, network);
      CFDictionaryAddValue(newlocation, kLocationPrinterIDKey, name);
      CFArrayInsertValueAtIndex(newlocations, 0, newlocation);

      // Limit the number of locations to 10...
      while (CFArrayGetCount(newlocations) > 10)
        CFArrayRemoveValueAtIndex(newlocations, 10);

      // Push the changes out...
      CFPreferencesSetAppValue(kLastUsedPrintersKey, newlocations, kCUPSPrintingPrefs);
      CFPreferencesAppSynchronize(kCUPSPrintingPrefs);

      notify_post("com.apple.printerPrefsChange");
    }

    if (newlocations)
      CFRelease(newlocations);

    if (newlocation)
      CFRelease(newlocation);
  }

  if (locations)
    CFRelease(locations);

  CFRelease(network);

#  else
  (void)name;
#  endif // _CUPS_LOCATION_DEFAULTS
}


//
// '_cupsAppleSetUseLastPrinter()' - Set whether to use the last used printer.
//

void
_cupsAppleSetUseLastPrinter(
    int uselast)			// O - 1 to use last printer, 0 otherwise
{
  CFPreferencesSetAppValue(kUseLastPrinter,
			   uselast ? kCFBooleanTrue : kCFBooleanFalse,
			   kCUPSPrintingPrefs);
  CFPreferencesAppSynchronize(kCUPSPrintingPrefs);

  notify_post("com.apple.printerPrefsChange");
}
#endif // __APPLE__


//
// 'cupsConnectDest()' - Open a connection to the destination.
//
// Connect to the destination, returning a new `http_t` connection object
// and optionally the resource path to use for the destination.  These calls
// will block until a connection is made, the timeout expires, the integer
// pointed to by "cancel" is non-zero, or the callback function (or block)
// returns `0`.  The caller is responsible for calling @link httpClose@ on the
// returned connection.
//
// The caller can pass `CUPS_DEST_FLAGS_DEVICE` for the "flags" argument to
// connect directly to the device associated with the destination.  Otherwise,
// the connection is made to the CUPS scheduler associated with the destination.
//

http_t *				// O - Connection to destination or `NULL`
cupsConnectDest(
    cups_dest_t    *dest,		// I - Destination
    unsigned       flags,		// I - Connection flags
    int            msec,		// I - Timeout in milliseconds
    int            *cancel,		// I - Pointer to "cancel" variable
    char           *resource,		// I - Resource buffer
    size_t         resourcesize,	// I - Size of resource buffer
    cups_dest_cb_t cb,			// I - Callback function
    void           *user_data)		// I - User data pointer
{
  const char	*uri;			// Printer URI
  char		scheme[32],		// URI scheme
		userpass[256],		// Username and password (unused)
		hostname[256],		// Hostname
		tempresource[1024];	// Temporary resource buffer
  int		port;			// Port number
  char		portstr[16];		// Port number string
  http_encryption_t encryption;		// Encryption to use
  http_addrlist_t *addrlist;		// Address list for server
  http_t	*http;			// Connection to server


  DEBUG_printf("cupsConnectDest(dest=%p, flags=0x%x, msec=%d, cancel=%p(%d), resource=\"%s\", resourcesize=" CUPS_LLFMT ", cb=%p, user_data=%p)", (void *)dest, flags, msec, (void *)cancel, cancel ? *cancel : -1, resource, CUPS_LLCAST resourcesize, (void *)cb, user_data);

  // Range check input...
  if (!dest)
  {
    if (resource)
      *resource = '\0';

    _cupsSetError(IPP_STATUS_ERROR_INTERNAL, strerror(EINVAL), 0);
    return (NULL);
  }

  if (!resource || resourcesize < 1)
  {
    resource     = tempresource;
    resourcesize = sizeof(tempresource);
  }

  // Grab the printer URI...
  if (flags & CUPS_DEST_FLAGS_DEVICE)
  {
    if ((uri = cupsGetOption("device-uri", dest->num_options, dest->options)) != NULL)
    {
#ifdef HAVE_DNSSD
      if (strstr(uri, "._tcp"))
        uri = cups_dest_resolve(dest, uri, msec, cancel, cb, user_data);
#endif // HAVE_DNSSD
    }
  }
  else if ((uri = cupsGetOption("printer-uri-supported", dest->num_options, dest->options)) == NULL)
  {
    if ((uri = cupsGetOption("device-uri", dest->num_options, dest->options)) != NULL)
    {
#ifdef HAVE_DNSSD
      if (strstr(uri, "._tcp"))
        uri = cups_dest_resolve(dest, uri, msec, cancel, cb, user_data);
#endif // HAVE_DNSSD
    }

    if (uri)
      uri = _cupsCreateDest(dest->name, cupsGetOption("printer-info", dest->num_options, dest->options), NULL, uri, tempresource, sizeof(tempresource));

    if (uri)
    {
      dest->num_options = cupsAddOption("printer-uri-supported", uri, dest->num_options, &dest->options);

      uri = cupsGetOption("printer-uri-supported", dest->num_options, dest->options);
    }
  }

  if (!uri)
  {
    _cupsSetError(IPP_STATUS_ERROR_INTERNAL, strerror(ENOENT), 0);

    if (cb)
      (*cb)(user_data, CUPS_DEST_FLAGS_UNCONNECTED | CUPS_DEST_FLAGS_ERROR, dest);

    return (NULL);
  }

  if (httpSeparateURI(HTTP_URI_CODING_ALL, uri, scheme, sizeof(scheme), userpass, sizeof(userpass), hostname, sizeof(hostname), &port, resource, resourcesize) < HTTP_URI_STATUS_OK)
  {
    _cupsSetError(IPP_STATUS_ERROR_INTERNAL, _("Bad printer-uri."), 1);

    if (cb)
      (*cb)(user_data, CUPS_DEST_FLAGS_UNCONNECTED | CUPS_DEST_FLAGS_ERROR, dest);

    return (NULL);
  }

  // Lookup the address for the server...
  if (cb)
    (*cb)(user_data, CUPS_DEST_FLAGS_UNCONNECTED | CUPS_DEST_FLAGS_RESOLVING, dest);

  snprintf(portstr, sizeof(portstr), "%d", port);

  if ((addrlist = httpAddrGetList(hostname, AF_UNSPEC, portstr)) == NULL)
  {
    if (cb)
      (*cb)(user_data, CUPS_DEST_FLAGS_UNCONNECTED | CUPS_DEST_FLAGS_ERROR, dest);

    return (NULL);
  }

  if (cancel && *cancel)
  {
    httpAddrFreeList(addrlist);

    if (cb)
      (*cb)(user_data, CUPS_DEST_FLAGS_UNCONNECTED | CUPS_DEST_FLAGS_CANCELED, dest);

    return (NULL);
  }

  // Create the HTTP object pointing to the server referenced by the URI...
  if (!strcmp(scheme, "ipps") || port == 443)
    encryption = HTTP_ENCRYPTION_ALWAYS;
  else
    encryption = HTTP_ENCRYPTION_IF_REQUESTED;

  http = httpConnect(hostname, port, addrlist, AF_UNSPEC, encryption, 1, 0, NULL);
  httpAddrFreeList(addrlist);

  // Connect if requested...
  if (flags & CUPS_DEST_FLAGS_UNCONNECTED)
  {
    if (cb)
      (*cb)(user_data, CUPS_DEST_FLAGS_UNCONNECTED, dest);
  }
  else
  {
    if (cb)
      (*cb)(user_data, CUPS_DEST_FLAGS_UNCONNECTED | CUPS_DEST_FLAGS_CONNECTING, dest);

    if (httpReconnect(http, msec, cancel) && cb)
    {
      if (cancel && *cancel)
	(*cb)(user_data, CUPS_DEST_FLAGS_UNCONNECTED | CUPS_DEST_FLAGS_CONNECTING, dest);
      else
	(*cb)(user_data, CUPS_DEST_FLAGS_UNCONNECTED | CUPS_DEST_FLAGS_ERROR, dest);
    }
    else if (cb)
      (*cb)(user_data, CUPS_DEST_FLAGS_NONE, dest);
  }

  return (http);
}


//
// 'cupsCopyDest()' - Copy a destination.
//
// Make a copy of the destination to an array of destinations (or just a single
// copy) - for use with the `cupsEnumDests*` functions. The caller is
// responsible for calling @link cupsFreeDests@ on the returned object(s).
//

size_t					// O  - New number of destinations
cupsCopyDest(cups_dest_t *dest,		// I  - Destination to copy
             size_t      num_dests,	// I  - Number of destinations
             cups_dest_t **dests)	// IO - Destination array
{
  size_t	i;			// Looping var
  cups_dest_t	*new_dest;		// New destination pointer
  cups_option_t	*new_option,		// Current destination option
		*option;		// Current parent option


  // Range check input...
  if (!dest || !dests)
    return (num_dests);

  // See if the destination already exists...
  if ((new_dest = cupsGetDest(dest->name, dest->instance, num_dests, *dests)) != NULL)
  {
    // Protect against copying destination to itself...
    if (new_dest == dest)
      return (num_dests);

    // Otherwise, free the options...
    cupsFreeOptions(new_dest->num_options, new_dest->options);

    new_dest->num_options = 0;
    new_dest->options     = NULL;
  }
  else
  {
    new_dest = cups_add_dest(dest->name, dest->instance, &num_dests, dests);
  }

  if (new_dest)
  {
    new_dest->is_default = dest->is_default;

    if ((new_dest->options = calloc(sizeof(cups_option_t), (size_t)dest->num_options)) == NULL)
      return (cupsRemoveDest(dest->name, dest->instance, num_dests, dests));

    new_dest->num_options = dest->num_options;

    for (i = dest->num_options, option = dest->options, new_option = new_dest->options; i > 0; i --, option ++, new_option ++)
    {
      new_option->name  = _cupsStrRetain(option->name);
      new_option->value = _cupsStrRetain(option->value);
    }
  }

  return (num_dests);
}


//
// '_cupsCreateDest()' - Create a local (temporary) queue.
//

char *					// O - Printer URI or `NULL` on error
_cupsCreateDest(const char *name,	// I - Printer name
                const char *info,	// I - Printer description of `NULL`
		const char *device_id,	// I - 1284 Device ID or `NULL`
		const char *device_uri,	// I - Device URI
		char       *uri,	// I - Printer URI buffer
		size_t     urisize)	// I - Size of URI buffer
{
  http_t	*http;			// Connection to server
  ipp_t		*request,		// CUPS-Create-Local-Printer request
		*response;		// CUPS-Create-Local-Printer response
  ipp_attribute_t *attr;		// printer-uri-supported attribute
  ipp_pstate_t	state = IPP_PSTATE_STOPPED;
					// printer-state value


  if (!name || !device_uri || !uri || urisize < 32)
    return (NULL);

  if ((http = httpConnect(cupsGetServer(), ippGetPort(), NULL, AF_UNSPEC, HTTP_ENCRYPTION_IF_REQUESTED, 1, 30000, NULL)) == NULL)
    return (NULL);

  request = ippNewRequest(IPP_OP_CUPS_CREATE_LOCAL_PRINTER);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri", NULL, "ipp://localhost/");
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name", NULL, cupsGetUser());

  ippAddString(request, IPP_TAG_PRINTER, IPP_TAG_URI, "device-uri", NULL, device_uri);
  ippAddString(request, IPP_TAG_PRINTER, IPP_TAG_NAME, "printer-name", NULL, name);
  if (info)
    ippAddString(request, IPP_TAG_PRINTER, IPP_TAG_TEXT, "printer-info", NULL, info);
  if (device_id)
    ippAddString(request, IPP_TAG_PRINTER, IPP_TAG_TEXT, "printer-device-id", NULL, device_id);

  response = cupsDoRequest(http, request, "/");

  if ((attr = ippFindAttribute(response, "printer-uri-supported", IPP_TAG_URI)) != NULL)
    cupsCopyString(uri, ippGetString(attr, 0, NULL), urisize);
  else
  {
    ippDelete(response);
    httpClose(http);
    return (NULL);
  }

  if ((attr = ippFindAttribute(response, "printer-state", IPP_TAG_ENUM)) != NULL)
    state = (ipp_pstate_t)ippGetInteger(attr, 0);

  while (state == IPP_PSTATE_STOPPED && cupsGetError() == IPP_STATUS_OK)
  {
    sleep(1);
    ippDelete(response);

    request = ippNewRequest(IPP_OP_GET_PRINTER_ATTRIBUTES);

    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri", NULL, uri);
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name", NULL, cupsGetUser());
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD, "requested-attributes", NULL, "printer-state");

    response = cupsDoRequest(http, request, "/");

    if ((attr = ippFindAttribute(response, "printer-state", IPP_TAG_ENUM)) != NULL)
      state = (ipp_pstate_t)ippGetInteger(attr, 0);
  }

  ippDelete(response);

  httpClose(http);

  return (uri);
}


//
// 'cupsEnumDests()' - Enumerate available destinations with a callback function.
//
// Destinations are enumerated from one or more sources.  The callback function
// receives the "user_data" pointer and the destination pointer which can be
// used as input to the @link cupsCopyDest@ function.  The function must return
// `true` to continue enumeration or `false` to stop.
//
// The "type" and "mask" arguments allow the caller to filter the destinations
// that are enumerated.  Passing `0` for both will enumerate all printers.  The
// constant `CUPS_PRINTER_DISCOVERED` is used to filter on destinations that are
// available but have not yet been added locally.
//
// Enumeration happens on the current thread and does not return until all
// destinations have been enumerated or the callback function returns `false`.
//
// Note: The callback function will likely receive multiple updates for the same
// destinations - it is up to the caller to suppress any duplicate destinations.
//

bool					// O - `true` on success, `false` on failure
cupsEnumDests(
  unsigned       flags,			// I - Enumeration flags
  int            msec,			// I - Timeout in milliseconds, -1 for indefinite
  int            *cancel,		// I - Pointer to "cancel" variable
  cups_ptype_t   type,			// I - Printer type bits
  cups_ptype_t   mask,			// I - Mask for printer type bits
  cups_dest_cb_t cb,			// I - Callback function
  void           *user_data)		// I - User data
{
  return (cups_enum_dests(CUPS_HTTP_DEFAULT, flags, msec, cancel, type, mask, cb, user_data));
}


//
// 'cupsFreeDests()' - Free the memory used by the list of destinations.
//

void
cupsFreeDests(size_t      num_dests,	// I - Number of destinations
              cups_dest_t *dests)	// I - Destinations
{
  size_t	i;			// Looping var
  cups_dest_t	*dest;			// Current destination


  if (num_dests == 0 || dests == NULL)
    return;

  for (i = num_dests, dest = dests; i > 0; i --, dest ++)
  {
    _cupsStrFree(dest->name);
    _cupsStrFree(dest->instance);

    cupsFreeOptions(dest->num_options, dest->options);
  }

  free(dests);
}


//
// '_cupsFreeProfiles()' - Free profiles.
//

void
_cupsFreeProfiles(
    _cups_profiles_t *profiles)		// I - Profiles
{
  cupsArrayDelete(profiles->prof_locations);
  cupsArrayDelete(profiles->prof_printers);
  cupsArrayDelete(profiles->prof_servers);
  cupsArrayDelete(profiles->prof_systems);
}


//
// 'cupsGetDest()' - Get the named destination from the list.
//
// This function gets the named destination from the list of destinations.  Use
// the @link cupsEnumDests@ or @link cupsGetDests@ functions to get a list of
// supported destinations for the current user.
//

cups_dest_t *				// O - Destination pointer or `NULL`
cupsGetDest(const char  *name,		// I - Destination name or `NULL` for the default destination
            const char	*instance,	// I - Instance name or `NULL`
            size_t      num_dests,	// I - Number of destinations
            cups_dest_t *dests)		// I - Destinations
{
  int	diff,				// Result of comparison
	match;				// Matching index


  if (num_dests <= 0 || !dests)
    return (NULL);

  if (!name)
  {
    // NULL name for default printer.
    while (num_dests > 0)
    {
      if (dests->is_default)
        return (dests);

      num_dests --;
      dests ++;
    }
  }
  else
  {
    // Lookup name and optionally the instance...
    match = cups_find_dest(name, instance, num_dests, dests, num_dests, &diff);

    if (!diff)
      return (dests + match);
  }

  return (NULL);
}


//
// '_cupsGetDestResource()' - Get the resource path and URI for a destination.
//

const char *				// O - URI
_cupsGetDestResource(
    cups_dest_t *dest,			// I - Destination
    unsigned    flags,			// I - Destination flags
    char        *resource,		// I - Resource buffer
    size_t      resourcesize)		// I - Size of resource buffer
{
  const char	*uri,			// URI
		*device_uri,		// Device URI
		*printer_uri;		// Printer URI
  char		scheme[32],		// URI scheme
		userpass[256],		// Username and password (unused)
		hostname[256];		// Hostname
  int		port;			// Port number


  DEBUG_printf("_cupsGetDestResource(dest=%p(%s), flags=%u, resource=%p, resourcesize=%d)", (void *)dest, dest ? dest->name : "(null)", flags, (void *)resource, (int)resourcesize);

  // Range check input...
  if (!dest || !resource || resourcesize < 1)
  {
    if (resource)
      *resource = '\0';

    _cupsSetError(IPP_STATUS_ERROR_INTERNAL, strerror(EINVAL), 0);
    return (NULL);
  }

  // Grab the printer and device URIs...
  device_uri  = cupsGetOption("device-uri", dest->num_options, dest->options);
  printer_uri = cupsGetOption("printer-uri-supported", dest->num_options, dest->options);

  DEBUG_printf("1_cupsGetDestResource: device-uri=\"%s\", printer-uri-supported=\"%s\".", device_uri, printer_uri);

#ifdef HAVE_DNSSD
  if (((flags & CUPS_DEST_FLAGS_DEVICE) || !printer_uri) && device_uri && strstr(device_uri, "._tcp"))
  {
    if ((device_uri = cups_dest_resolve(dest, device_uri, 5000, NULL, NULL, NULL)) != NULL)
    {
      DEBUG_printf("1_cupsGetDestResource: Resolved device-uri=\"%s\".", device_uri);
    }
    else
    {
      DEBUG_puts("1_cupsGetDestResource: Unable to resolve device.");

      if (resource)
	*resource = '\0';

      _cupsSetError(IPP_STATUS_ERROR_INTERNAL, strerror(ENOENT), 0);

      return (NULL);
    }
  }
#endif // HAVE_DNSSD

  if (flags & CUPS_DEST_FLAGS_DEVICE)
  {
    uri = device_uri;
  }
  else if (printer_uri)
  {
    uri = printer_uri;
  }
  else
  {
    uri = _cupsCreateDest(dest->name, cupsGetOption("printer-info", dest->num_options, dest->options), NULL, device_uri, resource, resourcesize);

    if (uri)
    {
      DEBUG_printf("1_cupsGetDestResource: Local printer-uri-supported=\"%s\"", uri);

      dest->num_options = cupsAddOption("printer-uri-supported", uri, dest->num_options, &dest->options);

      uri = cupsGetOption("printer-uri-supported", dest->num_options, dest->options);
    }
  }

  if (!uri)
  {
    DEBUG_puts("1_cupsGetDestResource: No printer-uri-supported or device-uri found.");

    if (resource)
      *resource = '\0';

    _cupsSetError(IPP_STATUS_ERROR_INTERNAL, strerror(ENOENT), 0);

    return (NULL);
  }
  else if (httpSeparateURI(HTTP_URI_CODING_ALL, uri, scheme, sizeof(scheme), userpass, sizeof(userpass), hostname, sizeof(hostname), &port, resource, resourcesize) < HTTP_URI_STATUS_OK)
  {
    _cupsSetError(IPP_STATUS_ERROR_INTERNAL, _("Bad URI."), 1);

    return (NULL);
  }

  DEBUG_printf("1_cupsGetDestResource: resource=\"%s\"", resource);

  return (uri);
}


//
// 'cupsGetDestWithURI()' - Get a destination associated with a URI.
//
// "name" is the desired name for the printer. If `NULL`, a name will be
// created using the URI.
//
// "uri" is the 'ipp:' or 'ipps:' URI for the printer.
//

cups_dest_t *				// O - Destination or `NULL`
cupsGetDestWithURI(const char *name,	// I - Desired printer name or `NULL`
                   const char *uri)	// I - URI for the printer
{
  cups_dest_t	*dest;			// New destination
  char		temp[1024],		// Temporary string
		scheme[256],		// Scheme from URI
		userpass[256],		// Username:password from URI
		hostname[256],		// Hostname from URI
		resource[1024],		// Resource path from URI
		*ptr;			// Pointer into string
  const char	*info;			// printer-info string
  int		port;			// Port number from URI


  // Range check input...
  if (!uri)
  {
    _cupsSetError(IPP_STATUS_ERROR_INTERNAL, strerror(EINVAL), 0);
    return (NULL);
  }

  if (httpSeparateURI(HTTP_URI_CODING_ALL, uri, scheme, sizeof(scheme), userpass, sizeof(userpass), hostname, sizeof(hostname), &port, resource, sizeof(resource)) < HTTP_URI_STATUS_OK ||
      (strncmp(uri, "ipp://", 6) && strncmp(uri, "ipps://", 7)))
  {
    _cupsSetError(IPP_STATUS_ERROR_INTERNAL, _("Bad printer-uri."), 1);

    return (NULL);
  }

  if (name)
  {
    info = name;
  }
  else
  {
    // Create the name from the URI...
    if (strstr(hostname, "._tcp"))
    {
      // Use the service instance name...
      if ((ptr = strstr(hostname, "._")) != NULL)
        *ptr = '\0';

      cups_queue_name(temp, hostname, sizeof(temp));
      name = temp;
      info = hostname;
    }
    else if (!strncmp(resource, "/classes/", 9))
    {
      snprintf(temp, sizeof(temp), "%s @ %s", resource + 9, hostname);
      name = resource + 9;
      info = temp;
    }
    else if (!strncmp(resource, "/printers/", 10))
    {
      snprintf(temp, sizeof(temp), "%s @ %s", resource + 10, hostname);
      name = resource + 10;
      info = temp;
    }
    else if (!strncmp(resource, "/ipp/print/", 11))
    {
      snprintf(temp, sizeof(temp), "%s @ %s", resource + 11, hostname);
      name = resource + 11;
      info = temp;
    }
    else
    {
      name = hostname;
      info = hostname;
    }
  }

  // Create the destination...
  if ((dest = calloc(1, sizeof(cups_dest_t))) == NULL)
  {
    _cupsSetError(IPP_STATUS_ERROR_INTERNAL, strerror(errno), 0);
    return (NULL);
  }

  dest->name        = _cupsStrAlloc(name);
  dest->num_options = cupsAddOption("device-uri", uri, dest->num_options, &(dest->options));
  dest->num_options = cupsAddOption("printer-info", info, dest->num_options, &(dest->options));

  return (dest);
}


//
// '_cupsGetDests()' - Get destinations from a server.
//
// "op" is IPP_OP_CUPS_GET_PRINTERS to get a full list, IPP_OP_CUPS_GET_DEFAULT
// to get the system-wide default printer, or IPP_OP_GET_PRINTER_ATTRIBUTES for
// a known printer.
//
// "name" is the name of an existing printer and is only used when "op" is
// IPP_OP_GET_PRINTER_ATTRIBUTES.
//
// "dest" is initialized to point to the array of destinations.
//
// 0 is returned if there are no printers, no default printer, or the named
// printer does not exist, respectively.
//
// Free the memory used by the destination array using the @link cupsFreeDests@
// function.
//
// Note: On macOS this function also gets the default paper from the system
// preferences (~/L/P/org.cups.PrintingPrefs.plist) and includes it in the
// options array for each destination that supports it.
//

size_t					// O  - Number of destinations
_cupsGetDests(http_t       *http,	// I  - Connection to server or `CUPS_HTTP_DEFAULT`
	      ipp_op_t     op,		// I  - IPP operation
	      const char   *name,	// I  - Name of destination
	      cups_dest_t  **dests,	// IO - Destinations
	      cups_ptype_t type,	// I  - Printer type bits
	      cups_ptype_t mask)	// I  - Printer type mask
{
  size_t	num_dests = 0;		// Number of destinations
  cups_dest_t	*dest;			// Current destination
  ipp_t		*request,		// IPP Request
		*response;		// IPP Response
  ipp_attribute_t *attr;		// Current attribute
  const char	*printer_name;		// printer-name attribute
  char		uri[1024];		// printer-uri value
  size_t	num_options;		// Number of options
  cups_option_t	*options;		// Options
#ifdef __APPLE__
  char		media_default[41];	// Default paper size
#endif // __APPLE__
  char		optname[1024],		// Option name
		value[2048],		// Option value
		*ptr;			// Pointer into name/value
  static const char * const pattrs[] =	// Attributes we're interested in
		{
		  "auth-info-required",
		  "device-uri",
		  "job-sheets-default",
		  "marker-change-time",
		  "marker-colors",
		  "marker-high-levels",
		  "marker-levels",
		  "marker-low-levels",
		  "marker-message",
		  "marker-names",
		  "marker-types",
#ifdef __APPLE__
		  "media-supported",
#endif // __APPLE__
		  "printer-commands",
		  "printer-defaults",
		  "printer-info",
		  "printer-is-accepting-jobs",
		  "printer-is-shared",
                  "printer-is-temporary",
		  "printer-location",
		  "printer-make-and-model",
		  "printer-mandatory-job-attributes",
		  "printer-name",
		  "printer-state",
		  "printer-state-change-time",
		  "printer-state-reasons",
		  "printer-type",
		  "printer-uri-supported"
		};


  DEBUG_printf("_cupsGetDests(http=%p, op=%x(%s), name=\"%s\", dests=%p, type=%x, mask=%x)", (void *)http, op, ippOpString(op), name, (void *)dests, type, mask);

#ifdef __APPLE__
  // Get the default paper size...
  appleGetPaperSize(media_default, sizeof(media_default));
  DEBUG_printf("1_cupsGetDests: Default media is '%s'.", media_default);
#endif // __APPLE__

  // Build a IPP_OP_CUPS_GET_PRINTERS or IPP_OP_GET_PRINTER_ATTRIBUTES request, which
  // require the following attributes:
  //
  //   attributes-charset
  //   attributes-natural-language
  //   requesting-user-name
  //   printer-uri [for IPP_OP_GET_PRINTER_ATTRIBUTES]
  request = ippNewRequest(op);

  if (name && op != IPP_OP_CUPS_GET_DEFAULT)
  {
    httpAssembleURIf(HTTP_URI_CODING_ALL, uri, sizeof(uri), "ipp", NULL, "localhost", ippGetPort(), "/printers/%s", name);
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri", NULL, uri);
  }
  else if (mask)
  {
    ippAddInteger(request, IPP_TAG_OPERATION, IPP_TAG_ENUM, "printer-type", (int)type);
    ippAddInteger(request, IPP_TAG_OPERATION, IPP_TAG_ENUM, "printer-type-mask", (int)mask);
  }

  ippAddStrings(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD, "requested-attributes", sizeof(pattrs) / sizeof(pattrs[0]), NULL, pattrs);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name", NULL, cupsGetUser());

  // Do the request and get back a response...
  if ((response = cupsDoRequest(http, request, "/")) != NULL)
  {
    for (attr = response->attrs; attr != NULL; attr = attr->next)
    {
      // Skip leading attributes until we hit a printer...
      while (attr != NULL && attr->group_tag != IPP_TAG_PRINTER)
        attr = attr->next;

      if (attr == NULL)
        break;

      // Pull the needed attributes from this printer...
      printer_name = NULL;
      num_options  = 0;
      options      = NULL;

      for (; attr && attr->group_tag == IPP_TAG_PRINTER; attr = attr->next)
      {
	if (attr->value_tag != IPP_TAG_INTEGER &&
	    attr->value_tag != IPP_TAG_ENUM &&
	    attr->value_tag != IPP_TAG_BOOLEAN &&
	    attr->value_tag != IPP_TAG_TEXT &&
	    attr->value_tag != IPP_TAG_TEXTLANG &&
	    attr->value_tag != IPP_TAG_NAME &&
	    attr->value_tag != IPP_TAG_NAMELANG &&
	    attr->value_tag != IPP_TAG_KEYWORD &&
	    attr->value_tag != IPP_TAG_RANGE &&
	    attr->value_tag != IPP_TAG_URI)
          continue;

        if (!strcmp(attr->name, "auth-info-required") ||
	    !strcmp(attr->name, "device-uri") ||
	    !strcmp(attr->name, "marker-change-time") ||
	    !strcmp(attr->name, "marker-colors") ||
	    !strcmp(attr->name, "marker-high-levels") ||
	    !strcmp(attr->name, "marker-levels") ||
	    !strcmp(attr->name, "marker-low-levels") ||
	    !strcmp(attr->name, "marker-message") ||
	    !strcmp(attr->name, "marker-names") ||
	    !strcmp(attr->name, "marker-types") ||
	    !strcmp(attr->name, "printer-commands") ||
	    !strcmp(attr->name, "printer-info") ||
            !strcmp(attr->name, "printer-is-shared") ||
            !strcmp(attr->name, "printer-is-temporary") ||
	    !strcmp(attr->name, "printer-make-and-model") ||
	    !strcmp(attr->name, "printer-mandatory-job-attributes") ||
	    !strcmp(attr->name, "printer-state") ||
	    !strcmp(attr->name, "printer-state-change-time") ||
	    !strcmp(attr->name, "printer-type") ||
            !strcmp(attr->name, "printer-is-accepting-jobs") ||
            !strcmp(attr->name, "printer-location") ||
            !strcmp(attr->name, "printer-state-reasons") ||
	    !strcmp(attr->name, "printer-uri-supported"))
        {
	  // Add a printer description attribute...
          num_options = cupsAddOption(attr->name, cups_make_string(attr, value, sizeof(value)), num_options, &options);
	}
#ifdef __APPLE__
	else if (!strcmp(attr->name, "media-supported") && media_default[0])
	{
	  // See if we can set a default media size...
          size_t	i;		// Looping var

	  for (i = 0; i < attr->num_values; i ++)
	  {
	    if (!_cups_strcasecmp(media_default, attr->values[i].string.text))
	    {
              DEBUG_printf("1_cupsGetDests: Setting media to '%s'.", media_default);
	      num_options = cupsAddOption("media", media_default, num_options, &options);
              break;
	    }
	  }
	}
#endif // __APPLE__
        else if (!strcmp(attr->name, "printer-name") && attr->value_tag == IPP_TAG_NAME)
	{
	  printer_name = attr->values[0].string.text;
        }
        else if (strncmp(attr->name, "notify-", 7) && strncmp(attr->name, "print-quality-", 14) && (attr->value_tag == IPP_TAG_BOOLEAN || attr->value_tag == IPP_TAG_ENUM || attr->value_tag == IPP_TAG_INTEGER || attr->value_tag == IPP_TAG_KEYWORD || attr->value_tag == IPP_TAG_NAME || attr->value_tag == IPP_TAG_RANGE) && (ptr = strstr(attr->name, "-default")) != NULL)
	{
	  // Add a default option...
          cupsCopyString(optname, attr->name, sizeof(optname));
	  optname[ptr - attr->name] = '\0';

	  if (_cups_strcasecmp(optname, "media") || !cupsGetOption("media", num_options, options))
	    num_options = cupsAddOption(optname, cups_make_string(attr, value, sizeof(value)), num_options, &options);
	}
      }

      // See if we have everything needed...
      if (!printer_name)
      {
        cupsFreeOptions(num_options, options);

        if (attr == NULL)
	  break;
	else
          continue;
      }

      if ((dest = cups_add_dest(printer_name, NULL, &num_dests, dests)) != NULL)
      {
        dest->num_options = num_options;
	dest->options     = options;
      }
      else
        cupsFreeOptions(num_options, options);

      if (attr == NULL)
	break;
    }

    ippDelete(response);
  }

  // Return the count...
  return (num_dests);
}


//
// 'cupsGetDests()' - Get the list of destinations from the specified server.
//
// Starting with CUPS 1.2, the returned list of destinations include the
// "printer-info", "printer-is-accepting-jobs", "printer-is-shared",
// "printer-make-and-model", "printer-state", "printer-state-change-time",
// "printer-state-reasons", "printer-type", and "printer-uri-supported"
// attributes as options.
//
// CUPS 1.4 adds the "marker-change-time", "marker-colors",
// "marker-high-levels", "marker-levels", "marker-low-levels", "marker-message",
// "marker-names", "marker-types", and "printer-commands" attributes as options.
//
// CUPS 2.2 adds accessible IPP printers to the list of destinations that can
// be used.  The "printer-uri-supported" option will be present for those IPP
// printers that have been recently used.
//
// Use the @link cupsFreeDests@ function to free the destination list and
// the @link cupsGetDest@ function to find a particular destination.
//
//
//

size_t					// O - Number of destinations
cupsGetDests(http_t      *http,		// I - Connection to server or @code CUPS_HTTP_DEFAULT@
             cups_dest_t **dests)	// O - Destinations
{
  _cups_getdata_t data;                 // Enumeration data


  DEBUG_printf("cupsGetDests(http=%p, dests=%p)", (void *)http, (void *)dests);

  // Range check the input...
  if (!dests)
  {
    _cupsSetError(IPP_STATUS_ERROR_INTERNAL, _("Bad NULL dests pointer"), 1);
    DEBUG_puts("1cupsGetDests: NULL dests pointer, returning 0.");
    return (0);
  }

  // Connect to the server as needed...
  if (!http)
  {
    if ((http = _cupsConnect()) == NULL)
    {
      *dests = NULL;

      return (0);
    }
  }

  // Grab the printers and classes...
  data.num_dests = 0;
  data.dests     = NULL;

  if (!httpAddrIsLocalhost(httpGetAddress(http)))
  {
    // When talking to a remote cupsd, just enumerate printers on the remote cupsd.
    cups_enum_dests(http, 0, _CUPS_DNSSD_GET_DESTS, NULL, 0, CUPS_PRINTER_DISCOVERED, (cups_dest_cb_t)cups_get_cb, &data);
  }
  else
  {
    // When talking to a local cupsd, enumerate both local printers and ones we
    // can find on the network...
    cups_enum_dests(http, 0, _CUPS_DNSSD_GET_DESTS, NULL, 0, 0, (cups_dest_cb_t)cups_get_cb, &data);
  }

  // Return the number of destinations...
  *dests = data.dests;

  if (data.num_dests > 0)
    _cupsSetError(IPP_STATUS_OK, NULL, 0);

  DEBUG_printf("1cupsGetDests: Returning %u destinations.", (unsigned)data.num_dests);

  return (data.num_dests);
}


//
// 'cupsGetNamedDest()' - Get options for the named destination.
//
// This function is optimized for retrieving a single destination and should
// be used instead of @link cupsGetDests2@ and @link cupsGetDest@ when you
// either know the name of the destination or want to print to the default
// destination.  If `NULL` is returned, the destination does not exist or
// there is no default destination.
//
// If "http" is @code CUPS_HTTP_DEFAULT@, the connection to the default print
// server will be used.
//
// If "name" is `NULL`, the default printer for the current user will be
// returned.
//
// The returned destination must be freed using @link cupsFreeDests@ with a
// "num_dests" value of 1.
//
//
//

cups_dest_t *				// O - Destination or `NULL`
cupsGetNamedDest(http_t     *http,	// I - Connection to server or @code CUPS_HTTP_DEFAULT@
                 const char *name,	// I - Destination name or `NULL` for the default destination
                 const char *instance)	// I - Instance name or `NULL`
{
  const char    *dest_name;             // Working destination name
  cups_dest_t	*dest;			// Destination
  char		filename[1024],		// Path to lpoptions
		defname[256];		// Default printer name
  int		set_as_default = 0;	// Set returned destination as default
  ipp_op_t	op = IPP_OP_GET_PRINTER_ATTRIBUTES;
					// IPP operation to get server ops
  _cups_globals_t *cg = _cupsGlobals();	// Pointer to library globals


  DEBUG_printf("cupsGetNamedDest(http=%p, name=\"%s\", instance=\"%s\")", (void *)http, name, instance);

  // If "name" is NULL, find the default destination...
  dest_name = name;

  if (!dest_name)
  {
    set_as_default = 1;
    dest_name      = _cupsGetUserDefault(defname, sizeof(defname));

    if (dest_name)
    {
      char	*ptr;			// Temporary pointer...

      if ((ptr = strchr(defname, '/')) != NULL)
      {
        *ptr++   = '\0';
	instance = ptr;
      }
      else
      {
        instance = NULL;
      }
    }
    else if (cg->userconfig)
    {
      // No default in the environment, try the user's lpoptions files...
      snprintf(filename, sizeof(filename), "%s/lpoptions", cg->userconfig);

      dest_name = cups_get_default(filename, defname, sizeof(defname), &instance);

      if (dest_name)
        set_as_default = 2;
    }

    if (!dest_name)
    {
      // Still not there?  Try the system lpoptions file...
      snprintf(filename, sizeof(filename), "%s/lpoptions", cg->sysconfig);
      dest_name = cups_get_default(filename, defname, sizeof(defname), &instance);

      if (dest_name)
        set_as_default = 3;
    }

    if (!dest_name)
    {
      // No locally-set default destination, ask the server...
      op             = IPP_OP_CUPS_GET_DEFAULT;
      set_as_default = 4;

      DEBUG_puts("1cupsGetNamedDest: Asking server for default printer...");
    }
    else
      DEBUG_printf("1cupsGetNamedDest: Using name=\"%s\"...", name);
  }

  // Get the printer's attributes...
  if (!_cupsGetDests(http, op, dest_name, &dest, 0, 0))
  {
    if (name)
    {
      _cups_namedata_t  data;           // Callback data

      DEBUG_puts("1cupsGetNamedDest: No queue found for printer, looking on network...");

      data.name = name;
      data.dest = NULL;

      cupsEnumDests(0, 1000, NULL, 0, 0, (cups_dest_cb_t)cups_name_cb, &data);

      if (!data.dest)
      {
        _cupsSetError(IPP_STATUS_ERROR_NOT_FOUND, _("The printer or class does not exist."), 1);
        return (NULL);
      }

      dest = data.dest;
    }
    else
    {
      switch (set_as_default)
      {
        default :
            break;

        case 1 : // Set from env vars
            if (getenv("LPDEST"))
              _cupsSetError(IPP_STATUS_ERROR_NOT_FOUND, _("LPDEST environment variable names default destination that does not exist."), 1);
	    else if (getenv("PRINTER"))
              _cupsSetError(IPP_STATUS_ERROR_NOT_FOUND, _("PRINTER environment variable names default destination that does not exist."), 1);
	    else
              _cupsSetError(IPP_STATUS_ERROR_NOT_FOUND, _("No default destination."), 1);
            break;

        case 2 : // Set from ~/.../lpoptions
	    _cupsSetError(IPP_STATUS_ERROR_NOT_FOUND, _("~/.../lpoptions file names default destination that does not exist."), 1);
            break;

        case 3 : // Set from /etc/cups/lpoptions
	    _cupsSetError(IPP_STATUS_ERROR_NOT_FOUND, _("/etc/cups/lpoptions file names default destination that does not exist."), 1);
            break;

        case 4 : // Set from server
	    _cupsSetError(IPP_STATUS_ERROR_NOT_FOUND, _("No default destination."), 1);
            break;
      }

      return (NULL);
    }
  }

  DEBUG_printf("1cupsGetNamedDest: Got dest=%p", (void *)dest);

  if (instance)
    dest->instance = _cupsStrAlloc(instance);

  if (set_as_default)
    dest->is_default = true;

  // Then add local options...
  snprintf(filename, sizeof(filename), "%s/lpoptions", cg->sysconfig);
  cups_get_dests(filename, dest_name, instance, 0, 1, 1, &dest);

  if (cg->userconfig)
  {
    snprintf(filename, sizeof(filename), "%s/lpoptions", cg->userconfig);

    cups_get_dests(filename, dest_name, instance, 0, 1, 1, &dest);
  }

  // Return the result...
  return (dest);
}


//
// 'cupsRemoveDest()' - Remove a destination from the destination list.
//
// Removing a destination/instance does not delete the class or printer
// queue, merely the lpoptions for that destination/instance.  Use the
// @link cupsSetDests@ function to save the new options for the user.
//
//
//

size_t					// O  - New number of destinations
cupsRemoveDest(const char  *name,	// I  - Destination name
               const char  *instance,	// I  - Instance name or `NULL`
	       size_t      num_dests,	// I  - Number of destinations
	       cups_dest_t **dests)	// IO - Destinations
{
  size_t	i;			// Index into destinations
  cups_dest_t	*dest;			// Pointer to destination


  // Find the destination...
  if ((dest = cupsGetDest(name, instance, num_dests, *dests)) == NULL)
    return (num_dests);

  // Free memory...
  _cupsStrFree(dest->name);
  _cupsStrFree(dest->instance);
  cupsFreeOptions(dest->num_options, dest->options);

  // Remove the destination from the array...
  num_dests --;

  i = (size_t)(dest - *dests);

  if (i < num_dests)
    memmove(dest, dest + 1, (num_dests - i) * sizeof(cups_dest_t));

  return (num_dests);
}


//
// 'cupsSetDefaultDest()' - Set the default destination.
//
//
//

void
cupsSetDefaultDest(
    const char  *name,			// I - Destination name
    const char  *instance,		// I - Instance name or `NULL`
    size_t      num_dests,		// I - Number of destinations
    cups_dest_t *dests)			// I - Destinations
{
  size_t	i;			// Looping var
  cups_dest_t	*dest;			// Current destination


  // Range check input...
  if (!name || num_dests <= 0 || !dests)
    return;

  // Loop through the array and set the "is_default" flag for the matching destination...
  for (i = num_dests, dest = dests; i > 0; i --, dest ++)
    dest->is_default = !_cups_strcasecmp(name, dest->name) && ((!instance && !dest->instance) || (instance && dest->instance && !_cups_strcasecmp(instance, dest->instance)));
}


//
// 'cupsSetDests()' - Save the list of destinations for the specified server.
//
// This function saves the destinations to /etc/cups/lpoptions when run
// as root and ~/.../lpoptions when run as a normal user.
//

bool					// O - `true` on success, `false` on error
cupsSetDests(http_t      *http,		// I - Connection to server or @code CUPS_HTTP_DEFAULT@
             size_t      num_dests,	// I - Number of destinations
             cups_dest_t *dests)	// I - Destinations
{
  size_t	i, j;			// Looping vars
  bool		wrote;			// Wrote definition?
  cups_dest_t	*dest;			// Current destination
  cups_option_t	*option;		// Current option
  _ipp_option_t	*match;			// Matching attribute for option
  FILE		*fp;			// File pointer
  char		filename[1024];		// lpoptions file
  size_t	num_temps;		// Number of temporary destinations
  cups_dest_t	*temps = NULL,		// Temporary destinations
		*temp;			// Current temporary dest
  const char	*val;			// Value of temporary option
  _cups_globals_t *cg = _cupsGlobals();	// Pointer to library globals


  // Range check the input...
  if (!num_dests || !dests)
    return (false);

  // Get the server destinations...
  num_temps = _cupsGetDests(http, IPP_OP_CUPS_GET_PRINTERS, NULL, &temps, 0, 0);

  if (cupsGetError() >= IPP_STATUS_REDIRECTION_OTHER_SITE)
  {
    cupsFreeDests(num_temps, temps);
    return (false);
  }

  // Figure out which file to write to...
  snprintf(filename, sizeof(filename), "%s/lpoptions", cg->sysconfig);

  if (cg->userconfig)
  {
    // Create user subdirectory...
    if (mkdir(cg->userconfig, 0700) && errno != EEXIST)
    {
      cupsFreeDests(num_temps, temps);
      return (false);
    }

    snprintf(filename, sizeof(filename), "%s/lpoptions", cg->userconfig);
  }

  // Try to open the file...
  if ((fp = fopen(filename, "w")) == NULL)
  {
    cupsFreeDests(num_temps, temps);
    return (false);
  }

#ifndef _WIN32
  // Set the permissions to 0644 when saving to the /etc/cups/lpoptions file...
  if (!getuid())
    fchmod(fileno(fp), 0644);
#endif // !_WIN32

  // Write each printer; each line looks like:
  //
  //   Dest name[/instance] options
  //   Default name[/instance] options
  for (i = num_dests, dest = dests; i > 0; i --, dest ++)
  {
    if (dest->instance != NULL || dest->num_options != 0 || dest->is_default)
    {
      if (dest->is_default)
      {
	fprintf(fp, "Default %s", dest->name);
	if (dest->instance)
	  fprintf(fp, "/%s", dest->instance);

        wrote = true;
      }
      else
      {
        wrote = false;
      }

      temp = cupsGetDest(dest->name, NULL, num_temps, temps);

      for (j = dest->num_options, option = dest->options; j > 0; j --, option ++)
      {
        // See if this option is a printer attribute; if so, skip it...
        if ((match = _ippFindOption(option->name)) != NULL && match->group_tag == IPP_TAG_PRINTER)
	  continue;

        // See if the server options match these; if so, don't write 'em.
        if (temp && (val = cupsGetOption(option->name, temp->num_options, temp->options)) != NULL && !_cups_strcasecmp(val, option->value))
	  continue;

        // Options don't match, write to the file...
        if (!wrote)
	{
	  fprintf(fp, "Dest %s", dest->name);
	  if (dest->instance)
	    fprintf(fp, "/%s", dest->instance);
          wrote = true;
	}

        if (option->value[0])
	{
	  if (strchr(option->value, ' ') || strchr(option->value, '\\') || strchr(option->value, '\"') || strchr(option->value, '\''))
	  {
	    // Quote the value...
	    fprintf(fp, " %s=\"", option->name);

	    for (val = option->value; *val; val ++)
	    {
	      if (strchr("\"\'\\", *val))
	        putc('\\', fp);

              putc(*val, fp);
	    }

	    putc('\"', fp);
          }
	  else
	  {
	    // Store the literal value...
	    fprintf(fp, " %s=%s", option->name, option->value);
          }
	}
	else
	  fprintf(fp, " %s", option->name);
      }

      if (wrote)
        fputs("\n", fp);
    }
  }

  // Free the temporary destinations and close the file...
  cupsFreeDests(num_temps, temps);

  fclose(fp);

#ifdef __APPLE__
  // Set the default printer for this location - this allows command-line
  // and GUI applications to share the same default destination...
  if ((dest = cupsGetDest(NULL, NULL, num_dests, dests)) != NULL)
  {
    CFStringRef name = CFStringCreateWithCString(kCFAllocatorDefault, dest->name, kCFStringEncodingUTF8);
					// Default printer name

    if (name)
    {
      _cupsAppleSetDefaultPrinter(name);
      CFRelease(name);
    }
  }

  // Send a notification so that macOS applications can know about the change, too.
  notify_post("com.apple.printerListChange");
#endif // __APPLE__

  return (true);
}


//
// '_cupsGetUserDefault()' - Get the user default printer from environment
//                        variables and location information.
//

char *					// O - Default printer or NULL
_cupsGetUserDefault(char   *name,	// I - Name buffer
                    size_t namesize)	// I - Size of name buffer
{
  const char	*env;			// LPDEST or PRINTER env variable
#ifdef __APPLE__
  CFStringRef	locprinter;		// Last printer as this location
#endif // __APPLE__


  if ((env = getenv("LPDEST")) == NULL)
  {
    if ((env = getenv("PRINTER")) != NULL && !strcmp(env, "lp"))
      env = NULL;
  }

  if (env)
  {
    cupsCopyString(name, env, namesize);
    return (name);
  }

#ifdef __APPLE__
  // Use location-based defaults if "use last printer" is selected in the
  // system preferences...
  if (!getenv("CUPS_NO_APPLE_DEFAULT") && (locprinter = _cupsAppleCopyDefaultPrinter()) != NULL)
  {
    CFStringGetCString(locprinter, name, (CFIndex)namesize, kCFStringEncodingUTF8);
    CFRelease(locprinter);
  }
  else
  {
    name[0] = '\0';
  }

  DEBUG_printf("1_cupsGetUserDefault: Returning \"%s\".", name);

  return (*name ? name : NULL);

#else
  // No location-based defaults on this platform...
  name[0] = '\0';
  return (NULL);
#endif // __APPLE__
}


#if _CUPS_LOCATION_DEFAULTS
//
// 'appleCopyLocations()' - Copy the location history array.
//

static CFArrayRef			// O - Location array or NULL
appleCopyLocations(void)
{
  CFArrayRef	locations;		// Location array


  // Look up the location array in the preferences...
  if ((locations = CFPreferencesCopyAppValue(kLastUsedPrintersKey, kCUPSPrintingPrefs)) == NULL)
    return (NULL);

  if (CFGetTypeID(locations) != CFArrayGetTypeID())
  {
    CFRelease(locations);
    return (NULL);
  }

  return (locations);
}


//
// 'appleCopyNetwork()' - Get the network ID for the current location.
//

static CFStringRef			// O - Network ID
appleCopyNetwork(void)
{
  SCDynamicStoreRef	dynamicStore;	// System configuration data
  CFStringRef		key;		// Current network configuration key
  CFDictionaryRef	ip_dict;	// Network configuration data
  CFStringRef		network = NULL;	// Current network ID


  if ((dynamicStore = SCDynamicStoreCreate(NULL, CFSTR("libcups"), NULL,
                                           NULL)) != NULL)
  {
    // First use the IPv6 router address, if available, since that will generally
    // be a globally-unique link-local address.
    if ((key = SCDynamicStoreKeyCreateNetworkGlobalEntity(NULL, kSCDynamicStoreDomainState, kSCEntNetIPv6)) != NULL)
    {
      if ((ip_dict = SCDynamicStoreCopyValue(dynamicStore, key)) != NULL)
      {
	if ((network = CFDictionaryGetValue(ip_dict, kSCPropNetIPv6Router)) != NULL)
          CFRetain(network);

        CFRelease(ip_dict);
      }

      CFRelease(key);
    }

    // If that doesn't work, try the IPv4 router address. This isn't as unique
    // and will likely be a 10.x.y.z or 192.168.y.z address...
    if (!network)
    {
      if ((key = SCDynamicStoreKeyCreateNetworkGlobalEntity(NULL, kSCDynamicStoreDomainState, kSCEntNetIPv4)) != NULL)
      {
	if ((ip_dict = SCDynamicStoreCopyValue(dynamicStore, key)) != NULL)
	{
	  if ((network = CFDictionaryGetValue(ip_dict, kSCPropNetIPv4Router)) != NULL)
	    CFRetain(network);

	  CFRelease(ip_dict);
	}

	CFRelease(key);
      }
    }

    CFRelease(dynamicStore);
  }

  return (network);
}
#endif // _CUPS_LOCATION_DEFAULTS


#ifdef __APPLE__
//
// 'appleGetPaperSize()' - Get the default paper size.
//

static char *				// O - Default paper size
appleGetPaperSize(char   *name,		// I - Paper size name buffer
                  size_t namesize)	// I - Size of buffer
{
  CFStringRef	defaultPaperID;		// Default paper ID
  pwg_media_t	*pwgmedia;		// PWG media size


  defaultPaperID = _cupsAppleCopyDefaultPaperID();
  if (!defaultPaperID ||
      CFGetTypeID(defaultPaperID) != CFStringGetTypeID() ||
      !CFStringGetCString(defaultPaperID, name, (CFIndex)namesize, kCFStringEncodingUTF8))
    name[0] = '\0';
  else if ((pwgmedia = pwgMediaForLegacy(name)) != NULL)
    cupsCopyString(name, pwgmedia->pwg, namesize);

  if (defaultPaperID)
    CFRelease(defaultPaperID);

  return (name);
}
#endif // __APPLE__


#if _CUPS_LOCATION_DEFAULTS
//
// 'appleGetPrinter()' - Get a printer from the history array.
//

static CFStringRef			// O - Printer name or NULL
appleGetPrinter(CFArrayRef  locations,	// I - Location array
                CFStringRef network,	// I - Network name
		CFIndex     *locindex)	// O - Index in array
{
  CFIndex		i,		// Looping var
			count;		// Number of locations
  CFDictionaryRef	location;	// Current location
  CFStringRef		locnetwork,	// Current network
			locprinter;	// Current printer


  for (i = 0, count = CFArrayGetCount(locations); i < count; i ++)
    if ((location = CFArrayGetValueAtIndex(locations, i)) != NULL &&
        CFGetTypeID(location) == CFDictionaryGetTypeID())
    {
      if ((locnetwork = CFDictionaryGetValue(location,
                                             kLocationNetworkKey)) != NULL &&
          CFGetTypeID(locnetwork) == CFStringGetTypeID() &&
	  CFStringCompare(network, locnetwork, 0) == kCFCompareEqualTo &&
	  (locprinter = CFDictionaryGetValue(location,
	                                     kLocationPrinterIDKey)) != NULL &&
	  CFGetTypeID(locprinter) == CFStringGetTypeID())
      {
        if (locindex)
	  *locindex = i;

	return (locprinter);
      }
    }

  return (NULL);
}
#endif // _CUPS_LOCATION_DEFAULTS


//
// 'cups_add_dest()' - Add a destination to the array.
//
// Unlike cupsAddDest(), this function does not check for duplicates.
//

static cups_dest_t *			// O  - New destination
cups_add_dest(const char  *name,	// I  - Name of destination
              const char  *instance,	// I  - Instance or NULL
              size_t      *num_dests,	// IO - Number of destinations
	      cups_dest_t **dests)	// IO - Destinations
{
  size_t	insert;			// Insertion point
  int		diff;			// Result of comparison
  cups_dest_t	*dest;			// Destination pointer


  // Add new destination...
  if (*num_dests == 0)
    dest = malloc(sizeof(cups_dest_t));
  else
    dest = realloc(*dests, sizeof(cups_dest_t) * (size_t)(*num_dests + 1));

  if (!dest)
    return (NULL);

  *dests = dest;

  // Find where to insert the destination...
  if (*num_dests == 0)
  {
    insert = 0;
  }
  else
  {
    insert = cups_find_dest(name, instance, *num_dests, *dests, *num_dests - 1, &diff);

    if (diff > 0)
      insert ++;
  }

  // Move the array elements as needed...
  if (insert < *num_dests)
    memmove(*dests + insert + 1, *dests + insert, (*num_dests - insert) * sizeof(cups_dest_t));

  (*num_dests) ++;

  // Initialize the destination...
  dest              = *dests + insert;
  dest->name        = _cupsStrAlloc(name);
  dest->instance    = _cupsStrAlloc(instance);
  dest->is_default  = false;
  dest->num_options = 0;
  dest->options     = (cups_option_t *)0;

  return (dest);
}


//
// 'cups_compare_dests()' - Compare two destinations.
//

static int				// O - Result of comparison
cups_compare_dests(cups_dest_t *a,	// I - First destination
                   cups_dest_t *b)	// I - Second destination
{
  int	diff;				// Difference


  if ((diff = _cups_strcasecmp(a->name, b->name)) != 0)
    return (diff);
  else if (a->instance && b->instance)
    return (_cups_strcasecmp(a->instance, b->instance));
  else
    return ((a->instance && !b->instance) - (!a->instance && b->instance));
}


//
// 'cups_dest_browse_cb()' - Browse for printers.
//

static void
cups_dest_browse_cb(
    cups_dnssd_browse_t *browse,	// I - DNS-SD browser
    void                *context,	// I - Enumeration data
    cups_dnssd_flags_t  flags,		// I - Flags
    uint32_t            if_index,	// I - Interface
    const char          *serviceName,	// I - Name of service/device
    const char          *regtype,	// I - Type of service
    const char          *replyDomain)	// I - Service domain
{
  _cups_dnssd_data_t	*data = (_cups_dnssd_data_t *)context;
					// Enumeration data


  DEBUG_printf("5cups_dest_browse_cb(browse=%p, context=%p, flags=%x, if_index=%d, serviceName=\"%s\", regtype=\"%s\", replyDomain=\"%s\")", (void *)browse, context, flags, if_index, serviceName, regtype, replyDomain);

  // Don't do anything on error, only add services...
  if ((flags & CUPS_DNSSD_FLAGS_ERROR) || !(flags & CUPS_DNSSD_FLAGS_ADD))
    return;

  // Get the device...
  cups_dnssd_get_device(data, serviceName, regtype, replyDomain);
}
//
// 'cups_dnssd_compare_device()' - Compare two devices.
//

static int				// O - Result of comparison
cups_dnssd_compare_devices(
    _cups_dnssd_device_t *a,		// I - First device
    _cups_dnssd_device_t *b)		// I - Second device
{
  return (strcmp(a->dest.name, b->dest.name));
}


//
// 'cups_dnssd_free_device()' - Free the memory used by a device.
//

static void
cups_dnssd_free_device(
    _cups_dnssd_device_t *device,	// I - Device
    _cups_dnssd_data_t   *data)		// I - Enumeration data
{
  DEBUG_printf("5cups_dnssd_free_device(device=%p(%s), data=%p)", (void *)device, device->dest.name, (void *)data);

  _cupsStrFree(device->domain);
  _cupsStrFree(device->fullname);
  _cupsStrFree(device->regtype);
  _cupsStrFree(device->dest.name);

  cupsFreeOptions(device->dest.num_options, device->dest.options);

  free(device);
}


//
// 'cups_dnssd_get_device()' - Lookup a device and create it as needed.
//

static _cups_dnssd_device_t *		// O - Device
cups_dnssd_get_device(
    _cups_dnssd_data_t *data,		// I - Enumeration data
    const char         *serviceName,	// I - Service name
    const char         *regtype,	// I - Registration type
    const char         *replyDomain)	// I - Domain name
{
  _cups_dnssd_device_t	key,		// Search key
			*device;	// Device
  char			fullname[1024],	// Full name for query
			name[128];	// Queue name


  DEBUG_printf("5cups_dnssd_get_device(data=%p, serviceName=\"%s\", regtype=\"%s\", replyDomain=\"%s\")", (void *)data, serviceName, regtype, replyDomain);

  // See if this is an existing device...
  cups_queue_name(name, serviceName, sizeof(name));

  key.dest.name = name;

  cupsRWLockRead(&data->rwlock);

  device = cupsArrayFind(data->devices, &key);

  cupsRWUnlock(&data->rwlock);

  if (device)
  {
    // Yes, see if we need to do anything with this...
    bool	update = false;		// Non-zero if we need to update

    if (!_cups_strcasecmp(replyDomain, "local.") && _cups_strcasecmp(device->domain, replyDomain))
    {
      // Update the "global" listing to use the .local domain name instead.
      _cupsStrFree(device->domain);
      device->domain = _cupsStrAlloc(replyDomain);

      DEBUG_printf("6cups_dnssd_get_device: Updating '%s' to use local domain.", device->dest.name);

      update = true;
    }

    if (!_cups_strcasecmp(regtype, "_ipps._tcp") && _cups_strcasecmp(device->regtype, regtype))
    {
      // Prefer IPPS over IPP.
      _cupsStrFree(device->regtype);
      device->regtype = _cupsStrAlloc(regtype);

      DEBUG_printf("6cups_dnssd_get_device: Updating '%s' to use IPPS.", device->dest.name);

      update = true;
    }

    if (!update)
    {
      DEBUG_printf("6cups_dnssd_get_device: No changes to '%s'.", device->dest.name);
      return (device);
    }
  }
  else
  {
    // No, add the device...
    DEBUG_printf("6cups_dnssd_get_device: Adding '%s' for %s with domain '%s'.", serviceName, !strcmp(regtype, "_ipps._tcp") ? "IPPS" : "IPP", replyDomain);

    if ((device = calloc(sizeof(_cups_dnssd_device_t), 1)) == NULL)
      return (NULL);

    device->dest.name = _cupsStrAlloc(name);
    device->domain    = _cupsStrAlloc(replyDomain);
    device->regtype   = _cupsStrAlloc(regtype);

    device->dest.num_options = cupsAddOption("printer-info", serviceName, 0, &device->dest.options);

    cupsRWLockWrite(&data->rwlock);
    cupsArrayAdd(data->devices, device);
    cupsRWUnlock(&data->rwlock);
  }

  // Set the "full name" of this service, which is used for queries...
  cupsDNSSDAssembleFullName(fullname, sizeof(fullname), serviceName, regtype, replyDomain);
  _cupsStrFree(device->fullname);
  device->fullname = _cupsStrAlloc(fullname);

  cupsDNSSDQueryDelete(device->query);
  device->query = NULL;

  if (device->state == _CUPS_DNSSD_ACTIVE)
  {
    DEBUG_printf("6cups_dnssd_get_device: Remove callback for \"%s\".", device->dest.name);

    (*data->cb)(data->user_data, CUPS_DEST_FLAGS_REMOVED, &device->dest);
    device->state = _CUPS_DNSSD_NEW;
  }

  return (device);
}


//
// 'cups_dest_query_cb()' - Process query data.
//

static void
cups_dest_query_cb(
    cups_dnssd_query_t  *query,		// I - Query request
    void                *context,	// I - Enumeration data
    cups_dnssd_flags_t	flags,		// I - DNS-SD flags
    uint32_t            if_index,	// I - Interface
    const char          *fullname,	// I - Full service name
    uint16_t            rrtype,		// I - Record type
    const void          *rdata,		// I - Record data
    uint16_t            rdlen)		// I - Length of record data
{
  _cups_dnssd_data_t	*data = (_cups_dnssd_data_t *)context;
					// Enumeration data
  char			serviceName[256],// Service name
			name[128],	// Queue name
			*ptr;		// Pointer into string
  _cups_dnssd_device_t	dkey,		// Search key
			*device;	// Device


  (void)query;
  (void)if_index;
  (void)rrtype;

  // Only process "add" data...
  if (!(flags & CUPS_DNSSD_FLAGS_ADD) || (flags & CUPS_DNSSD_FLAGS_ERROR))
    return;

  // Lookup the service in the devices array.
  cups_dnssd_unquote(serviceName, fullname, sizeof(serviceName));

  if ((ptr = strstr(serviceName, "._")) != NULL)
    *ptr = '\0';

  cups_queue_name(name, serviceName, sizeof(name));

  dkey.dest.name = name;

  if ((device = cupsArrayFind(data->devices, &dkey)) != NULL && device->state == _CUPS_DNSSD_NEW)
  {
    // Found it, pull out the make and model from the TXT record and save it...
    const uint8_t	*txt,		// Pointer into data
			*txtnext,	// Next key/value pair
			*txtend;	// End of entire TXT record
    uint8_t		txtlen;		// Length of current key/value pair
    char		key[256],	// Key string
			value[256],	// Value string
			make_and_model[512],
					// Manufacturer and model
			model[256],	// Model
			uriname[1024],	// Name for URI
			uri[1024];	// Printer URI
    cups_ptype_t	type = CUPS_PRINTER_DISCOVERED | CUPS_PRINTER_BW;
					// Printer type
    bool		saw_printer_type = false;
					// Did we see a printer-type key?

    device->state     = _CUPS_DNSSD_PENDING;
    make_and_model[0] = '\0';

    cupsCopyString(model, "Unknown", sizeof(model));

    for (txt = rdata, txtend = txt + rdlen; txt < txtend; txt = txtnext)
    {
      // Read a key/value pair starting with an 8-bit length.  Since the
      // length is 8 bits and the size of the key/value buffers is 256, we
      // don't need to check for overflow...
      txtlen = *txt++;

      if (!txtlen || (txt + txtlen) > txtend)
	break;

      txtnext = txt + txtlen;

      for (ptr = key; txt < txtnext && *txt != '='; txt ++)
	*ptr++ = (char)*txt;
      *ptr = '\0';

      if (txt < txtnext && *txt == '=')
      {
	txt ++;

	if (txt < txtnext)
	  memcpy(value, txt, (size_t)(txtnext - txt));
	value[txtnext - txt] = '\0';

	DEBUG_printf("6cups_dest_query_cb: %s=%s", key, value);
      }
      else
      {
	DEBUG_printf("6cups_dest_query_cb: '%s' with no value.", key);
	continue;
      }

      if (!_cups_strcasecmp(key, "usb_MFG") || !_cups_strcasecmp(key, "usb_MANU") || !_cups_strcasecmp(key, "usb_MANUFACTURER"))
      {
	cupsCopyString(make_and_model, value, sizeof(make_and_model));
      }
      else if (!_cups_strcasecmp(key, "usb_MDL") || !_cups_strcasecmp(key, "usb_MODEL"))
      {
	cupsCopyString(model, value, sizeof(model));
      }
      else if (!_cups_strcasecmp(key, "product") && !strstr(value, "Ghostscript"))
      {
	if (value[0] == '(')
	{
	  // Strip parenthesis...
	  if ((ptr = value + strlen(value) - 1) > value && *ptr == ')')
	    *ptr = '\0';

	  cupsCopyString(model, value + 1, sizeof(model));
	}
	else
	{
	  cupsCopyString(model, value, sizeof(model));
	}
      }
      else if (!_cups_strcasecmp(key, "ty"))
      {
	cupsCopyString(model, value, sizeof(model));

	if ((ptr = strchr(model, ',')) != NULL)
	  *ptr = '\0';
      }
      else if (!_cups_strcasecmp(key, "note"))
      {
        device->dest.num_options = cupsAddOption("printer-location", value, device->dest.num_options, &device->dest.options);
      }
      else if (!_cups_strcasecmp(key, "pdl"))
      {
        // Look for PDF-capable printers; only PDF-capable printers are shown.
        const char *start, *next;	// Pointer into value
        bool	have_pdf = false,	// Have PDF?
		have_raster = false;	// Have raster format support?

        for (start = value; start && *start; start = next)
        {
          if (!_cups_strncasecmp(start, "application/pdf", 15) && (!start[15] || start[15] == ','))
          {
            have_pdf = true;
            break;
          }
          else if ((!_cups_strncasecmp(start, "image/pwg-raster", 16) && (!start[16] || start[16] == ',')) || (!_cups_strncasecmp(start, "image/urf", 9) && (!start[9] || start[9] == ',')))
          {
            have_raster = true;
            break;
          }

          if ((next = strchr(start, ',')) != NULL)
            next ++;
        }

        if (!have_pdf && !have_raster)
          device->state = _CUPS_DNSSD_INCOMPATIBLE;
      }
      else if (!_cups_strcasecmp(key, "printer-type"))
      {
        // Value is either NNNN or 0xXXXX
	saw_printer_type = true;
        type             = (cups_ptype_t)strtol(value, NULL, 0) | CUPS_PRINTER_DISCOVERED;
      }
      else if (!saw_printer_type)
      {
	if (!_cups_strcasecmp(key, "air") && !_cups_strcasecmp(value, "t"))
	{
	  type |= CUPS_PRINTER_AUTHENTICATED;
	}
	else if (!_cups_strcasecmp(key, "bind") && !_cups_strcasecmp(value, "t"))
	{
	  type |= CUPS_PRINTER_BIND;
	}
	else if (!_cups_strcasecmp(key, "collate") && !_cups_strcasecmp(value, "t"))
	{
	  type |= CUPS_PRINTER_COLLATE;
	}
	else if (!_cups_strcasecmp(key, "color") && !_cups_strcasecmp(value, "t"))
	{
	  type |= CUPS_PRINTER_COLOR;
	}
	else if (!_cups_strcasecmp(key, "copies") && !_cups_strcasecmp(value, "t"))
	{
	  type |= CUPS_PRINTER_COPIES;
	}
	else if (!_cups_strcasecmp(key, "duplex") && !_cups_strcasecmp(value, "t"))
	{
	  type |= CUPS_PRINTER_DUPLEX;
	}
	else if (!_cups_strcasecmp(key, "fax") && !_cups_strcasecmp(value, "t"))
	{
	  type |= CUPS_PRINTER_MFP;
	}
	else if (!_cups_strcasecmp(key, "papercustom") && !_cups_strcasecmp(value, "t"))
	{
	  type |= CUPS_PRINTER_VARIABLE;
	}
	else if (!_cups_strcasecmp(key, "papermax"))
	{
	  if (!_cups_strcasecmp(value, "legal-a4"))
	    type |= CUPS_PRINTER_SMALL;
	  else if (!_cups_strcasecmp(value, "isoc-a2"))
	    type |= CUPS_PRINTER_MEDIUM;
	  else if (!_cups_strcasecmp(value, ">isoc-a2"))
	    type |= CUPS_PRINTER_LARGE;
	}
	else if (!_cups_strcasecmp(key, "punch") && !_cups_strcasecmp(value, "t"))
	{
	  type |= CUPS_PRINTER_PUNCH;
	}
	else if (!_cups_strcasecmp(key, "scan") && !_cups_strcasecmp(value, "t"))
	{
	  type |= CUPS_PRINTER_MFP;
	}
	else if (!_cups_strcasecmp(key, "sort") && !_cups_strcasecmp(value, "t"))
	{
	  type |= CUPS_PRINTER_SORT;
	}
	else if (!_cups_strcasecmp(key, "staple") && !_cups_strcasecmp(value, "t"))
	{
	  type |= CUPS_PRINTER_STAPLE;
	}
      }
    }

    // Save the printer-xxx values...
    if (make_and_model[0])
    {
      cupsConcatString(make_and_model, " ", sizeof(make_and_model));
      cupsConcatString(make_and_model, model, sizeof(make_and_model));

      device->dest.num_options = cupsAddOption("printer-make-and-model", make_and_model, device->dest.num_options, &device->dest.options);
    }
    else
    {
      device->dest.num_options = cupsAddOption("printer-make-and-model", model, device->dest.num_options, &device->dest.options);
    }

    device->type = type;
    snprintf(value, sizeof(value), "%u", type);
    device->dest.num_options = cupsAddOption("printer-type", value, device->dest.num_options, &device->dest.options);

    // Save the URI...
    cups_dnssd_unquote(uriname, device->fullname, sizeof(uriname));
    httpAssembleURI(HTTP_URI_CODING_ALL, uri, sizeof(uri), !strcmp(device->regtype, "_ipps._tcp") ? "ipps" : "ipp", NULL, uriname, 0, saw_printer_type ? "/cups" : "/");

    DEBUG_printf("6cups_dnssd_query: device-uri=\"%s\"", uri);

    device->dest.num_options = cupsAddOption("device-uri", uri, device->dest.num_options, &device->dest.options);
  }
  else
  {
    DEBUG_printf("6cups_dnssd_query: Ignoring TXT record for '%s'.", fullname);
  }
}


//
// 'cups_dest_resolve()' - Resolve a Bonjour printer URI.
//

static const char *			// O - Resolved URI or NULL
cups_dest_resolve(
    cups_dest_t    *dest,		// I - Destination
    const char     *uri,		// I - Current printer URI
    int            msec,		// I - Time in milliseconds
    int            *cancel,		// I - Pointer to "cancel" variable
    cups_dest_cb_t cb,			// I - Callback
    void           *user_data)		// I - User data for callback
{
  char			tempuri[1024];	// Temporary URI buffer
  _cups_dnssd_resdata_t	resolve;	// Resolve data


  // Resolve the URI...
  resolve.cancel = cancel;
  gettimeofday(&resolve.end_time, NULL);
  if (msec > 0)
  {
    resolve.end_time.tv_sec  += msec / 1000;
    resolve.end_time.tv_usec += (msec % 1000) * 1000;

    while (resolve.end_time.tv_usec >= 1000000)
    {
      resolve.end_time.tv_sec ++;
      resolve.end_time.tv_usec -= 1000000;
    }
  }
  else
  {
    resolve.end_time.tv_sec += 75;
  }

  if (cb)
    (*cb)(user_data, CUPS_DEST_FLAGS_UNCONNECTED | CUPS_DEST_FLAGS_RESOLVING, dest);

  if ((uri = httpResolveURI(uri, tempuri, sizeof(tempuri), HTTP_RESOLVE_DEFAULT, cups_dest_resolve_cb, &resolve)) == NULL)
  {
    _cupsSetError(IPP_STATUS_ERROR_INTERNAL, _("Unable to resolve printer-uri."), 1);

    if (cb)
      (*cb)(user_data, CUPS_DEST_FLAGS_UNCONNECTED | CUPS_DEST_FLAGS_ERROR, dest);

    return (NULL);
  }

  // Save the resolved URI...
  dest->num_options = cupsAddOption("device-uri", uri, dest->num_options, &dest->options);

  return (cupsGetOption("device-uri", dest->num_options, dest->options));
}


//
// 'cups_dest_resolve_cb()' - See if we should continue resolving.
//

static bool				// O - `true` to continue, `false` to stop
cups_dest_resolve_cb(void *context)	// I - Resolve data
{
  _cups_dnssd_resdata_t	*resolve = (_cups_dnssd_resdata_t *)context;
					// Resolve data
  struct timeval	curtime;	// Current time


  // If the cancel variable is set, return immediately.
  if (resolve->cancel && *(resolve->cancel))
  {
    DEBUG_puts("4cups_dest_resolve_cb: Canceled.");
    return (false);
  }

  // Otherwise check the end time...
  gettimeofday(&curtime, NULL);

  DEBUG_printf("4cups_dest_resolve_cb: curtime=%d.%06d, end_time=%d.%06d", (int)curtime.tv_sec, (int)curtime.tv_usec, (int)resolve->end_time.tv_sec, (int)resolve->end_time.tv_usec);

  return (curtime.tv_sec < resolve->end_time.tv_sec || (curtime.tv_sec == resolve->end_time.tv_sec && curtime.tv_usec < resolve->end_time.tv_usec));
}


//
// 'cups_dnssd_unquote()' - Unquote a name string.
//

static void
cups_dnssd_unquote(char       *dst,	// I - Destination buffer
                   const char *src,	// I - Source string
		   size_t     dstsize)	// I - Size of destination buffer
{
  char	*dstend = dst + dstsize - 1;	// End of destination buffer


  while (*src && dst < dstend)
  {
    if (*src == '\\')
    {
      src ++;
      if (isdigit(src[0] & 255) && isdigit(src[1] & 255) && isdigit(src[2] & 255))
      {
        *dst++ = ((((src[0] - '0') * 10) + src[1] - '0') * 10) + src[2] - '0';
	src += 3;
      }
      else
      {
        *dst++ = *src++;
      }
    }
    else
    {
      *dst++ = *src ++;
    }
  }

  *dst = '\0';
}


//
// 'cups_elapsed()' - Return the elapsed time in milliseconds.
//

static int				// O  - Elapsed time in milliseconds
cups_elapsed(struct timeval *t)		// IO - Previous time
{
  int			msecs;		// Milliseconds
  struct timeval	nt;		// New time


  gettimeofday(&nt, NULL);

  msecs = (int)(1000 * (nt.tv_sec - t->tv_sec) + (nt.tv_usec - t->tv_usec) / 1000);

  *t = nt;

  return (msecs);
}


//
// 'cups_enum_dests()' - Enumerate destinations from a specific server.
//

static bool				// O - `true` on success, `false` on failure
cups_enum_dests(
  http_t         *http,                 // I - Connection to scheduler
  unsigned       flags,                 // I - Enumeration flags
  int            msec,                  // I - Timeout in milliseconds, -1 for indefinite
  int            *cancel,               // I - Pointer to "cancel" variable
  cups_ptype_t   type,                  // I - Printer type bits
  cups_ptype_t   mask,                  // I - Mask for printer type bits
  cups_dest_cb_t cb,                    // I - Callback function
  void           *user_data)            // I - User data
{
  size_t	i, j, k,		// Looping vars
		num_dests,		// Number of destinations
		num_devices;		// Number of devices
  cups_dest_t   *dests = NULL,		// Destinations
                *dest;			// Current destination
  cups_option_t	*option;		// Current option
  const char	*user_default;		// Default printer from environment
  size_t	count,			// Number of queries started
		completed;		// Number of completed queries
  int		remaining;		// Remainder of timeout
  struct timeval curtime;               // Current time
  _cups_dnssd_data_t data;		// Data for callback
  _cups_dnssd_device_t *device;         // Current device
  cups_dnssd_t	*dnssd = NULL;		// DNS-SD context
  char		filename[1024];		// Local lpoptions file
  cups_array_t	*proflist;		// Profiles list
  const char	*profname;		// Profile filename
  char		profpath[1024];		// Profiles path
  _cups_profiles_t profiles;		// Profiles
  const char	*profile;		// Profile URI/hostname
  _cups_globals_t *cg = _cupsGlobals();	// Pointer to library globals


  DEBUG_printf("cups_enum_dests(flags=%x, msec=%d, cancel=%p, type=%x, mask=%x, cb=%p, user_data=%p)", flags, msec, (void *)cancel, type, mask, (void *)cb, (void *)user_data);

  // Range check input...
  (void)flags;

  if (!cb)
  {
    DEBUG_puts("1cups_enum_dests: No callback, returning 0.");
    return (false);
  }

  // Load the /etc/cups/lpoptions and ~/.cups/lpoptions files...
  memset(&data, 0, sizeof(data));

  user_default = _cupsGetUserDefault(data.def_name, sizeof(data.def_name));

  snprintf(filename, sizeof(filename), "%s/lpoptions", cg->sysconfig);
  data.num_dests = cups_get_dests(filename, NULL, NULL, 1, user_default != NULL, data.num_dests, &data.dests);

  if (cg->userconfig)
  {
    snprintf(filename, sizeof(filename), "%s/lpoptions", cg->userconfig);

    data.num_dests = cups_get_dests(filename, NULL, NULL, 1, user_default != NULL, data.num_dests, &data.dests);
  }

  if (!user_default && (dest = cupsGetDest(NULL, NULL, data.num_dests, data.dests)) != NULL)
  {
    // Use an lpoptions default printer...
    if (dest->instance)
      snprintf(data.def_name, sizeof(data.def_name), "%s/%s", dest->name, dest->instance);
    else
      cupsCopyString(data.def_name, dest->name, sizeof(data.def_name));
  }
  else
  {
    const char	*default_printer;	// Server default printer

    if ((default_printer = cupsGetDefault(http)) != NULL)
      cupsCopyString(data.def_name, default_printer, sizeof(data.def_name));
  }

  if (data.def_name[0])
  {
    // Separate printer and instance name...
    if ((data.def_instance = strchr(data.def_name, '/')) != NULL)
      *data.def_instance++ = '\0';
  }

  DEBUG_printf("1cups_enum_dests: def_name=\"%s\", def_instance=\"%s\"", data.def_name, data.def_instance);

  // Get profiles...
  proflist = cupsArrayNew(/*cb*/NULL, /*cb_data*/NULL, /*hash_cb*/NULL, /*hashsize*/0, (cups_acopy_cb_t)strdup, (cups_afree_cb_t)free);
  cups_init_profiles(&profiles);

  snprintf(profpath, sizeof(profpath), "%s/profiles", cg->sysconfig);
  cups_scan_profiles(proflist, profpath, &profiles.prof_mtime);

  snprintf(profpath, sizeof(profpath), "%s/profiles", cg->userconfig);
  cups_scan_profiles(proflist, profpath, &profiles.prof_mtime);

  if (profiles.prof_mtime > cg->profiles.prof_mtime)
  {
    // Save new profiles...
    for (profname = (const char *)cupsArrayGetFirst(proflist); profname; profname = (const char *)cupsArrayGetNext(proflist))
      cups_load_profile(&profiles, profname);

    _cupsFreeProfiles(&cg->profiles);
    cg->profiles       = profiles;
    cg->time_profdests = 0;
  }
  else
  {
    // Free old prof
    _cupsFreeProfiles(&profiles);
  }

  cupsArrayDelete(proflist);

  // Get ready to enumerate...
  cupsRWInit(&data.rwlock);

  data.type      = type;
  data.mask      = mask;
  data.cb        = cb;
  data.user_data = user_data;
  data.devices   = cupsArrayNew((cups_array_cb_t)cups_dnssd_compare_devices, NULL, NULL, 0, NULL, (cups_afree_cb_t)cups_dnssd_free_device);

  if (!(mask & CUPS_PRINTER_DISCOVERED) || !(type & CUPS_PRINTER_DISCOVERED))
  {
    // Get the list of local printers and pass them to the callback function...
    num_dests = _cupsGetDests(http, IPP_OP_CUPS_GET_PRINTERS, NULL, &dests, type, mask);

    if (data.def_name[0])
    {
      // Lookup the named default printer and instance and make it the default...
      if ((dest = cupsGetDest(data.def_name, data.def_instance, num_dests, dests)) != NULL)
      {
	DEBUG_printf("1cups_enum_dests: Setting is_default on \"%s/%s\".", dest->name, dest->instance);
        dest->is_default = true;
      }
    }

    for (i = num_dests, dest = dests; i > 0 && (!cancel || !*cancel); i --, dest ++)
    {
      cups_dest_t	*user_dest;	// Destination from lpoptions
      const char	*device_uri;	// Device URI

      if ((user_dest = cupsGetDest(dest->name, NULL, data.num_dests, data.dests)) != NULL)
      {
        // Apply user defaults to this destination for all instances...
        for (j = (size_t)(user_dest - data.dests); j < data.num_dests; j ++, user_dest ++)
        {
          if (_cups_strcasecmp(user_dest->name, dest->name))
          {
            j = data.num_dests;
            break;
          }

	  for (k = dest->num_options, option = dest->options; k > 0; k --, option ++)
	    user_dest->num_options = cupsAddOption(option->name, option->value, user_dest->num_options, &user_dest->options);

          if (!(*cb)(user_data, i > 1 ? CUPS_DEST_FLAGS_MORE : CUPS_DEST_FLAGS_NONE, user_dest))
            break;
        }

        if (j < data.num_dests)
          break;
      }
      else if (!(*cb)(user_data, i > 1 ? CUPS_DEST_FLAGS_MORE : CUPS_DEST_FLAGS_NONE, dest))
        break;

      if (!dest->instance && (device_uri = cupsGetOption("device-uri", dest->num_options, dest->options)) != NULL && !strncmp(device_uri, "dnssd://", 8))
      {
        // Add existing queue using service name, etc. so we don't list it again...
        char    scheme[32],             // URI scheme
                userpass[32],           // Username:password
                serviceName[256],       // Service name (host field)
                resource[256],          // Resource (options)
                *regtype,               // Registration type
                *replyDomain;           // Registration domain
        int     port;                   // Port number (not used)

        if (httpSeparateURI(HTTP_URI_CODING_ALL, device_uri, scheme, sizeof(scheme), userpass, sizeof(userpass), serviceName, sizeof(serviceName), &port, resource, sizeof(resource)) >= HTTP_URI_STATUS_OK)
        {
          if ((regtype = strstr(serviceName, "._ipp")) != NULL)
          {
            *regtype++ = '\0';

            if ((replyDomain = strstr(regtype, "._tcp.")) != NULL)
            {
              replyDomain[5] = '\0';
              replyDomain += 6;

              if ((device = cups_dnssd_get_device(&data, serviceName, regtype, replyDomain)) != NULL)
                device->state = _CUPS_DNSSD_ACTIVE;
            }
          }
        }
      }
    }

    cupsFreeDests(num_dests, dests);

    if (i > 0 || msec == 0)
      goto enum_finished;
  }

  // Return early if the caller doesn't want to do discovery...
  if ((mask & CUPS_PRINTER_DISCOVERED) && !(type & CUPS_PRINTER_DISCOVERED))
    goto enum_finished;

  // Get profile printers...
  if ((time(NULL) - cg->time_profdests) >= 60)
    cups_update_profiles(cg, cancel);

  for (i = cg->num_profdests, dest = cg->profdests; i > 0 && (!cancel || !*cancel); i --, dest ++)
  {
    cups_dest_t	*user_dest;	// Destination from lpoptions
    const char	*device_uri;	// Device URI

    if ((user_dest = cupsGetDest(dest->name, NULL, data.num_dests, data.dests)) != NULL)
    {
      // Apply user defaults to this destination for all instances...
      for (j = (size_t)(user_dest - data.dests); j < data.num_dests; j ++, user_dest ++)
      {
	if (_cups_strcasecmp(user_dest->name, dest->name))
	{
	  j = data.num_dests;
	  break;
	}

	for (k = dest->num_options, option = dest->options; k > 0; k --, option ++)
	  user_dest->num_options = cupsAddOption(option->name, option->value, user_dest->num_options, &user_dest->options);

	if (!(*cb)(user_data, i > 1 ? CUPS_DEST_FLAGS_MORE : CUPS_DEST_FLAGS_NONE, user_dest))
	  break;
      }

      if (j < data.num_dests)
	break;
    }
    else if (!(*cb)(user_data, i > 1 ? CUPS_DEST_FLAGS_MORE : CUPS_DEST_FLAGS_NONE, dest))
      break;
  }

  // Get DNS-SD printers...
  gettimeofday(&curtime, NULL);

  if ((dnssd = cupsDNSSDNew(dnssd_error_cb, NULL)) == NULL)
  {
    DEBUG_puts("1cups_enum_dests: Unable to create service browser, returning 0.");

    cupsFreeDests(data.num_dests, data.dests);
    cupsArrayDelete(data.devices);

    return (false);
  }

  if (!cupsDNSSDBrowseNew(dnssd, CUPS_DNSSD_IF_INDEX_ANY, "_ipp._tcp", /*domain*/NULL, cups_dest_browse_cb, &data))
  {
    DEBUG_puts("1cups_enum_dests: Unable to create IPP browser, returning 0.");
    cupsDNSSDDelete(dnssd);

    cupsFreeDests(data.num_dests, data.dests);
    cupsArrayDelete(data.devices);

    return (false);
  }

  if (!cupsDNSSDBrowseNew(dnssd, CUPS_DNSSD_IF_INDEX_ANY, "_ipps._tcp", /*domain*/NULL, cups_dest_browse_cb, &data))
  {
    DEBUG_puts("1cups_enum_dests: Unable to create IPPS browser, returning 0.");
    cupsDNSSDDelete(dnssd);

    cupsFreeDests(data.num_dests, data.dests);
    cupsArrayDelete(data.devices);

    return (false);
  }

  if (msec < 0)
    remaining = INT_MAX;
  else
    remaining = msec;

  while (remaining > 0 && (!cancel || !*cancel))
  {
    // Check for input...
    DEBUG_printf("1cups_enum_dests: remaining=%d", remaining);

    cups_elapsed(&curtime);

    remaining -= cups_elapsed(&curtime);

    cupsRWLockRead(&data.rwlock);

    for (i = 0, num_devices = cupsArrayGetCount(data.devices), count = 0, completed = 0; i < num_devices; i ++)
    {
      device = cupsArrayGetElement(data.devices, i);

      if (device->query)
        count ++;

      if (device->state == _CUPS_DNSSD_ACTIVE)
        completed ++;

      if (!device->query && device->state == _CUPS_DNSSD_NEW)
      {
        DEBUG_printf("1cups_enum_dests: Querying '%s'.", device->fullname);

	if ((device->query = cupsDNSSDQueryNew(dnssd, CUPS_DNSSD_IF_INDEX_ANY, device->fullname, CUPS_DNSSD_RRTYPE_TXT, cups_dest_query_cb, &data)) != NULL)
        {
          count ++;
        }
        else
        {
          device->state = _CUPS_DNSSD_ERROR;

          DEBUG_puts("1cups_enum_dests: Query failed.");
        }
      }
      else if (device->query && device->state == _CUPS_DNSSD_PENDING)
      {
        completed ++;

        DEBUG_printf("1cups_enum_dests: Query for \"%s\" is complete.", device->fullname);

        if ((device->type & mask) == type)
        {
          cups_dest_t	*user_dest;	// Destination from lpoptions

          dest = &device->dest;

	  if ((user_dest = cupsGetDest(dest->name, dest->instance, data.num_dests, data.dests)) != NULL)
	  {
	    // Apply user defaults to this destination for all instances...
	    for (j = (size_t)(user_dest - data.dests); j < data.num_dests; j ++, user_dest ++)
	    {
	      if (_cups_strcasecmp(user_dest->name, dest->name))
	      {
		j = data.num_dests;
		break;
	      }

	      for (k = dest->num_options, option = dest->options; k > 0; k --, option ++)
		user_dest->num_options = cupsAddOption(option->name, option->value, user_dest->num_options, &user_dest->options);

	      if (!(*cb)(user_data, CUPS_DEST_FLAGS_NONE, user_dest))
		break;
	    }

	    if (j < data.num_dests)
	    {
	      remaining = -1;
	      break;
	    }
	  }
	  else
	  {
	    if (!strcasecmp(dest->name, data.def_name) && !data.def_instance)
	    {
	      DEBUG_printf("1cups_enum_dests: Setting is_default on discovered \"%s\".", dest->name);
	      dest->is_default = 1;
	    }

	    DEBUG_printf("1cups_enum_dests: Add callback for \"%s\".", device->dest.name);
	    if (!(*cb)(user_data, CUPS_DEST_FLAGS_NONE, dest))
	    {
	      remaining = -1;
	      break;
	    }
	  }
        }

        device->state = _CUPS_DNSSD_ACTIVE;
      }
    }

    DEBUG_printf("1cups_enum_dests: remaining=%d, completed=%u, count=%u, devices count=%u", remaining, (unsigned)completed, (unsigned)count, (unsigned)cupsArrayGetCount(data.devices));

    cupsRWUnlock(&data.rwlock);

    if (completed && completed == cupsArrayGetCount(data.devices))
      break;

    usleep(100000);
  }

  // Return...
  enum_finished:

  cupsDNSSDDelete(dnssd);

  cupsFreeDests(data.num_dests, data.dests);
  cupsArrayDelete(data.devices);

  DEBUG_puts("1cups_enum_dests: Returning true.");

  return (true);
}


//
// 'cups_find_dest()' - Find a destination using a binary search.
//

static size_t				// O - Index of match
cups_find_dest(const char  *name,	// I - Destination name
               const char  *instance,	// I - Instance or NULL
               size_t      num_dests,	// I - Number of destinations
	       cups_dest_t *dests,	// I - Destinations
	       size_t      prev,	// I - Previous index
	       int         *rdiff)	// O - Difference of match
{
  size_t	left,			// Low mark for binary search
		right,			// High mark for binary search
		current;		// Current index
  int		diff;			// Result of comparison
  cups_dest_t	key;			// Search key


  key.name     = (char *)name;
  key.instance = (char *)instance;

  if (prev < num_dests)
  {
    // Start search on either side of previous...
    if ((diff = cups_compare_dests(&key, dests + prev)) == 0 || (diff < 0 && prev == 0) || (diff > 0 && prev == (num_dests - 1)))
    {
      *rdiff = diff;
      return (prev);
    }
    else if (diff < 0)
    {
      // Start with previous on right side...
      left  = 0;
      right = prev;
    }
    else
    {
      // Start wih previous on left side...
      left  = prev;
      right = num_dests - 1;
    }
  }
  else
  {
    // Start search in the middle...
    left  = 0;
    right = num_dests - 1;
  }

  do
  {
    current = (left + right) / 2;
    diff    = cups_compare_dests(&key, dests + current);

    if (diff == 0)
      break;
    else if (diff < 0)
      right = current;
    else
      left = current;
  }
  while ((right - left) > 1);

  if (diff != 0)
  {
    // Check the last 1 or 2 elements...
    if ((diff = cups_compare_dests(&key, dests + left)) <= 0)
    {
      current = left;
    }
    else
    {
      diff    = cups_compare_dests(&key, dests + right);
      current = right;
    }
  }

  // Return the closest destination and the difference...
  *rdiff = diff;

  return (current);
}


//
// 'cups_get_cb()' - Collect enumerated destinations.
//

static bool				// O - `true` to continue, `false` to stop
cups_get_cb(_cups_getdata_t *data,      // I - Data from cupsGetDests
            unsigned        flags,      // I - Enumeration flags
            cups_dest_t     *dest)      // I - Destination
{
  if (flags & CUPS_DEST_FLAGS_REMOVED)
  {
    // Remove destination from array...
    data->num_dests = cupsRemoveDest(dest->name, dest->instance, data->num_dests, &data->dests);
  }
  else
  {
    // Add destination to array...
    data->num_dests = cupsCopyDest(dest, data->num_dests, &data->dests);
  }

  return (true);
}


//
// 'cups_get_default()' - Get the default destination from an lpoptions file.
//

static char *				// O - Default destination or NULL
cups_get_default(const char *filename,	// I - File to read
                 char       *namebuf,	// I - Name buffer
		 size_t     namesize,	// I - Size of name buffer
		 const char **instance)	// I - Instance
{
  cups_file_t	*fp;			// lpoptions file
  char		line[8192],		// Line from file
		*value,			// Value for line
		*nameptr;		// Pointer into name
  int		linenum;		// Current line


  *namebuf = '\0';

  if ((fp = cupsFileOpen(filename, "r")) != NULL)
  {
    linenum  = 0;

    while (cupsFileGetConf(fp, line, sizeof(line), &value, &linenum))
    {
      if (!_cups_strcasecmp(line, "default") && value)
      {
        cupsCopyString(namebuf, value, namesize);

	if ((nameptr = strchr(namebuf, ' ')) != NULL)
	  *nameptr = '\0';
	if ((nameptr = strchr(namebuf, '\t')) != NULL)
	  *nameptr = '\0';

	if ((nameptr = strchr(namebuf, '/')) != NULL)
	  *nameptr++ = '\0';

        *instance = nameptr;
	break;
      }
    }

    cupsFileClose(fp);
  }

  return (*namebuf ? namebuf : NULL);
}


//
// 'cups_get_dests()' - Get destinations from a file.
//

static size_t				// O - Number of destinations
cups_get_dests(
    const char  *filename,		// I - File to read from
    const char  *match_name,		// I - Destination name we want
    const char  *match_inst,		// I - Instance name we want
    bool        load_all,		// I - Load all saved destinations?
    bool        user_default_set,	// I - User default printer set?
    size_t      num_dests,		// I - Number of destinations
    cups_dest_t **dests)		// IO - Destinations
{
  size_t	i;			// Looping var
  cups_dest_t	*dest;			// Current destination
  cups_file_t	*fp;			// File pointer
  char		line[8192],		// Line from file
		*lineptr,		// Pointer into line
		*name,			// Name of destination/option
		*instance;		// Instance of destination
  int		linenum;		// Current line number


  DEBUG_printf("7cups_get_dests(filename=\"%s\", match_name=\"%s\", match_inst=\"%s\", load_all=%s, user_default_set=%s, num_dests=%u, dests=%p)", filename, match_name, match_inst, load_all ? "true" : "false", user_default_set ? "true" : "false", (unsigned)num_dests, (void *)dests);

  // Try to open the file...
  if ((fp = cupsFileOpen(filename, "r")) == NULL)
    return (num_dests);

  // Read each printer; each line looks like:
  //
  //   Dest name[/instance] options
  //   Default name[/instance] options
  linenum = 0;

  while (cupsFileGetConf(fp, line, sizeof(line), &lineptr, &linenum))
  {
    // See what type of line it is...
    DEBUG_printf("9cups_get_dests: linenum=%d line=\"%s\" lineptr=\"%s\"", linenum, line, lineptr);

    if ((_cups_strcasecmp(line, "dest") && _cups_strcasecmp(line, "default")) || !lineptr)
    {
      DEBUG_puts("9cups_get_dests: Not a dest or default line...");
      continue;
    }

    name = lineptr;

    // Search for an instance...
    while (!isspace(*lineptr & 255) && *lineptr && *lineptr != '/')
      lineptr ++;

    if (*lineptr == '/')
    {
      // Found an instance...
      *lineptr++ = '\0';
      instance = lineptr;

      // Search for an instance...
      while (!isspace(*lineptr & 255) && *lineptr)
	lineptr ++;
    }
    else
    {
      instance = NULL;
    }

    if (*lineptr)
      *lineptr++ = '\0';

    DEBUG_printf("9cups_get_dests: name=\"%s\", instance=\"%s\"", name, instance);

    // Match and/or ignore missing destinations...
    if (match_name)
    {
      if (_cups_strcasecmp(name, match_name) || (!instance && match_inst) || (instance && !match_inst) || (instance && _cups_strcasecmp(instance, match_inst)))
	continue;

      dest = *dests;
    }
    else if (!load_all && cupsGetDest(name, NULL, num_dests, *dests) == NULL)
    {
      DEBUG_puts("9cups_get_dests: Not found!");
      continue;
    }
    else
    {
      // Add the destination...
      num_dests = cupsAddDest(name, instance, num_dests, dests);

      if ((dest = cupsGetDest(name, instance, num_dests, *dests)) == NULL)
      {
        // Out of memory!
        DEBUG_puts("9cups_get_dests: Could not find destination after adding, must be out of memory.");
        break;
      }
    }

    // Add options until we hit the end of the line...
    dest->num_options = cupsParseOptions(lineptr, dest->num_options, &(dest->options));

    // If we found what we were looking for, stop now...
    if (match_name)
      break;

    // Set this as default if needed...
    if (!user_default_set && !_cups_strcasecmp(line, "default"))
    {
      DEBUG_puts("9cups_get_dests: Setting as default...");

      for (i = 0; i < num_dests; i ++)
        (*dests)[i].is_default = 0;

      dest->is_default = 1;
    }
  }

  // Close the file and return...
  cupsFileClose(fp);

  return (num_dests);
}


//
// 'cups_init_profiles()' - Initialize profiles.
//

static void
cups_init_profiles(
    _cups_profiles_t *profiles)		// I - Profiles
{
  memset(profiles, 0, sizeof(_cups_profiles_t));
  profiles->prof_geolimit = -1.0f;
}


//
// 'cups_load_profile()' - Load printers and servers from a profile.
//

static void
cups_load_profile(
    _cups_profiles_t *profiles,		// I - Profile data
    const char      *filename)		// I - Profile filename
{
  cups_file_t	*fp;			// Profile file
  int		linenum = 0;		// Line number
  char		line[1024],		// Line
		*value;			// Value from line


  if ((fp = cupsFileOpen(filename, "r")) == NULL)
    return;

  while (cupsFileGetConf(fp, line, sizeof(line), &value, &linenum))
  {
    // Ignore lines with no value...
    if (!value)
      continue;

    // Look for profile/filter directives...
    if (!_cups_strcasecmp(line, "FilterGeoLocation"))
    {
      // FilterGeoLocation none
      // FilterGeoLocation DISTANCE-METERS
    }
    else if (!_cups_strcasecmp(line, "FilterLocation"))
    {
      // FilterLocation none
      // FilterLocation "LOCATION-STRING"
      // FilterLocation /LOCATION-REGEX/
      char	*locptr;		// Pointer into location value

      if (!_cups_strcasecmp(value, "none"))
      {
        cupsArrayDelete(profiles->prof_locations);
        profiles->prof_locations = NULL;
      }
      else if (*value == '\"' || *value == '\'' || *value == '/')
      {
        if ((locptr = value + strlen(value) - 1) > value && *locptr == *value)
        {
          *locptr = '\0';

          if (!profiles->prof_locations)
            profiles->prof_locations = cupsArrayNew(/*cb*/NULL, /*cb_data*/NULL, /*hash_cb*/NULL, /*hashsize*/0, (cups_acopy_cb_t)strdup, (cups_afree_cb_t)free);

          cupsArrayAdd(profiles->prof_locations, value);
        }
      }
    }
    else if (!_cups_strcasecmp(line, "FilterType"))
    {
      // FilterType {any,mono,color,duplex,simplex,staple,small,mediumm,large}
      cups_array_t	*types = cupsArrayNewStrings(value, ',');
					// Array of type strings
      const char	*type;		// Current type

      for (type = (const char *)cupsArrayGetFirst(types); type; type = (const char *)cupsArrayGetNext(types))
      {
        if (!_cups_strcasecmp(type, "any"))
        {
          profiles->prof_ptype = 0;
          profiles->prof_pmask = 0;
	}
        else if (!_cups_strcasecmp(type, "color"))
        {
          profiles->prof_ptype |= CUPS_PRINTER_COLOR;
          profiles->prof_pmask |= CUPS_PRINTER_COLOR;
	}
        else if (!_cups_strcasecmp(type, "mono"))
        {
          profiles->prof_pmask |= CUPS_PRINTER_COLOR;
	}
        else if (!_cups_strcasecmp(type, "duplex"))
        {
          profiles->prof_ptype |= CUPS_PRINTER_DUPLEX;
          profiles->prof_pmask |= CUPS_PRINTER_DUPLEX;
	}
        else if (!_cups_strcasecmp(type, "simplex"))
        {
          profiles->prof_pmask |= CUPS_PRINTER_DUPLEX;
	}
        else if (!_cups_strcasecmp(type, "staple"))
        {
          profiles->prof_ptype |= CUPS_PRINTER_STAPLE;
          profiles->prof_pmask |= CUPS_PRINTER_STAPLE;
	}
        else if (!_cups_strcasecmp(type, "punch"))
        {
          profiles->prof_ptype |= CUPS_PRINTER_PUNCH;
          profiles->prof_pmask |= CUPS_PRINTER_PUNCH;
	}
        else if (!_cups_strcasecmp(type, "fold"))
        {
          profiles->prof_ptype |= CUPS_PRINTER_FOLD;
          profiles->prof_pmask |= CUPS_PRINTER_FOLD;
	}
        else if (!_cups_strcasecmp(type, "small"))
        {
          profiles->prof_ptype |= CUPS_PRINTER_SMALL;
          profiles->prof_pmask |= CUPS_PRINTER_SMALL | CUPS_PRINTER_MEDIUM | CUPS_PRINTER_LARGE;
	}
        else if (!_cups_strcasecmp(type, "medium"))
        {
          profiles->prof_ptype |= CUPS_PRINTER_MEDIUM;
          profiles->prof_pmask |= CUPS_PRINTER_SMALL | CUPS_PRINTER_MEDIUM | CUPS_PRINTER_LARGE;
	}
        else if (!_cups_strcasecmp(type, "large"))
        {
          profiles->prof_ptype |= CUPS_PRINTER_LARGE;
          profiles->prof_pmask |= CUPS_PRINTER_SMALL | CUPS_PRINTER_MEDIUM | CUPS_PRINTER_LARGE;
	}
      }

      cupsArrayDelete(types);
    }
    else if (!_cups_strcasecmp(line, "Printer"))
    {
      // Printer PRINTER-URI
      if (!profiles->prof_printers)
       profiles->prof_printers = cupsArrayNew(/*cb*/NULL, /*cb_data*/NULL, /*hash_cb*/NULL, /*hashsize*/0, (cups_acopy_cb_t)strdup, (cups_afree_cb_t)free);

      cupsArrayAdd(profiles->prof_printers, value);
    }
    else if (!_cups_strcasecmp(line, "Server"))
    {
      // Server HOSTNAME[:PORT]
      if (!profiles->prof_servers)
       profiles->prof_servers = cupsArrayNew(/*cb*/NULL, /*cb_data*/NULL, /*hash_cb*/NULL, /*hashsize*/0, (cups_acopy_cb_t)strdup, (cups_afree_cb_t)free);

      cupsArrayAdd(profiles->prof_servers, value);
    }
    else if (!_cups_strcasecmp(line, "System"))
    {
      // System SYSTEM-URI
      if (!profiles->prof_systems)
       profiles->prof_systems = cupsArrayNew(/*cb*/NULL, /*cb_data*/NULL, /*hash_cb*/NULL, /*hashsize*/0, (cups_acopy_cb_t)strdup, (cups_afree_cb_t)free);

      cupsArrayAdd(profiles->prof_systems, value);
    }
  }

  cupsFileClose(fp);
}


//
// 'cups_make_string()' - Make a comma-separated string of values from an IPP
//                        attribute.
//

static char *				// O - New string
cups_make_string(
    ipp_attribute_t *attr,		// I - Attribute to convert
    char            *buffer,		// I - Buffer
    size_t          bufsize)		// I - Size of buffer
{
  size_t	i;			// Looping var
  char		*ptr,			// Pointer into buffer
		*end,			// Pointer to end of buffer
		*valptr;		// Pointer into string attribute


  // Return quickly if we have a single string value...
  if (attr->num_values == 1 && attr->value_tag != IPP_TAG_INTEGER && attr->value_tag != IPP_TAG_ENUM && attr->value_tag != IPP_TAG_BOOLEAN && attr->value_tag != IPP_TAG_RANGE)
    return (attr->values[0].string.text);

  // Copy the values to the string, separating with commas and escaping strings
  // as needed...
  end = buffer + bufsize - 1;

  for (i = 0, ptr = buffer; i < attr->num_values && ptr < end; i ++)
  {
    if (i)
      *ptr++ = ',';

    switch (attr->value_tag)
    {
      case IPP_TAG_INTEGER :
      case IPP_TAG_ENUM :
	  snprintf(ptr, (size_t)(end - ptr + 1), "%d", attr->values[i].integer);
	  break;

      case IPP_TAG_BOOLEAN :
	  if (attr->values[i].boolean)
	    cupsCopyString(ptr, "true", (size_t)(end - ptr + 1));
	  else
	    cupsCopyString(ptr, "false", (size_t)(end - ptr + 1));
	  break;

      case IPP_TAG_RANGE :
	  if (attr->values[i].range.lower == attr->values[i].range.upper)
	    snprintf(ptr, (size_t)(end - ptr + 1), "%d", attr->values[i].range.lower);
	  else
	    snprintf(ptr, (size_t)(end - ptr + 1), "%d-%d", attr->values[i].range.lower, attr->values[i].range.upper);
	  break;

      default :
	  for (valptr = attr->values[i].string.text;
	       *valptr && ptr < end;)
	  {
	    if (strchr(" \t\n\\\'\"", *valptr))
	    {
	      if (ptr >= (end - 1))
	        break;

	      *ptr++ = '\\';
	    }

	    *ptr++ = *valptr++;
	  }

	  *ptr = '\0';
	  break;
    }

    ptr += strlen(ptr);
  }

  *ptr = '\0';

  return (buffer);
}


//
// 'cups_name_cb()' - Find an enumerated destination.
//

static bool				// O - `true` to continue, `false` to stop
cups_name_cb(_cups_namedata_t *data,    // I - Data from cupsGetNamedDest
             unsigned         flags,    // I - Enumeration flags
             cups_dest_t      *dest)    // I - Destination
{
  DEBUG_printf("2cups_name_cb(data=%p(%s), flags=%x, dest=%p(%s)", (void *)data, data->name, flags, (void *)dest, dest->name);

  if (!(flags & CUPS_DEST_FLAGS_REMOVED) && !dest->instance && !strcasecmp(data->name, dest->name))
  {
    // Copy destination and stop enumeration...
    cupsCopyDest(dest, 0, &data->dest);
    return (false);
  }

  return (true);
}


//
// 'cups_queue_name()' - Create a local queue name based on the service name.
//

static void
cups_queue_name(
    char       *name,			// I - Name buffer
    const char *serviceName,		// I - Service name
    size_t     namesize)		// I - Size of name buffer
{
  const char	*ptr;			// Pointer into serviceName
  char		*nameptr;		// Pointer into name


  for (nameptr = name, ptr = serviceName; *ptr && nameptr < (name + namesize - 1); ptr ++)
  {
    // Sanitize the printer name...
    if (_cups_isalnum(*ptr))
      *nameptr++ = *ptr;
    else if (nameptr == name || nameptr[-1] != '_')
      *nameptr++ = '_';
  }

  // Remove an underscore if it is the last character and isn't the only
  // character in the name...
  if (nameptr > (name + 1) && nameptr[-1] == '_')
    nameptr --;

  *nameptr = '\0';
}


//
// 'cups_scan_profiles()' - Scan for profile files.
//

static void
cups_scan_profiles(
    cups_array_t *profiles,		// I  - Profile array
    const char   *path,			// I  - Profile directory
    time_t       *mtime)		// IO - Newest modification time
{
  cups_dir_t	*dir;			// Directory
  cups_dentry_t	*dent;			// Directory entry
  char		filename[1024];		// Profile filename


  // Find all of the files in the specified directory...
  if ((dir = cupsDirOpen(path)) != NULL)
  {
    while ((dent = cupsDirRead(dir)) != NULL)
    {
      // Ignore anything other than plain files...
      if (!S_ISREG(dent->fileinfo.st_mode))
        continue;

      // Add this file and update the mtime value as needed...
      snprintf(filename, sizeof(filename), "%s/%s", path, dent->filename);
      cupsArrayAdd(profiles, filename);

      if (dent->fileinfo.st_mtime > *mtime)
        *mtime = dent->fileinfo.st_mtime;
    }

    cupsDirClose(dir);
  }
}


//
// 'cups_update_profiles()' - Update the list of available profile printers.
//

static void
cups_update_profiles(
    _cups_globals_t *cg,		// Global data
    int             *cancel)		// I - Cancel variable
{
  const char	*profile;		// Current profile URI/server


  // Clear existing (cached) printers...
  cupsFreeDests(cg->num_profdests, cg->profdests);
  cg->time_profdests = 0;
  cg->num_profdests  = 0;
  cg->profdests      = NULL;

  // Loop through all of the printers...
  for (profile = (const char *)cupsArrayGetFirst(cg->profiles.prof_printers); (!cancel || !*cancel) && profile; profile = (const char *)cupsArrayGetNext(cg->profiles.prof_printers))
  {
    char		scheme[32],	// URI scheme
			userpass[256],	// URI username:password (ignored)
			host[256],	// URI hostname
			resource[256];	// URI resource
    int			port;		// URI port
    http_encryption_t	encryption;	// Encryption to use
    http_t		*prof_http;	// Connection to host

    if (httpSeparateURI(HTTP_URI_CODING_ALL, profile, scheme, sizeof(scheme), userpass, sizeof(userpass), host, sizeof(host), &port, resource, sizeof(resource)) < HTTP_URI_STATUS_OK)
      continue;

    if (port == 443 || !strcmp(scheme, "ipps"))
      encryption = HTTP_ENCRYPTION_ALWAYS;
    else
      encryption = HTTP_ENCRYPTION_IF_REQUESTED;

    if ((prof_http = httpConnect(host, port, /*addrlist*/NULL, AF_UNSPEC, encryption, /*blocking*/true, /*msec*/1000, /*cancel*/cancel)) != NULL)
    {
      // TODO: Add dest
      httpClose(prof_http);
    }
  }

  if (cancel && *cancel)
    return;

  // If we got this far, update the cache time and return...
  cg->time_profdests = time(NULL);
}


//
// 'dnssd_error_cb()' - Report an error.
//

static void
dnssd_error_cb(void       *cb_data,	// I - Callback data (unused)
               const char *message)	// I - Message
{
  (void)cb_data;
  _cupsSetError(IPP_STATUS_ERROR_INTERNAL, message, 0);
}

