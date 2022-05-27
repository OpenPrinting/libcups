/*
 * Private IPP definitions for CUPS.
 *
 * Copyright © 2021 by OpenPrinting.
 * Copyright © 2007-2018 by Apple Inc.
 * Copyright © 1997-2006 by Easy Software Products.
 *
 * Licensed under Apache License v2.0.  See the file "LICENSE" for more
 * information.
 */

#ifndef _CUPS_IPP_PRIVATE_H_
#  define _CUPS_IPP_PRIVATE_H_
#  include "cups.h"
#  ifdef __cplusplus
extern "C" {
#  endif /* __cplusplus */


/*
 * Constants...
 */

#  define IPP_BUF_SIZE	(IPP_MAX_LENGTH + 2)
					/* Size of buffer */


/*
 * Structures...
 */

typedef union _ipp_request_u		/**** Request Header ****/
{
  struct				/* Any Header */
  {
    ipp_uchar_t	version[2];		/* Protocol version number */
    int		op_status;		/* Operation ID or status code*/
    int		request_id;		/* Request ID */
  }		any;

  struct				/* Operation Header */
  {
    ipp_uchar_t	version[2];		/* Protocol version number */
    ipp_op_t	operation_id;		/* Operation ID */
    int		request_id;		/* Request ID */
  }		op;

  struct				/* Status Header */
  {
    ipp_uchar_t	version[2];		/* Protocol version number */
    ipp_status_t status_code;		/* Status code */
    int		request_id;		/* Request ID */
  }		status;

  /**** New in CUPS 1.1.19 ****/
  struct				/* Event Header */
  {
    ipp_uchar_t	version[2];		/* Protocol version number */
    ipp_status_t status_code;		/* Status code */
    int		request_id;		/* Request ID */
  }		event;
} _ipp_request_t;

typedef union _ipp_value_u		/**** Attribute Value ****/
{
  int		integer;		/* Integer/enumerated value */

  bool		boolean;		/* Boolean value */

  ipp_t		*collection;		/* Collection value */

  ipp_uchar_t	date[11];		/* Date/time value */

  struct
  {
    int		xres,			/* Horizontal resolution */
		yres;			/* Vertical resolution */
    ipp_res_t	units;			/* Resolution units */
  }		resolution;		/* Resolution value */

  struct
  {
    int		lower,			/* Lower value */
		upper;			/* Upper value */
  }		range;			/* Range of integers value */

  struct
  {
    char	*language;		/* Language code */
    char	*text;			/* String */
  }		string;			/* String with language value */

  struct
  {
    size_t	length;			/* Length of attribute */
    void	*data;			/* Data in attribute */
  }		unknown;		/* Unknown attribute type */
} _ipp_value_t;

struct _ipp_attribute_s			/**** IPP attribute ****/
{
  ipp_attribute_t *next;		/* Next attribute in list */
  ipp_tag_t	group_tag,		/* Job/Printer/Operation group tag */
		value_tag;		/* What type of value is it? */
  char		*name;			/* Name of attribute */
  size_t	num_values;		/* Number of values */
  _ipp_value_t	values[1];		/* Values */
};

struct _ipp_s				/**** IPP Request/Response/Notification ****/
{
  ipp_state_t		state;		/* State of request */
  _ipp_request_t	request;	/* Request header */
  ipp_attribute_t	*attrs;		/* Attributes */
  ipp_attribute_t	*last;		/* Last attribute in list */
  ipp_attribute_t	*current;	/* Current attribute (for read/write) */
  ipp_tag_t		curtag;		/* Current attribute group tag */

  ipp_attribute_t	*prev;		/* Previous attribute (for read) */
  int			use;		/* Use count @since CUPS 1.4.4.?@ */
  bool			atend;		/* At end of list? */
  size_t		curindex;	/* Current attribute index for hierarchical search */
};

typedef struct _ipp_option_s		/**** Attribute mapping data ****/
{
  int		multivalue;		/* Option has multiple values? */
  const char	*name;			/* Option/attribute name */
  ipp_tag_t	value_tag;		/* Value tag for this attribute */
  ipp_tag_t	group_tag;		/* Group tag for this attribute */
  ipp_tag_t	alt_group_tag;		/* Alternate group tag for this attribute */
  const ipp_op_t *operations;		/* Allowed operations for this attr */
} _ipp_option_t;


/*
 * Prototypes for private functions...
 */

/* encode.c */
#ifdef DEBUG
extern const char	*_ippCheckOptions(void) _CUPS_PRIVATE;
#endif /* DEBUG */
extern _ipp_option_t	*_ippFindOption(const char *name) _CUPS_PRIVATE;

/* ipp-file.c */
extern ipp_t		*_ippFileParse(_ipp_vars_t *v, const char *filename, void *user_data) _CUPS_PRIVATE;
extern int		_ippFileReadToken(_ipp_file_t *f, char *token, size_t tokensize) _CUPS_PRIVATE;

/* ipp-vars.c */
extern void		_ippVarsDeinit(_ipp_vars_t *v) _CUPS_PRIVATE;
extern void		_ippVarsExpand(_ipp_vars_t *v, char *dst, const char *src, size_t dstsize) _CUPS_NONNULL(1,2,3) _CUPS_PRIVATE;
extern const char	*_ippVarsGet(_ipp_vars_t *v, const char *name) _CUPS_PRIVATE;
extern void		_ippVarsInit(_ipp_vars_t *v, _ipp_fattr_cb_t attrcb, _ipp_ferror_cb_t errorcb, _ipp_ftoken_cb_t tokencb) _CUPS_PRIVATE;
extern const char	*_ippVarsPasswordCB(const char *prompt, http_t *http, const char *method, const char *resource, void *user_data) _CUPS_PRIVATE;
extern int		_ippVarsSet(_ipp_vars_t *v, const char *name, const char *value) _CUPS_PRIVATE;


#  ifdef __cplusplus
}
#  endif /* __cplusplus */
#endif /* !_CUPS_IPP_H_ */
