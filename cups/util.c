/*
 * Printing utilities for CUPS.
 *
 * Copyright © 2021-2022 by OpenPrinting.
 * Copyright © 2007-2018 by Apple Inc.
 * Copyright © 1997-2006 by Easy Software Products.
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
#include <sys/stat.h>
#if defined(_WIN32) || defined(__EMX__)
#  include <io.h>
#else
#  include <unistd.h>
#endif /* _WIN32 || __EMX__ */


/*
 * 'cupsFreeJobs()' - Free memory used by job data.
 */

void
cupsFreeJobs(int        num_jobs,	/* I - Number of jobs */
             cups_job_t *jobs)		/* I - Jobs */
{
  int		i;			/* Looping var */
  cups_job_t	*job;			/* Current job */


  if (num_jobs <= 0 || !jobs)
    return;

  for (i = num_jobs, job = jobs; i > 0; i --, job ++)
  {
    _cupsStrFree(job->dest);
    _cupsStrFree(job->user);
    _cupsStrFree(job->format);
    _cupsStrFree(job->title);
  }

  free(jobs);
}


/*
 * 'cupsGetDefault()' - Get the default printer or class for the specified server.
 *
 * This function returns the default printer or class as defined by
 * the LPDEST or PRINTER environment variables. If these environment
 * variables are not set, the server default destination is returned.
 * Applications should use the @link cupsGetDests@ and @link cupsGetDest@
 * functions to get the user-defined default printer, as this function does
 * not support the lpoptions-defined default printer.
 *
 *
 */

const char *				/* O - Default printer or `NULL` for none */
cupsGetDefault(http_t *http)		/* I - Connection to server or `CUPS_HTTP_DEFAULT` */
{
  ipp_t		*request,		/* IPP Request */
		*response;		/* IPP Response */
  ipp_attribute_t *attr;		/* Current attribute */
  _cups_globals_t *cg = _cupsGlobals();	/* Pointer to library globals */


 /*
  * See if we have a user default printer set...
  */

  if (_cupsGetUserDefault(cg->def_printer, sizeof(cg->def_printer)))
    return (cg->def_printer);

 /*
  * Connect to the server as needed...
  */

  if (!http)
    if ((http = _cupsConnect()) == NULL)
      return (NULL);

 /*
  * Build a CUPS_GET_DEFAULT request, which requires the following
  * attributes:
  *
  *    attributes-charset
  *    attributes-natural-language
  */

  request = ippNewRequest(IPP_OP_CUPS_GET_DEFAULT);

 /*
  * Do the request and get back a response...
  */

  if ((response = cupsDoRequest(http, request, "/")) != NULL)
  {
    if ((attr = ippFindAttribute(response, "printer-name",
                                 IPP_TAG_NAME)) != NULL)
    {
      cupsCopyString(cg->def_printer, attr->values[0].string.text,
              sizeof(cg->def_printer));
      ippDelete(response);
      return (cg->def_printer);
    }

    ippDelete(response);
  }

  return (NULL);
}


/*
 * 'cupsGetJobs()' - Get the jobs from the specified server.
 *
 * A "whichjobs" value of @code CUPS_WHICHJOBS_ALL@ returns all jobs regardless
 * of state, while @code CUPS_WHICHJOBS_ACTIVE@ returns jobs that are
 * pending, processing, or held and @code CUPS_WHICHJOBS_COMPLETED@ returns
 * jobs that are stopped, canceled, aborted, or completed.
 *
 *
 */

int					/* O - Number of jobs */
cupsGetJobs(http_t           *http,	/* I - Connection to server or @code CUPS_HTTP_DEFAULT@ */
            cups_job_t       **jobs,	/* O - Job data */
            const char       *name,	/* I - @code NULL@ = all destinations, otherwise show jobs for named destination */
            bool             myjobs,	/* I - `false` = all users, `true` = mine */
	    cups_whichjobs_t whichjobs)	/* I - `CUPS_WHICHJOBS_ALL`, `CUPS_WHICHJOBS_ACTIVE`, or `CUPS_WHICHJOBS_COMPLETED` */
{
  int		n;			/* Number of jobs */
  ipp_t		*request,		/* IPP Request */
		*response;		/* IPP Response */
  ipp_attribute_t *attr;		/* Current attribute */
  cups_job_t	*temp;			/* Temporary pointer */
  int		id,			/* job-id */
		priority,		/* job-priority */
		size;			/* job-k-octets */
  ipp_jstate_t	state;			/* job-state */
  time_t	completed_time,		/* time-at-completed */
		creation_time,		/* time-at-creation */
		processing_time;	/* time-at-processing */
  const char	*dest,			/* job-printer-uri */
		*format,		/* document-format */
		*title,			/* job-name */
		*user;			/* job-originating-user-name */
  char		uri[HTTP_MAX_URI];	/* URI for jobs */
  _cups_globals_t *cg = _cupsGlobals();	/* Pointer to library globals */
  static const char * const attrs[] =	/* Requested attributes */
		{
		  "document-format",
		  "job-id",
		  "job-k-octets",
		  "job-name",
		  "job-originating-user-name",
		  "job-printer-uri",
		  "job-priority",
		  "job-state",
		  "time-at-completed",
		  "time-at-creation",
		  "time-at-processing"
		};


 /*
  * Range check input...
  */

  if (!jobs)
  {
    _cupsSetError(IPP_STATUS_ERROR_INTERNAL, strerror(EINVAL), 0);

    return (-1);
  }

 /*
  * Get the right URI...
  */

  if (name)
  {
    if (httpAssembleURIf(HTTP_URI_CODING_ALL, uri, sizeof(uri), "ipp", NULL,
                         "localhost", 0, "/printers/%s",
                         name) < HTTP_URI_STATUS_OK)
    {
      _cupsSetError(IPP_STATUS_ERROR_INTERNAL,
                    _("Unable to create printer-uri"), 1);

      return (-1);
    }
  }
  else
    cupsCopyString(uri, "ipp://localhost/", sizeof(uri));

  if (!http)
    if ((http = _cupsConnect()) == NULL)
      return (-1);

 /*
  * Build an IPP_GET_JOBS request, which requires the following
  * attributes:
  *
  *    attributes-charset
  *    attributes-natural-language
  *    printer-uri
  *    requesting-user-name
  *    which-jobs
  *    my-jobs
  *    requested-attributes
  */

  request = ippNewRequest(IPP_OP_GET_JOBS);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI,
               "printer-uri", NULL, uri);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME,
               "requesting-user-name", NULL, cupsGetUser());

  if (myjobs)
    ippAddBoolean(request, IPP_TAG_OPERATION, "my-jobs", 1);

  if (whichjobs == CUPS_WHICHJOBS_COMPLETED)
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD,
                 "which-jobs", NULL, "completed");
  else if (whichjobs == CUPS_WHICHJOBS_ALL)
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD,
                 "which-jobs", NULL, "all");

  ippAddStrings(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD,
                "requested-attributes", sizeof(attrs) / sizeof(attrs[0]),
		NULL, attrs);

 /*
  * Do the request and get back a response...
  */

  n     = 0;
  *jobs = NULL;

  if ((response = cupsDoRequest(http, request, "/")) != NULL)
  {
    for (attr = response->attrs; attr; attr = attr->next)
    {
     /*
      * Skip leading attributes until we hit a job...
      */

      while (attr && attr->group_tag != IPP_TAG_JOB)
        attr = attr->next;

      if (!attr)
        break;

     /*
      * Pull the needed attributes from this job...
      */

      id              = 0;
      size            = 0;
      priority        = 50;
      state           = IPP_JSTATE_PENDING;
      user            = "unknown";
      dest            = NULL;
      format          = "application/octet-stream";
      title           = "untitled";
      creation_time   = 0;
      completed_time  = 0;
      processing_time = 0;

      while (attr && attr->group_tag == IPP_TAG_JOB)
      {
        if (!strcmp(attr->name, "job-id") &&
	    attr->value_tag == IPP_TAG_INTEGER)
	  id = attr->values[0].integer;
        else if (!strcmp(attr->name, "job-state") &&
	         attr->value_tag == IPP_TAG_ENUM)
	  state = (ipp_jstate_t)attr->values[0].integer;
        else if (!strcmp(attr->name, "job-priority") &&
	         attr->value_tag == IPP_TAG_INTEGER)
	  priority = attr->values[0].integer;
        else if (!strcmp(attr->name, "job-k-octets") &&
	         attr->value_tag == IPP_TAG_INTEGER)
	  size = attr->values[0].integer;
        else if (!strcmp(attr->name, "time-at-completed") &&
	         attr->value_tag == IPP_TAG_INTEGER)
	  completed_time = attr->values[0].integer;
        else if (!strcmp(attr->name, "time-at-creation") &&
	         attr->value_tag == IPP_TAG_INTEGER)
	  creation_time = attr->values[0].integer;
        else if (!strcmp(attr->name, "time-at-processing") &&
	         attr->value_tag == IPP_TAG_INTEGER)
	  processing_time = attr->values[0].integer;
        else if (!strcmp(attr->name, "job-printer-uri") &&
	         attr->value_tag == IPP_TAG_URI)
	{
	  if ((dest = strrchr(attr->values[0].string.text, '/')) != NULL)
	    dest ++;
        }
        else if (!strcmp(attr->name, "job-originating-user-name") &&
	         attr->value_tag == IPP_TAG_NAME)
	  user = attr->values[0].string.text;
        else if (!strcmp(attr->name, "document-format") &&
	         attr->value_tag == IPP_TAG_MIMETYPE)
	  format = attr->values[0].string.text;
        else if (!strcmp(attr->name, "job-name") &&
	         (attr->value_tag == IPP_TAG_TEXT ||
		  attr->value_tag == IPP_TAG_NAME))
	  title = attr->values[0].string.text;

        attr = attr->next;
      }

     /*
      * See if we have everything needed...
      */

      if (!dest || !id)
      {
        if (!attr)
	  break;
	else
          continue;
      }

     /*
      * Allocate memory for the job...
      */

      if (n == 0)
        temp = malloc(sizeof(cups_job_t));
      else
	temp = realloc(*jobs, sizeof(cups_job_t) * (size_t)(n + 1));

      if (!temp)
      {
       /*
        * Ran out of memory!
        */

        _cupsSetError(IPP_STATUS_ERROR_INTERNAL, NULL, 0);

	cupsFreeJobs(n, *jobs);
	*jobs = NULL;

        ippDelete(response);

	return (-1);
      }

      *jobs = temp;
      temp  += n;
      n ++;

     /*
      * Copy the data over...
      */

      temp->dest            = _cupsStrAlloc(dest);
      temp->user            = _cupsStrAlloc(user);
      temp->format          = _cupsStrAlloc(format);
      temp->title           = _cupsStrAlloc(title);
      temp->id              = id;
      temp->priority        = priority;
      temp->state           = state;
      temp->size            = size;
      temp->completed_time  = completed_time;
      temp->creation_time   = creation_time;
      temp->processing_time = processing_time;

      if (!attr)
        break;
    }

    ippDelete(response);
  }

  if (n == 0 && cg->last_error >= IPP_STATUS_ERROR_BAD_REQUEST)
    return (-1);
  else
    return (n);
}
