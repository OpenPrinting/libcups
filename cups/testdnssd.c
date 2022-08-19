//
// DNS-SD API test program for CUPS.
//
// Copyright © 2022 by OpenPrinting.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#include "test-internal.h"
#include "dnssd.h"
#include "thread.h"


//
// Local structures...
//

typedef struct testdata_s		// Test data structure
{
  cups_mutex_t	mutex;			// Mutex for access
  size_t	browse_count;		// Number of browse callbacks
  size_t	error_count;		// Number of error callbacks
  size_t	query_count;		// Number of query callbacks
  size_t	resolve_count;		// Number of resolve callbacks
  size_t	service_count;		// Number of service callbacks
} testdata_t;


//
// Local functions...
//

static void	browse_cb(cups_dnssd_browse_t *browse, void *cb_data, cups_dnssd_flags_t flags, uint32_t if_index, const char *name, const char *regtype, const char *domain);
static void	error_cb(void *cb_data, const char *message);
//static void	query_cb(cups_dnssd_query_t *query, void *cb_data, cups_dnssd_flags_t flags, uint32_t if_index, const char *fullname, uint16_t rrtype, const void *qdata, uint16_t qlen);
//static void	resolve_cb(cups_dnssd_resolve_t *res, void *cb_data, cups_dnssd_flags_t flags, uint32_t if_index, const char *fullname, const char *host, uint16_t port, size_t num_txt, cups_option_t *txt);
static void	service_cb(cups_dnssd_service_t *service, void *cb_data, cups_dnssd_flags_t flags, const char *name, const char *regtype, const char *domain);
static void	usage(const char *arg);


//
// 'main()' - Main entry.
//

int					// O - Exit status
main(int  argc,				// I - Number of command-line arguments
     char *argv[])			// I - Command-line arguments
{
  int			i;		// Looping var
  cups_dnssd_t		*dnssd;		// DNS-SD context
  cups_dnssd_browse_t	*browse;	// DNS-SD browse request
//  cups_dnssd_query_t	*query;		// DNS-SD query request
//  cups_dnssd_resolve_t	*resolve;	// DNS-SD resolve request
  cups_dnssd_service_t	*service;	// DNS-SD service registration
  size_t		num_txt;	// Number of TXT record key/value pairs
  cups_option_t		*txt;		// TXT record key/value pairs
  testdata_t		testdata;	// Test data


  // Clear test data...
  memset(&testdata, 0, sizeof(testdata));

  if (argc == 1)
  {
    // Do unit tests...
    testBegin("cupsDNSSDNew");
    if ((dnssd = cupsDNSSDNew(error_cb, &testdata)) != NULL)
      testEnd(true);
    else
      return (1);

    testBegin("cupsDNSSDBrowseNew(_ipp._tcp)");
    if ((browse = cupsDNSSDBrowseNew(dnssd, CUPS_DNSSD_IF_INDEX_ANY, "_ipp._tcp", NULL, browse_cb, &testdata)) != NULL)
      testEnd(true);
    else
      return (1);

    testBegin("cupsDNSSDServiceNew(Test Printer)");
    if ((service = cupsDNSSDServiceNew(dnssd, "Test Printer", service_cb, &testdata)) != NULL)
      testEnd(true);
    else
      return (1);

    num_txt = cupsAddOption("rp", "ipp/print", 0, &txt);

    testBegin("cupsDNSSDServiceAdd(_http._tcp)");
    if (cupsDNSSDServiceAdd(service, CUPS_DNSSD_IF_INDEX_ANY, "_http._tcp,_printer", NULL, NULL, 631, 0, NULL))
      testEnd(true);
    else
      return (1);

    testBegin("cupsDNSSDServiceAdd(_ipp._tcp)");
    if (cupsDNSSDServiceAdd(service, CUPS_DNSSD_IF_INDEX_ANY, "_ipp._tcp,_print", NULL, NULL, 631, num_txt, txt))
      testEnd(true);
    else
      return (1);

    cupsFreeOptions(num_txt, txt);

    testBegin("Wait for callbacks");

    for (i = 0; i < 30; i ++)
    {
      if (testdata.service_count != 0 && testdata.browse_count != 0)
        break;

      sleep(1);
    }

    testEnd(i < 30);

    cupsDNSSDDelete(dnssd);
  }
  else
  {
    usage(argv[1]);
  }

  return (0);
}


//
// 'browse_cb()' - Browse callback.
//

static void
browse_cb(
    cups_dnssd_browse_t *browse,	// I - Browse request
    void                *cb_data,	// I - Callback data
    cups_dnssd_flags_t  flags,		// I - Bit flags
    uint32_t            if_index,	// I - Interface index
    const char          *name,		// I - Service name
    const char          *regtype,	// I - Registration type
    const char          *domain)	// I - Domain
{
  testdata_t	*data = (testdata_t *)cb_data;
					// Test data


  fprintf(stderr, "Browse flags=%02X if_index=%u name=\"%s\" regtype=\"%s\" domain=\"%s\"\n", flags, if_index, name, regtype, domain);

  cupsMutexLock(&data->mutex);
  data->browse_count ++;
  cupsMutexUnlock(&data->mutex);
}


//
// 'error_cb()' - Display an error.
//

static void
error_cb(void       *cb_data,		// I - Callback data
         const char *message)		// I - Error message
{
  testdata_t	*data = (testdata_t *)cb_data;
					// Test data


  testEndMessage(false, "%s", message);

  cupsMutexLock(&data->mutex);
  data->error_count ++;
  cupsMutexUnlock(&data->mutex);
}


#if 0
//
// 'query_cb()' - Query callback.
//

static void
query_cb(
    cups_dnssd_query_t *query,
    void               *cb_data,
    cups_dnssd_flags_t flags,
    uint32_t           if_index,
    const char         *fullname,
    uint16_t           rrtype,
    const void         *qdata,
    uint16_t           qlen)
{
  testdata_t	*data = (testdata_t *)cb_data;
					// Test data


  fprintf(stderr, "Query: flags=%02X if_index=%u fullname=\"%s\" rrtype=%u qdata=%p qlen=%u\n", flags, if_index, fullname, rrtype, qdata, qlen);

  cupsMutexLock(&data->mutex);
  data->query_count ++;
  cupsMutexUnlock(&data->mutex);
}


static void
resolve_cb(
    cups_dnssd_resolve_t *res,
    void                 *cb_data,
    cups_dnssd_flags_t   flags,
    uint32_t             if_index,
    const char           *fullname,
    const char           *host,
    uint16_t             port,
    size_t               num_txt,
    cups_option_t        *txt)
{
  testdata_t	*data = (testdata_t *)cb_data;
					// Test data


  fprintf(stderr, "Resolve: flags=%02X if_index=%u fullname=\"%s\" host=\"%s\" port=%u num_txt=%u, txt=%p\n", flags, if_index, fullname, host, port, (unsigned)num_txt, txt);

  cupsMutexLock(&data->mutex);
  data->resolve_count ++;
  cupsMutexUnlock(&data->mutex);
}
#endif // 0


static void
service_cb(
    cups_dnssd_service_t *service,
    void                 *cb_data,
    cups_dnssd_flags_t   flags,
    const char           *name,
    const char           *regtype,
    const char           *domain)
{
  testdata_t	*data = (testdata_t *)cb_data;
					// Test data


  fprintf(stderr, "Service: flags=%02X name=\"%s\" regtype=\"%s\" domain=\"%s\"\n", flags, name, regtype, domain);

  cupsMutexLock(&data->mutex);
  data->service_count ++;
  cupsMutexUnlock(&data->mutex);
}


//
// 'usage()' - Show program usage.
//

static void
usage(const char *arg)			// I - Argument for usage message
{
  if (arg)
    printf("testdnssd: Unknown option \"%s\".\n", arg);

  puts("Usage:");

  exit(arg != NULL);
}
