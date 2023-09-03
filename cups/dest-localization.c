//
// Destination localization support for CUPS.
//
// Copyright © 2021-2022 by OpenPrinting.
// Copyright © 2012-2017 by Apple Inc.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#include "cups-private.h"


//
// Local functions...
//

static void	cups_load_localizations(http_t *http, cups_dinfo_t *dinfo);


//
// 'cupsLocalizeDestMedia()' - Get the localized string for a destination media
//                             size.
//
// The returned string is stored in the destination information and will become
// invalid if the destination information is deleted.
//

const char *				// O - Localized string
cupsLocalizeDestMedia(
    http_t       *http,			// I - Connection to destination
    cups_dest_t  *dest,			// I - Destination
    cups_dinfo_t *dinfo,		// I - Destination information
    unsigned     flags,			// I - Media flags
    cups_media_t *media)		// I - Media info
{
  cups_lang_t		*lang;		// Standard localizations
  pwg_media_t		*pwg;		// PWG media information
  cups_array_t		*db;		// Media database
  _cups_media_db_t	*mdb;		// Media database entry
  char			lstr[1024],	// Localized size name
			temp[256];	// Temporary string
  const char		*lsize,		// Localized media size
			*lsource,	// Localized media source
			*ltype;		// Localized media type


  DEBUG_printf("cupsLocalizeDestMedia(http=%p, dest=%p, dinfo=%p, flags=%x, media=%p(\"%s\"))", (void *)http, (void *)dest, (void *)dinfo, flags, (void *)media, media ? media->media : "(null)");

  // Range check input...
  if (!http || !dest || !dinfo || !media)
  {
    DEBUG_puts("1cupsLocalizeDestMedia: Returning NULL.");
    _cupsSetError(IPP_STATUS_ERROR_INTERNAL, strerror(EINVAL), 0);
    return (NULL);
  }

  // Find the matching media database entry...
  if (flags & CUPS_MEDIA_FLAGS_READY)
    db = dinfo->ready_db;
  else
    db = dinfo->media_db;

  for (mdb = (_cups_media_db_t *)cupsArrayGetFirst(db); mdb; mdb = (_cups_media_db_t *)cupsArrayGetNext(db))
  {
    if (mdb->key && !strcmp(mdb->key, media->media))
      break;
    else if (mdb->size_name && !strcmp(mdb->size_name, media->media))
      break;
  }

  if (!mdb)
  {
    for (mdb = (_cups_media_db_t *)cupsArrayGetFirst(db); mdb; mdb = (_cups_media_db_t *)cupsArrayGetNext(db))
    {
      if (mdb->width == media->width && mdb->length == media->length && mdb->bottom == media->bottom && mdb->left == media->left && mdb->right == media->right && mdb->top == media->top)
	break;
    }
  }

  // See if the localization is cached...
  lang = cupsLangDefault();

  if (!dinfo->localizations)
    cups_load_localizations(http, dinfo);

  snprintf(temp, sizeof(temp), "media.%s", media->media);
  if ((lsize = cupsLangGetString(lang, temp)) == temp)
  {
    // Not a media name, try a media-key name...
    snprintf(temp, sizeof(temp), "media-key.%s", media->media);
    if ((lsize = cupsLangGetString(lang, temp)) == temp)
      lsize = NULL;
  }

  if (!lsize && (pwg = pwgMediaForSize(media->width, media->length)) != NULL && pwg->ppd)
  {
    // Get a standard localization...
    snprintf(temp, sizeof(temp), "media.%s", pwg->pwg);
    if ((lsize = cupsLangGetString(lang, temp)) == temp)
      lsize = NULL;
  }

  if (!lsize)
  {
    // Make a dimensional localization...
    if ((media->width % 635) == 0 && (media->length % 635) == 0)
    {
      // Use inches since the size is a multiple of 1/4 inch.
      cupsLangFormatString(lang, temp, sizeof(temp), _("%g x %g \""), media->width / 2540.0, media->length / 2540.0);
    }
    else
    {
      // Use millimeters since the size is not a multiple of 1/4 inch.
      cupsLangFormatString(lang, temp, sizeof(temp), _("%d x %d mm"), (media->width + 50) / 100, (media->length + 50) / 100);
    }

    lsize = temp;
  }

  if (mdb)
  {
    DEBUG_printf("1cupsLocalizeDestMedia: MATCH mdb%p [key=\"%s\" size_name=\"%s\" source=\"%s\" type=\"%s\" width=%d length=%d B%d L%d R%d T%d]", (void *)mdb, mdb->key, mdb->size_name, mdb->source, mdb->type, mdb->width, mdb->length, mdb->bottom, mdb->left, mdb->right, mdb->top);

    if ((lsource = cupsLocalizeDestValue(http, dest, dinfo, "media-source", mdb->source)) == mdb->source && mdb->source)
      lsource = cupsLangGetString(lang, _("Other Tray"));
    if ((ltype = cupsLocalizeDestValue(http, dest, dinfo, "media-type", mdb->type)) == mdb->type && mdb->type)
      ltype = cupsLangGetString(lang, _("Other Media"));
  }
  else
  {
    lsource = NULL;
    ltype   = NULL;
  }

  if (!lsource && !ltype)
  {
    if (!media->bottom && !media->left && !media->right && !media->top)
      cupsLangFormatString(lang, lstr, sizeof(lstr), _("%s (Borderless)"), lsize);
    else
      cupsCopyString(lstr, lsize, sizeof(lstr));
  }
  else if (!lsource)
  {
    if (!media->bottom && !media->left && !media->right && !media->top)
      cupsLangFormatString(lang, lstr, sizeof(lstr), _("%s (Borderless, %s)"), lsize, ltype);
    else
      cupsLangFormatString(lang, lstr, sizeof(lstr), _("%s (%s)"), lsize, ltype);
  }
  else if (!ltype)
  {
    if (!media->bottom && !media->left && !media->right && !media->top)
      cupsLangFormatString(lang, lstr, sizeof(lstr), _("%s (Borderless, %s)"), lsize, lsource);
    else
      cupsLangFormatString(lang, lstr, sizeof(lstr), _("%s (%s)"), lsize, lsource);
  }
  else
  {
    if (!media->bottom && !media->left && !media->right && !media->top)
      cupsLangFormatString(lang, lstr, sizeof(lstr), _("%s (Borderless, %s, %s)"), lsize, ltype, lsource);
    else
      cupsLangFormatString(lang, lstr, sizeof(lstr), _("%s (%s, %s)"), lsize, ltype, lsource);
  }

  return (_cupsStrAlloc(lstr));
}


//
// 'cupsLocalizeDestOption()' - Get the localized string for a destination
//                              option.
//
// The returned string is stored in the destination information and will become
// invalid if the destination information is deleted.
//

const char *				// O - Localized string
cupsLocalizeDestOption(
    http_t       *http,			// I - Connection to destination
    cups_dest_t  *dest,			// I - Destination
    cups_dinfo_t *dinfo,		// I - Destination information
    const char   *option)		// I - Option to localize
{
  const char *localized;		// Localized string


  DEBUG_printf("cupsLocalizeDestOption(http=%p, dest=%p, dinfo=%p, option=\"%s\")", (void *)http, (void *)dest, (void *)dinfo, option);

  if (!http || !dest || !dinfo)
    return (_cupsStrAlloc(option));

  if (!dinfo->localizations)
    cups_load_localizations(http, dinfo);

  if ((localized = cupsLangGetString(cupsLangDefault(), option)) == option)
    return (_cupsStrAlloc(option));
  else
    return (localized);
}


//
// 'cupsLocalizeDestValue()' - Get the localized string for a destination
//                             option+value pair.
//
// The returned string is stored in the destination information and will become
// invalid if the destination information is deleted.
//

const char *				// O - Localized string
cupsLocalizeDestValue(
    http_t       *http,			// I - Connection to destination
    cups_dest_t  *dest,			// I - Destination
    cups_dinfo_t *dinfo,		// I - Destination information
    const char   *option,		// I - Option to localize
    const char   *value)		// I - Value to localize
{
  char		pair[256];		// option.value pair
  const char	*localized;		// Localized string


  DEBUG_printf("cupsLocalizeDestValue(http=%p, dest=%p, dinfo=%p, option=\"%s\", value=\"%s\")", (void *)http, (void *)dest, (void *)dinfo, option, value);

  if (!http || !dest || !dinfo)
    return (_cupsStrAlloc(value));

  if (!strcmp(option, "media"))
  {
    pwg_media_t *pwg = pwgMediaForPWG(value);
					// Media dimensions
    cups_media_t media;			// Media/size info

    memset(&media, 0, sizeof(media));
    cupsCopyString(media.media, value, sizeof(media.media));
    media.width  = pwg ? pwg->width : 0;
    media.length = pwg ? pwg->length : 0;
    media.left   = 0;
    media.right  = 0;
    media.bottom = 0;
    media.top    = 0;

    return (cupsLocalizeDestMedia(http, dest, dinfo, CUPS_MEDIA_FLAGS_DEFAULT, &media));
  }

  if (!dinfo->localizations)
    cups_load_localizations(http, dinfo);

  snprintf(pair, sizeof(pair), "%s.%s", option, value);
  if ((localized = cupsLangGetString(cupsLangDefault(), pair)) == pair)
    return (_cupsStrAlloc(value));
  else
    return (localized);
}


//
// 'cups_load_localizations()' - Load the localization strings for a
//                               destination.
//

static void
cups_load_localizations(
    http_t       *http,			// I - Connection to destination
    cups_dinfo_t *dinfo)		// I - Destination informations
{
  http_t		*http2;		// Connection for strings file
  http_status_t		status;		// Request status
  ipp_attribute_t	*attr;		// "printer-strings-uri" attribute
  char			scheme[32],	// URI scheme
  			userpass[256],	// Username/password info
  			hostname[256],	// Hostname
  			resource[1024],	// Resource
  			http_hostname[256],
  					// Hostname of connection
			tempfile[1024];	// Temporary filename
  int			port;		// Port number
  http_encryption_t	encryption;	// Encryption to use
  cups_file_t		*temp;		// Temporary file


  // See if there are any localizations...
  if ((attr = ippFindAttribute(dinfo->attrs, "printer-strings-uri", IPP_TAG_URI)) == NULL)
  {
    // Nope, create an empty message catalog...
    dinfo->localizations = true;
    DEBUG_puts("4cups_load_localizations: No printer-strings-uri (uri) value.");
    return;
  }

  // Pull apart the URI and determine whether we need to try a different
  // server...
  if (httpSeparateURI(HTTP_URI_CODING_ALL, attr->values[0].string.text, scheme, sizeof(scheme), userpass, sizeof(userpass), hostname, sizeof(hostname), &port, resource, sizeof(resource)) < HTTP_URI_STATUS_OK)
  {
    dinfo->localizations = true;
    DEBUG_printf("4cups_load_localizations: Bad printer-strings-uri value \"%s\".", attr->values[0].string.text);
    return;
  }

  httpGetHostname(http, http_hostname, sizeof(http_hostname));

  if (!_cups_strcasecmp(http_hostname, hostname) &&
      port == httpAddrGetPort(http->hostaddr))
  {
    // Use the same connection...
    http2 = http;
  }
  else
  {
    // Connect to the alternate host...
    if (!strcmp(scheme, "https"))
      encryption = HTTP_ENCRYPTION_ALWAYS;
    else
      encryption = HTTP_ENCRYPTION_IF_REQUESTED;

    if ((http2 = httpConnect(hostname, port, NULL, AF_UNSPEC, encryption, 1, 30000, NULL)) == NULL)
    {
      DEBUG_printf("4cups_load_localizations: Unable to connect to %s:%d: %s", hostname, port, cupsGetErrorString());
      return;
    }
  }

  // Get a temporary file...
  if ((temp = cupsCreateTempFile(NULL, ".strings", tempfile, sizeof(tempfile))) == NULL)
  {
    DEBUG_printf("4cups_load_localizations: Unable to create temporary file: %s", cupsGetErrorString());
    if (http2 != http)
      httpClose(http2);
    return;
  }

  status = cupsGetFd(http2, resource, cupsFileNumber(temp));
  cupsFileClose(temp);

  DEBUG_printf("4cups_load_localizations: GET %s = %s", resource, httpStatusString(status));

  if (status == HTTP_STATUS_OK)
  {
    // Got the file, read it...
    dinfo->localizations = cupsLangLoadStrings(cupsLangDefault(), tempfile, NULL);
  }

  // Cleanup...
  unlink(tempfile);

  if (http2 != http)
    httpClose(http2);
}
