//
// Destination job support for CUPS.
//
// Copyright © 2021-2023 by OpenPrinting.
// Copyright © 2012-2017 by Apple Inc.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#include "cups-private.h"


//
// 'cupsCancelDestJob()' - Cancel a job on a destination.
//
// This function cancels the specified job.  The "dest" argument is the name
// of the destination printer while the "job_id" is the number returned by
// @link cupsCreateDestJob@.
//
// Returns `IPP_STATUS_OK` on success and
// `IPP_STATUS_ERROR_NOT_AUTHORIZED` or
// `IPP_STATUS_ERROR_FORBIDDEN` on failure.
//

ipp_status_t                            // O - Status of cancel operation
cupsCancelDestJob(http_t      *http,	// I - Connection to destination
                  cups_dest_t *dest,	// I - Destination
                  int         job_id)	// I - Job ID
{
  cups_dinfo_t	*info;			// Destination information


  if ((info = cupsCopyDestInfo(http, dest)) != NULL)
  {
    ipp_t	*request;		// Cancel-Job request

    request = ippNewRequest(IPP_OP_CANCEL_JOB);

    ippSetVersion(request, info->version / 10, info->version % 10);

    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri", NULL, info->uri);
    ippAddInteger(request, IPP_TAG_OPERATION, IPP_TAG_INTEGER, "job-id", job_id);
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name", NULL, cupsGetUser());

    ippDelete(cupsDoRequest(http, request, info->resource));
    cupsFreeDestInfo(info);
  }

  return (cupsGetError());
}


//
// 'cupsCloseDestJob()' - Close a job and start printing.
//
// This function closes a job and starts printing.  Use when the last call to
// @link cupsStartDestDocument@ passed `false` for "last_document".  "job_id"
// is the job ID returned by @link cupsCreateDestJob@.  Returns `IPP_STATUS_OK`
// on success.
//

ipp_status_t				// O - IPP status code
cupsCloseDestJob(
    http_t       *http,			// I - Connection to destination
    cups_dest_t  *dest,			// I - Destination
    cups_dinfo_t *info, 		// I - Destination information
    int          job_id)		// I - Job ID
{
  ipp_t	*request = NULL;		// Close-Job/Send-Document request


  DEBUG_printf(("cupsCloseDestJob(http=%p, dest=%p(%s/%s), info=%p, job_id=%d)", (void *)http, (void *)dest, dest ? dest->name : NULL, dest ? dest->instance : NULL, (void *)info, job_id));

  // Get the default connection as needed...
  if (!http)
    http = _cupsConnect();

  // Range check input...
  if (!http || !dest || !info || job_id <= 0)
  {
    _cupsSetError(IPP_STATUS_ERROR_INTERNAL, strerror(EINVAL), false);
    DEBUG_puts("1cupsCloseDestJob: Bad arguments.");
    return (IPP_STATUS_ERROR_INTERNAL);
  }

  // Build a Close-Job or empty Send-Document request...
  if (ippContainsInteger(ippFindAttribute(info->attrs, "operations-supported", IPP_TAG_ENUM), IPP_OP_CLOSE_JOB))
    request = ippNewRequest(IPP_OP_CLOSE_JOB);
  else
    request = ippNewRequest(IPP_OP_SEND_DOCUMENT);

  if (!request)
  {
    _cupsSetError(IPP_STATUS_ERROR_INTERNAL, strerror(ENOMEM), false);
    DEBUG_puts("1cupsCloseDestJob: Unable to create Close-Job/Send-Document request.");
    return (IPP_STATUS_ERROR_INTERNAL);
  }

  ippSetVersion(request, info->version / 10, info->version % 10);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri", NULL, info->uri);
  ippAddInteger(request, IPP_TAG_OPERATION, IPP_TAG_INTEGER, "job-id", job_id);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name", NULL, cupsGetUser());
  if (ippGetOperation(request) == IPP_OP_SEND_DOCUMENT)
    ippAddBoolean(request, IPP_TAG_OPERATION, "last-document", true);

  // Send the request and return the status...
  ippDelete(cupsDoRequest(http, request, info->resource));

  DEBUG_printf(("1cupsCloseDestJob: %s (%s)", ippErrorString(cupsGetError()), cupsGetErrorString()));

  return (cupsGetError());
}


//
// 'cupsCreateDestJob()' - Create a job on a destination.
//
// This function creates a job on the specified destination "dest".  The "info"
// argument contains information about the destination obtained using the
// @link cupsCopyDestInfo@ function.
//
// The "job_id" argument provides an integer to hold the new job's ID number.
// The "title" argument provides the title of the job.  The "num_options" and
// "options" arguments provide an array of job options for the job.
//
// Returns `IPP_STATUS_OK` or `IPP_STATUS_OK_SUBST` on success, saving the job ID
// in the variable pointed to by "job_id".
//

ipp_status_t				// O - IPP status code
cupsCreateDestJob(
    http_t        *http,		// I - Connection to destination
    cups_dest_t   *dest,		// I - Destination
    cups_dinfo_t  *info, 		// I - Destination information
    int           *job_id,		// O - Job ID or `0` on error
    const char    *title,		// I - Job name
    size_t        num_options,		// I - Number of job options
    cups_option_t *options)		// I - Job options
{
  ipp_t			*request,	// Create-Job request
			*response;	// Create-Job response
  ipp_attribute_t	*attr;		// job-id attribute


  DEBUG_printf(("cupsCreateDestJob(http=%p, dest=%p(%s/%s), info=%p, "
                "job_id=%p, title=\"%s\", num_options=%u, options=%p)", (void *)http, (void *)dest, dest ? dest->name : NULL, dest ? dest->instance : NULL, (void *)info, (void *)job_id, title, (unsigned)num_options, (void *)options));

  // Get the default connection as needed...
  if (!http)
    http = _cupsConnect();

  // Range check input...
  if (job_id)
    *job_id = 0;

  if (!http || !dest || !info || !job_id)
  {
    _cupsSetError(IPP_STATUS_ERROR_INTERNAL, strerror(EINVAL), false);
    DEBUG_puts("1cupsCreateDestJob: Bad arguments.");
    return (IPP_STATUS_ERROR_INTERNAL);
  }

  // Build a Create-Job request...
  if ((request = ippNewRequest(IPP_OP_CREATE_JOB)) == NULL)
  {
    _cupsSetError(IPP_STATUS_ERROR_INTERNAL, strerror(ENOMEM), false);
    DEBUG_puts("1cupsCreateDestJob: Unable to create Create-Job request.");
    return (IPP_STATUS_ERROR_INTERNAL);
  }

  ippSetVersion(request, info->version / 10, info->version % 10);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri", NULL, info->uri);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name", NULL, cupsGetUser());
  if (title)
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "job-name", NULL, title);

  cupsEncodeOptions(request, num_options, options, IPP_TAG_OPERATION);
  cupsEncodeOptions(request, num_options, options, IPP_TAG_JOB);
  cupsEncodeOptions(request, num_options, options, IPP_TAG_SUBSCRIPTION);

  // Send the request and get the job-id...
  response = cupsDoRequest(http, request, info->resource);

  if ((attr = ippFindAttribute(response, "job-id", IPP_TAG_INTEGER)) != NULL)
  {
    *job_id = attr->values[0].integer;
    DEBUG_printf(("1cupsCreateDestJob: job-id=%d", *job_id));
  }

  ippDelete(response);

  // Return the status code from the Create-Job request...
  DEBUG_printf(("1cupsCreateDestJob: %s (%s)", ippErrorString(cupsGetError()), cupsGetErrorString()));

  return (cupsGetError());
}


//
// 'cupsFinishDestDocument()' - Finish the current document.
//
// This function finished the current document for a job on the specified
// destination "dest".  The "info" argument contains information about the
// destination obtained using the @link cupsCopyDestInfo@ function.
//
// Returns `IPP_STATUS_OK` or `IPP_STATUS_OK_SUBST` on success.
//

ipp_status_t				// O - Status of document submission
cupsFinishDestDocument(
    http_t       *http,			// I - Connection to destination
    cups_dest_t  *dest,			// I - Destination
    cups_dinfo_t *info) 		// I - Destination information
{
  DEBUG_printf(("cupsFinishDestDocument(http=%p, dest=%p(%s/%s), info=%p)", (void *)http, (void *)dest, dest ? dest->name : NULL, dest ? dest->instance : NULL, (void *)info));

  // Get the default connection as needed...
  if (!http)
    http = _cupsConnect();

  // Range check input...
  if (!http || !dest || !info)
  {
    _cupsSetError(IPP_STATUS_ERROR_INTERNAL, strerror(EINVAL), false);
    DEBUG_puts("1cupsFinishDestDocument: Bad arguments.");
    return (IPP_STATUS_ERROR_INTERNAL);
  }

  // Get the response at the end of the document and return it...
  ippDelete(cupsGetResponse(http, info->resource));

  DEBUG_printf(("1cupsFinishDestDocument: %s (%s)", ippErrorString(cupsGetError()), cupsGetErrorString()));

  return (cupsGetError());
}


//
// 'cupsStartDestDocument()' - Start a new document.
//
// This function starts a new document for the specified destination "dest" and
// job "job_id".  The "info" argument contains information about the destination
// obtained using the @link cupsCopyDestInfo@ function.
//
// The "docname" argument specifies the name of the document/file being printed.
// The "format" argument specifies the MIME media type for the document; the
// following constants are provided for convenience:
//
// - `CUPS_FORMAT_AUTO`: Auto-type the file using its contents
// - `CUPS_FORMAT_JPEG`: JPEG image file
// - `CUPS_FORMAT_PDF`: PDF file
// - `CUPS_FORMAT_TEXT`: Plain text file
//
// The "num_options" and "options" arguments provide the options to be applied
// to the document.  The "last_document" argument specifies whether this is the
// final document in the job (`true`) or not (`false`).
//
// Returns `HTTP_CONTINUE` on success.
//

http_status_t				// O - Status of document creation
cupsStartDestDocument(
    http_t        *http,		// I - Connection to destination
    cups_dest_t   *dest,		// I - Destination
    cups_dinfo_t  *info, 		// I - Destination information
    int           job_id,		// I - Job ID
    const char    *docname,		// I - Document name
    const char    *format,		// I - Document format
    size_t        num_options,		// I - Number of document options
    cups_option_t *options,		// I - Document options
    bool          last_document)	// I - `true` if this is the last document
{
  ipp_t		*request;		// Send-Document request
  http_status_t	status;			// HTTP status


  DEBUG_printf(("cupsStartDestDocument(http=%p, dest=%p(%s/%s), info=%p, job_id=%d, docname=\"%s\", format=\"%s\", num_options=%u, options=%p, last_document=%d)", (void *)http, (void *)dest, dest ? dest->name : NULL, dest ? dest->instance : NULL, (void *)info, job_id, docname, format, (unsigned)num_options, (void *)options, last_document));

  // Get the default connection as needed...
  if (!http)
    http = _cupsConnect();

  // Range check input...
  if (!http || !dest || !info || job_id <= 0)
  {
    _cupsSetError(IPP_STATUS_ERROR_INTERNAL, strerror(EINVAL), false);
    DEBUG_puts("1cupsStartDestDocument: Bad arguments.");
    return (HTTP_STATUS_ERROR);
  }

  // Create a Send-Document request...
  if ((request = ippNewRequest(IPP_OP_SEND_DOCUMENT)) == NULL)
  {
    _cupsSetError(IPP_STATUS_ERROR_INTERNAL, strerror(ENOMEM), false);
    DEBUG_puts("1cupsStartDestDocument: Unable to create Send-Document request.");
    return (HTTP_STATUS_ERROR);
  }

  ippSetVersion(request, info->version / 10, info->version % 10);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri", NULL, info->uri);
  ippAddInteger(request, IPP_TAG_OPERATION, IPP_TAG_INTEGER, "job-id", job_id);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name", NULL, cupsGetUser());
  if (docname)
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "document-name", NULL, docname);
  if (format)
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_MIMETYPE, "document-format", NULL, format);
  ippAddBoolean(request, IPP_TAG_OPERATION, "last-document", last_document);

  cupsEncodeOptions(request, num_options, options, IPP_TAG_OPERATION);
  cupsEncodeOptions(request, num_options, options, IPP_TAG_DOCUMENT);

  // Send and delete the request, then return the status...
  status = cupsSendRequest(http, request, info->resource, CUPS_LENGTH_VARIABLE);

  ippDelete(request);

  return (status);
}
