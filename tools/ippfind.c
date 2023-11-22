//
// Utility to find IPP printers via Bonjour/DNS-SD and optionally run
// commands such as IPP and Bonjour conformance tests.  This tool is
// inspired by the UNIX "find" command, thus its name.
//
// Copyright © 2021-2023 by OpenPrinting.
// Copyright © 2020 by the IEEE-ISTO Printer Working Group
// Copyright © 2008-2018 by Apple Inc.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#include <cups/cups-private.h>
#ifdef _WIN32
#  include <process.h>
#  include <sys/timeb.h>
#else
#  include <sys/wait.h>
#endif // _WIN32
#include <regex.h>
#include <cups/dnssd.h>

#ifndef _WIN32
extern char **environ;			// Process environment variables
#endif // !_WIN32


//
// Structures...
//

typedef enum ippfind_exit_e		// Exit codes
{
  IPPFIND_EXIT_TRUE = 0,		// OK and result is true
  IPPFIND_EXIT_FALSE,			// OK but result is false
  IPPFIND_EXIT_BONJOUR,			// Browse/resolve failure
  IPPFIND_EXIT_SYNTAX,			// Bad option or syntax error
  IPPFIND_EXIT_MEMORY			// Out of memory
} ippfind_exit_t;

typedef enum ippfind_op_e		// Operations for expressions
{
  // "Evaluation" operations
  IPPFIND_OP_NONE,			// No operation
  IPPFIND_OP_AND,			// Logical AND of all children
  IPPFIND_OP_OR,			// Logical OR of all children
  IPPFIND_OP_TRUE,			// Always true
  IPPFIND_OP_FALSE,			// Always false
  IPPFIND_OP_IS_LOCAL,			// Is a local service
  IPPFIND_OP_IS_REMOTE,			// Is a remote service
  IPPFIND_OP_DOMAIN_REGEX,		// Domain matches regular expression
  IPPFIND_OP_NAME_REGEX,		// Name matches regular expression
  IPPFIND_OP_NAME_LITERAL,		// Name matches literal string
  IPPFIND_OP_HOST_REGEX,		// Hostname matches regular expression
  IPPFIND_OP_PORT_RANGE,		// Port matches range
  IPPFIND_OP_PATH_REGEX,		// Path matches regular expression
  IPPFIND_OP_TXT_EXISTS,		// TXT record key exists
  IPPFIND_OP_TXT_REGEX,			// TXT record key matches regular expression
  IPPFIND_OP_URI_REGEX,			// URI matches regular expression

  // "Output" operations
  IPPFIND_OP_EXEC,			// Execute when true
  IPPFIND_OP_LIST,			// List when true
  IPPFIND_OP_PRINT_NAME,		// Print URI when true
  IPPFIND_OP_PRINT_URI,			// Print name when true
  IPPFIND_OP_QUIET			// No output when true
} ippfind_op_t;

typedef struct ippfind_expr_s		// Expression
{
  struct ippfind_expr_s
		*prev,			// Previous expression
		*next,			// Next expression
		*parent,		// Parent expressions
		*child;			// Child expressions
  ippfind_op_t	op;			// Operation code (see above)
  bool		invert;			// Invert the result?
  char		*name;			// TXT record key or literal name
  regex_t	re;			// Regular expression for matching
  int		range[2];		// Port number range
  size_t	num_args;		// Number of arguments for exec
  char		**args;			// Arguments for exec
} ippfind_expr_t;

typedef struct ippfind_srvs_s		// Services array
{
  cups_rwlock_t	rwlock;			// R/W lock
  cups_array_t	*services;		// Services array
} ippfind_srvs_t;

typedef struct ippfind_srv_s		// Service information
{
  cups_dnssd_resolve_t *resolve;	// Resolve request
  char		*name,			// Service name
		*domain,		// Domain name
		*regtype,		// Registration type
		*fullName,		// Full name
		*host,			// Hostname
		*resource,		// Resource path
		*uri;			// URI
  size_t	num_txt;		// Number of TXT record keys
  cups_option_t	*txt;			// TXT record keys
  int		port;			// Port number
  bool		is_local,		// Is a local service?
		is_processed,		// Did we process the service?
		is_resolved;		// Got the resolve data?
} ippfind_srv_t;


//
// Local globals...
//

static cups_dnssd_t *dnssd;		// DNS-SD context
static int	address_family = AF_UNSPEC;
					// Address family for LIST
static bool	bonjour_error = false;	// Error browsing/resolving?
static double	bonjour_timeout = 1.0;	// Timeout in seconds
static int	ipp_version = 20;	// IPP version for LIST


//
// Local functions...
//

static void		browse_callback(cups_dnssd_browse_t *browse, void *context, cups_dnssd_flags_t flags, uint32_t if_index, const char *serviceName, const char *regtype, const char *replyDomain);
static int		compare_services(ippfind_srv_t *a, ippfind_srv_t *b);
static int		eval_expr(ippfind_srv_t *service, ippfind_expr_t *expressions);
static int		exec_program(ippfind_srv_t *service, size_t num_args, char **args);
static ippfind_srv_t	*get_service(ippfind_srvs_t *services, const char *serviceName, const char *regtype, const char *replyDomain) _CUPS_NONNULL(1,2,3,4);
static double		get_time(void);
static int		list_service(ippfind_srv_t *service);
static ippfind_expr_t	*new_expr(ippfind_op_t op, bool invert, const char *value, const char *regex, char **args);
static void		resolve_callback(cups_dnssd_resolve_t *resolve, void *context, cups_dnssd_flags_t flags, uint32_t if_index, const char *fullName, const char *hostTarget, uint16_t port, size_t num_txt, cups_option_t *txt);
static void		set_service_uri(ippfind_srv_t *service);
static int		usage(FILE *out);
#if _WIN32
static char		*win32_escape_dup(const char *s);
#endif // _WIN32


//
// 'main()' - Browse for printers.
//

int					// O - Exit status
main(int  argc,				// I - Number of command-line args
     char *argv[])			// I - Command-line arguments
{
  int			i,		// Looping var
			status = IPPFIND_EXIT_FALSE;
					// Exit status
  bool			have_output = false;
					// Have output expression
  const char		*opt,		// Option character
			*search;	// Current browse/resolve string
  cups_array_t		*searches;	// Things to browse/resolve
  ippfind_srvs_t	services;	// Services array
  ippfind_srv_t		*service;	// Current service
  ippfind_expr_t	*expressions = NULL,
					// Expression tree
			*temp = NULL,	// New expression
			*parent = NULL,	// Parent expression
			*current = NULL,// Current expression
			*parens[100];	// Markers for parenthesis
  int			num_parens = 0;	// Number of parenthesis
  ippfind_op_t		logic = IPPFIND_OP_AND;
					// Logic for next expression
  bool			invert = false;	// Invert expression?
  double		endtime;	// End time
  static const char * const ops[] =	// Node operation names
  {
    "NONE",
    "AND",
    "OR",
    "TRUE",
    "FALSE",
    "IS_LOCAL",
    "IS_REMOTE",
    "DOMAIN_REGEX",
    "NAME_REGEX",
    "NAME_LITERAL",
    "HOST_REGEX",
    "PORT_RANGE",
    "PATH_REGEX",
    "TXT_EXISTS",
    "TXT_REGEX",
    "URI_REGEX",
    "EXEC",
    "LIST",
    "PRINT_NAME",
    "PRINT_URI",
    "QUIET"
  };


  // Initialize the locale...
  cupsLangSetLocale(argv);

  // Create arrays to track services and things we want to browse/resolve...
  searches = cupsArrayNew(NULL, NULL, NULL, 0, NULL, NULL);

  cupsRWInit(&services.rwlock);
  services.services = cupsArrayNew((cups_array_cb_t)compare_services, NULL, NULL, 0, NULL, NULL);

  // Parse command-line...
  if (getenv("IPPFIND_DEBUG"))
  {
    for (i = 1; i < argc; i ++)
      fprintf(stderr, "argv[%d]=\"%s\"\n", i, argv[i]);
  }

  for (i = 1; i < argc; i ++)
  {
    if (argv[i][0] == '-')
    {
      if (argv[i][1] == '-')
      {
        // Parse --option options...
        if (!strcmp(argv[i], "--and"))
        {
          if (logic == IPPFIND_OP_OR)
          {
            cupsLangPrintf(stderr, _("%s: Cannot use --and after --or."), "ippfind");
            return (usage(stderr));
          }

          if (!current)
          {
            cupsLangPrintf(stderr, _("%s: Missing expression before \"--and\"."), "ippfind");
            return (usage(stderr));
          }

	  temp = NULL;
        }
        else if (!strcmp(argv[i], "--domain"))
        {
          i ++;
          if (i >= argc)
          {
            cupsLangPrintf(stderr, _("%s: Missing regular expression after '%s'."), "ippfind", "--domain");
            return (usage(stderr));
          }

          temp = new_expr(IPPFIND_OP_DOMAIN_REGEX, invert, NULL, argv[i], NULL);
        }
        else if (!strcmp(argv[i], "--exec"))
        {
          i ++;
          if (i >= argc)
          {
            cupsLangPrintf(stderr, _("%s: Missing program after '%s'."), "ippfind", "--exec");
            return (usage(stderr));
          }

          temp = new_expr(IPPFIND_OP_EXEC, invert, NULL, NULL, argv + i);

          while (i < argc)
          {
            if (!strcmp(argv[i], ";"))
              break;
            else
              i ++;
          }

          if (i >= argc)
          {
            cupsLangPrintf(stderr, _("%s: Missing semi-colon after '%s'."), "ippfind", "--exec");
            return (usage(stderr));
          }

          have_output = true;
        }
        else if (!strcmp(argv[i], "--false"))
        {
          temp = new_expr(IPPFIND_OP_FALSE, invert, NULL, NULL, NULL);
        }
        else if (!strcmp(argv[i], "--help"))
        {
          return (usage(stdout));
        }
        else if (!strcmp(argv[i], "--host"))
        {
          i ++;
          if (i >= argc)
          {
            cupsLangPrintf(stderr, _("%s: Missing regular expression after '%s'."), "ippfind", "--host");
            return (usage(stderr));
          }

          temp = new_expr(IPPFIND_OP_HOST_REGEX, invert, NULL, argv[i], NULL);
        }
        else if (!strcmp(argv[i], "--ls"))
        {
          temp        = new_expr(IPPFIND_OP_LIST, invert, NULL, NULL, NULL);
          have_output = true;
        }
        else if (!strcmp(argv[i], "--local"))
        {
          temp = new_expr(IPPFIND_OP_IS_LOCAL, invert, NULL, NULL, NULL);
        }
        else if (!strcmp(argv[i], "--literal-name"))
        {
          i ++;
          if (i >= argc)
          {
            cupsLangPrintf(stderr, _("%s: Missing name after '%s'."), "ippfind", "--literal-name");
            return (usage(stderr));
          }

          temp = new_expr(IPPFIND_OP_NAME_LITERAL, invert, argv[i], NULL, NULL);
        }
        else if (!strcmp(argv[i], "--name"))
        {
          i ++;
          if (i >= argc)
          {
            cupsLangPrintf(stderr, _("%s: Missing regular expression after '%s'."), "ippfind", "--name");
            return (usage(stderr));
          }

          temp = new_expr(IPPFIND_OP_NAME_REGEX, invert, NULL, argv[i], NULL);
        }
        else if (!strcmp(argv[i], "--not"))
        {
          invert = true;
        }
        else if (!strcmp(argv[i], "--or"))
        {
          if (!current)
          {
            cupsLangPrintf(stderr, _("%s: Missing expression before '--or'."), "ippfind");
            return (usage(stderr));
          }

          logic = IPPFIND_OP_OR;

          if (parent && parent->op == IPPFIND_OP_OR)
          {
            // Already setup to do "foo --or bar --or baz"...
            temp = NULL;
          }
          else if (!current->prev && parent)
          {
            // Change parent node into an OR node...
            parent->op = IPPFIND_OP_OR;
            temp       = NULL;
          }
          else if (!current->prev)
          {
            // Need to group "current" in a new OR node...
	    temp            = new_expr(IPPFIND_OP_OR, 0, NULL, NULL, NULL);
            temp->parent    = parent;
            temp->child     = current;
            current->parent = temp;

            if (parent)
              parent->child = temp;
            else
              expressions = temp;

	    parent = temp;
	    temp   = NULL;
	  }
	  else
	  {
	    // Need to group previous expressions in an AND node, and then put that in an OR node...
	    temp = new_expr(IPPFIND_OP_AND, 0, NULL, NULL, NULL);

	    while (current->prev)
	    {
	      current->parent = temp;
	      current         = current->prev;
	    }

	    current->parent = temp;
	    temp->child     = current;
	    current         = temp;

	    temp            = new_expr(IPPFIND_OP_OR, 0, NULL, NULL, NULL);
            temp->parent    = parent;
            current->parent = temp;

            if (parent)
              parent->child = temp;
            else
              expressions = temp;

	    parent = temp;
	    temp   = NULL;
	  }
        }
        else if (!strcmp(argv[i], "--path"))
        {
          i ++;
          if (i >= argc)
          {
            cupsLangPrintf(stderr, _("%s: Missing regular expression after '%s'."), "ippfind", "--path");
            return (usage(stderr));
          }

          temp = new_expr(IPPFIND_OP_PATH_REGEX, invert, NULL, argv[i], NULL);
        }
        else if (!strcmp(argv[i], "--port"))
        {
          i ++;
          if (i >= argc)
          {
            cupsLangPrintf(stderr, _("%s: Missing port range after '%s'."), "ippfind", "--port");
            return (usage(stderr));
          }

          temp = new_expr(IPPFIND_OP_PORT_RANGE, invert, argv[i], NULL, NULL);
        }
        else if (!strcmp(argv[i], "--print"))
        {
          temp        = new_expr(IPPFIND_OP_PRINT_URI, invert, NULL, NULL, NULL);
          have_output = true;
        }
        else if (!strcmp(argv[i], "--print-name"))
        {
          temp        = new_expr(IPPFIND_OP_PRINT_NAME, invert, NULL, NULL, NULL);
          have_output = true;
        }
        else if (!strcmp(argv[i], "--quiet"))
        {
          temp        = new_expr(IPPFIND_OP_QUIET, invert, NULL, NULL, NULL);
          have_output = true;
        }
        else if (!strcmp(argv[i], "--remote"))
        {
          temp = new_expr(IPPFIND_OP_IS_REMOTE, invert, NULL, NULL, NULL);
        }
        else if (!strcmp(argv[i], "--true"))
        {
          temp = new_expr(IPPFIND_OP_TRUE, invert, NULL, argv[i], NULL);
        }
        else if (!strcmp(argv[i], "--txt"))
        {
          i ++;
          if (i >= argc)
          {
            cupsLangPrintf(stderr, _("%s: Missing key name after '%s'."), "ippfind", "--txt");
            return (usage(stderr));
          }

          temp = new_expr(IPPFIND_OP_TXT_EXISTS, invert, argv[i], NULL, NULL);
        }
        else if (!strncmp(argv[i], "--txt-", 6))
        {
          const char *key = argv[i] + 6;// TXT key

          i ++;
          if (i >= argc)
          {
            cupsLangPrintf(stderr, _("%s: Missing regular expression after '%s'."), "ippfind", argv[i - 1]);
            return (usage(stderr));
          }

          temp = new_expr(IPPFIND_OP_TXT_REGEX, invert, key, argv[i], NULL);
        }
        else if (!strcmp(argv[i], "--uri"))
        {
          i ++;
          if (i >= argc)
          {
            cupsLangPrintf(stderr, _("%s: Missing regular expression after '%s'."), "ippfind", "--uri");
            return (usage(stderr));
          }

          temp = new_expr(IPPFIND_OP_URI_REGEX, invert, NULL, argv[i], NULL);
        }
        else if (!strcmp(argv[i], "--version"))
        {
          puts(LIBCUPS_VERSION);
          return (0);
        }
        else
        {
	  cupsLangPrintf(stderr, _("%s: Unknown option '%s'."), "ippfind", argv[i]);
	  return (usage(stderr));
	}

        if (temp)
        {
          // Add new expression...
	  if (logic == IPPFIND_OP_AND && current && current->prev && parent && parent->op != IPPFIND_OP_AND)
          {
            // Need to re-group "current" in a new AND node...
            ippfind_expr_t *tempand;	// Temporary AND node

	    tempand = new_expr(IPPFIND_OP_AND, 0, NULL, NULL, NULL);

            // Replace "current" with new AND node at the end of this list...
            current->prev->next = tempand;
            tempand->prev       = current->prev;
            tempand->parent     = parent;

            // Add "current to the new AND node...
            tempand->child  = current;
            current->parent = tempand;
            current->prev   = NULL;
	    parent          = tempand;
	  }

          // Add the new node at current level...
	  temp->parent = parent;
	  temp->prev   = current;

	  if (current)
	    current->next = temp;
	  else if (parent)
	    parent->child = temp;
	  else
	    expressions = temp;

	  current = temp;
          invert  = false;
          logic   = IPPFIND_OP_AND;
          temp    = NULL;
        }
      }
      else
      {
        // Parse -o options
        for (opt = argv[i] + 1; *opt; opt ++)
        {
          switch (*opt)
          {
            case '4' :
                address_family = AF_INET;
                break;

            case '6' :
                address_family = AF_INET6;
                break;

            case 'N' : // Literal name
		i ++;
		if (i >= argc)
		{
		  cupsLangPrintf(stderr, _("%s: Missing name after '%s'."), "ippfind", "-N");
		  return (usage(stderr));
		}

		temp = new_expr(IPPFIND_OP_NAME_LITERAL, invert, argv[i], NULL, NULL);
		break;

            case 'P' :
		i ++;
		if (i >= argc)
		{
		  cupsLangPrintf(stderr, _("%s: Missing port range after '%s'."), "ippfind", "-P");
		  return (usage(stderr));
		}

		temp = new_expr(IPPFIND_OP_PORT_RANGE, invert, argv[i], NULL, NULL);
		break;

            case 'T' :
                i ++;
                if (i >= argc)
		{
		  cupsLangPrintf(stderr, _("%s: Missing timeout for '-T'."), "ippfind");
		  return (usage(stderr));
		}

                bonjour_timeout = atof(argv[i]);
                break;

            case 'V' :
                i ++;
                if (i >= argc)
		{
		  cupsLangPrintf(stderr, _("%s: Missing version for '-V'."), "ippfind");
		  return (usage(stderr));
		}

                if (!strcmp(argv[i], "1.1"))
                {
                  ipp_version = 11;
                }
                else if (!strcmp(argv[i], "2.0"))
                {
                  ipp_version = 20;
                }
                else if (!strcmp(argv[i], "2.1"))
                {
                  ipp_version = 21;
                }
                else if (!strcmp(argv[i], "2.2"))
                {
                  ipp_version = 22;
                }
                else
                {
                  cupsLangPrintf(stderr, _("%s: Bad version \"%s\" for '-V'."), "ippfind", argv[i]);
                  return (usage(stderr));
                }
                break;

            case 'd' :
		i ++;
		if (i >= argc)
		{
		  cupsLangPrintf(stderr, _("%s: Missing regular expression after '%s'."), "ippfind", "-d");
		  return (usage(stderr));
		}

		temp = new_expr(IPPFIND_OP_DOMAIN_REGEX, invert, NULL, argv[i], NULL);
                break;

            case 'h' :
		i ++;
		if (i >= argc)
		{
		  cupsLangPrintf(stderr, _("%s: Missing regular expression after '%s'."), "ippfind", "-h");
		  return (usage(stderr));
		}

		temp = new_expr(IPPFIND_OP_HOST_REGEX, invert, NULL, argv[i], NULL);
                break;

            case 'l' :
		temp        = new_expr(IPPFIND_OP_LIST, invert, NULL, NULL, NULL);
		have_output = true;
                break;

            case 'n' :
		i ++;
		if (i >= argc)
		{
		  cupsLangPrintf(stderr, _("%s: Missing regular expression after '%s'."), "ippfind", "-n");
		  return (usage(stderr));
		}

		temp = new_expr(IPPFIND_OP_NAME_REGEX, invert, NULL, argv[i], NULL);
                break;

            case 'p' :
		temp = new_expr(IPPFIND_OP_PRINT_URI, invert, NULL, NULL, NULL);

		have_output = true;
                break;

            case 'q' :
		temp        = new_expr(IPPFIND_OP_QUIET, invert, NULL, NULL, NULL);
		have_output = true;
                break;

            case 'r' :
		temp = new_expr(IPPFIND_OP_IS_REMOTE, invert, NULL, NULL, NULL);
                break;

            case 's' :
		temp        = new_expr(IPPFIND_OP_PRINT_NAME, invert, NULL, NULL, NULL);
		have_output = true;
                break;

            case 't' :
		i ++;
		if (i >= argc)
		{
		  cupsLangPrintf(stderr, _("%s: Missing key name after '%s'."), "ippfind", "-t");
		  return (usage(stderr));
		}

		temp = new_expr(IPPFIND_OP_TXT_EXISTS, invert, argv[i], NULL, NULL);
                break;

            case 'u' :
		i ++;
		if (i >= argc)
		{
		  cupsLangPrintf(stderr, _("%s: Missing regular expression after '%s'."), "ippfind", "-u");
		  return (usage(stderr));
		}

		temp = new_expr(IPPFIND_OP_URI_REGEX, invert, NULL, argv[i], NULL);
                break;

            case 'x' :
		i ++;
		if (i >= argc)
		{
		  cupsLangPrintf(stderr, _("%s: Missing program after '%s'."), "ippfind", "-x");
		  return (usage(stderr));
		}

		temp = new_expr(IPPFIND_OP_EXEC, invert, NULL, NULL, argv + i);

		while (i < argc)
		{
		  if (!strcmp(argv[i], ";"))
		    break;
		  else
		    i ++;
		}

		if (i >= argc)
		{
		  cupsLangPrintf(stderr, _("%s: Missing semi-colon after '%s'."), "ippfind", "-x");
		  return (usage(stderr));
		}

		have_output = true;
                break;

            default :
                cupsLangPrintf(stderr, _("%s: Unknown option '-%c'."), "ippfind", *opt);
                return (usage(stderr));
          }

	  if (temp)
	  {
	    // Add new expression...
	    if (logic == IPPFIND_OP_AND && current && current->prev && parent && parent->op != IPPFIND_OP_AND)
	    {
	      // Need to re-group "current" in a new AND node...
	      ippfind_expr_t *tempand;	// Temporary AND node

	      tempand = new_expr(IPPFIND_OP_AND, 0, NULL, NULL, NULL);

	      // Replace "current" with new AND node at the end of this list...
	      current->prev->next = tempand;
	      tempand->prev       = current->prev;
	      tempand->parent     = parent;

	      // Add "current to the new AND node...
	      tempand->child  = current;
	      current->parent = tempand;
	      current->prev   = NULL;
	      parent          = tempand;
	    }

	    // Add the new node at current level...
	    temp->parent = parent;
	    temp->prev   = current;

	    if (current)
	      current->next = temp;
	    else if (parent)
	      parent->child = temp;
	    else
	      expressions = temp;

	    current = temp;
	    invert  = false;
	    logic   = IPPFIND_OP_AND;
	    temp    = NULL;
	  }
        }
      }
    }
    else if (!strcmp(argv[i], "("))
    {
      if (num_parens >= 100)
      {
        cupsLangPrintf(stderr, _("%s: Too many parenthesis."), "ippfind");
        return (usage(stderr));
      }

      temp = new_expr(IPPFIND_OP_AND, invert, NULL, NULL, NULL);

      parens[num_parens++] = temp;

      if (current)
      {
	temp->parent  = current->parent;
	current->next = temp;
	temp->prev    = current;
      }
      else
      {
	expressions = temp;
      }

      parent  = temp;
      current = NULL;
      invert  = false;
      logic   = IPPFIND_OP_AND;
    }
    else if (!strcmp(argv[i], ")"))
    {
      if (num_parens <= 0)
      {
        cupsLangPrintf(stderr, _("%s: Missing open parenthesis."), "ippfind");
        return (usage(stderr));
      }

      current = parens[--num_parens];
      parent  = current->parent;
      invert  = false;
      logic   = IPPFIND_OP_AND;
    }
    else if (!strcmp(argv[i], "!"))
    {
      invert = true;
    }
    else
    {
      // _regtype._tcp[,subtype][.domain]
      //
      // OR
      //
      // service-name[._regtype._tcp[.domain]]
      cupsArrayAdd(searches, argv[i]);
    }
  }

  if (num_parens > 0)
  {
    cupsLangPrintf(stderr, _("%s: Missing close parenthesis."), "ippfind");
    return (usage(stderr));
  }

  if (!have_output)
  {
    // Add an implicit --print-uri to the end...
    temp = new_expr(IPPFIND_OP_PRINT_URI, 0, NULL, NULL, NULL);

    if (current)
    {
      while (current->parent)
	current = current->parent;

      current->next = temp;
      temp->prev    = current;
    }
    else
    {
      expressions = temp;
    }
  }

  if (cupsArrayGetCount(searches) == 0)
  {
    // Add an implicit browse for IPP printers ("_ipp._tcp")...
    cupsArrayAdd(searches, "_ipp._tcp");
  }

  if (getenv("IPPFIND_DEBUG"))
  {
    int	indent = 4;			// Indentation

    puts("Expression tree:");
    current = expressions;
    while (current)
    {
      // Print the current node...
      printf("%*s%s%s\n", indent, "", current->invert ? "!" : "", ops[current->op]);

      // Advance to the next node...
      if (current->child)
      {
        current = current->child;
        indent += 4;
      }
      else if (current->next)
      {
        current = current->next;
      }
      else if (current->parent)
      {
        while (current->parent)
        {
	  indent -= 4;
          current = current->parent;
          if (current->next)
            break;
        }

        current = current->next;
      }
      else
      {
        current = NULL;
      }
    }

    puts("\nSearch items:");
    for (search = (const char *)cupsArrayGetFirst(searches); search; search = (const char *)cupsArrayGetNext(searches))
      printf("    %s\n", search);
  }

  // Start up browsing/resolving...
  if ((dnssd = cupsDNSSDNew(NULL, NULL)) == NULL)
    exit(IPPFIND_EXIT_BONJOUR);

  for (search = (const char *)cupsArrayGetFirst(searches); search; search = (const char *)cupsArrayGetNext(searches))
  {
    char	buf[1024],		// Full name string
		*name = NULL,		// Service instance name
		*regtype,		// Registration type
		*domain;		// Domain, if any

    cupsCopyString(buf, search, sizeof(buf));

    if (!strncmp(buf, "_http._", 7) || !strncmp(buf, "_https._", 8) || !strncmp(buf, "_ipp._", 6) || !strncmp(buf, "_ipps._", 7))
    {
      regtype = buf;
    }
    else if ((regtype = strstr(buf, "._")) != NULL)
    {
      if (strcmp(regtype, "._tcp"))
      {
        // "something._protocol._tcp" -> search for something with the given protocol...
	name = buf;
	*regtype++ = '\0';
      }
      else
      {
        // "_protocol._tcp" -> search for everything with the given protocol...
        // name = NULL;
        regtype = buf;
      }
    }
    else
    {
      // "something" -> search for something with IPP protocol...
      name    = buf;
      regtype = "_ipp._tcp";
    }

    for (domain = regtype; *domain; domain ++)
    {
      if (*domain == '.' && domain[1] != '_')
      {
	*domain++ = '\0';
	break;
      }
    }

    if (!*domain)
      domain = NULL;

    if (name)
    {
      // Resolve the given service instance name, regtype, and domain...
      if (!domain)
        domain = "local.";

      service = get_service(&services, name, regtype, domain);

      if (getenv("IPPFIND_DEBUG"))
        fprintf(stderr, "Resolving name=\"%s\", regtype=\"%s\", domain=\"%s\"\n", name, regtype, domain);

      if ((service->resolve = cupsDNSSDResolveNew(dnssd, CUPS_DNSSD_IF_INDEX_ANY, name, regtype, domain, resolve_callback, service)) == NULL)
        exit(IPPFIND_EXIT_BONJOUR);
    }
    else
    {
      // Browse for services of the given type...
      if (getenv("IPPFIND_DEBUG"))
        fprintf(stderr, "Browsing for regtype=\"%s\", domain=\"%s\"\n", regtype, domain);

      if (!cupsDNSSDBrowseNew(dnssd, CUPS_DNSSD_IF_INDEX_ANY, regtype, domain, browse_callback, &services))
	exit(IPPFIND_EXIT_BONJOUR);
    }
  }

  // Process browse/resolve requests...
  if (bonjour_timeout > 1.0)
    endtime = get_time() + bonjour_timeout;
  else
    endtime = get_time() + 300.0;

  while (get_time() < endtime)
  {
    // Process any services that we have found...
    size_t	j,			// Looping var
		count,			// Number of services
		active = 0,		// Number of active resolves
		resolved = 0,		// Number of resolved services
		processed = 0;		// Number of processed services

    cupsRWLockRead(&services.rwlock);
    for (j = 0, count = cupsArrayGetCount(services.services); j < count; j ++)
    {
      service = (ippfind_srv_t *)cupsArrayGetElement(services.services, j);

      if (service->is_processed)
	processed ++;

      if (service->is_resolved)
	resolved ++;

      if (!service->resolve && !service->is_resolved)
      {
        // Found a service, now resolve it (but limit to 50 active resolves...)
	if (active < 50)
	{
	  if ((service->resolve = cupsDNSSDResolveNew(dnssd, CUPS_DNSSD_IF_INDEX_ANY, service->name, service->regtype, service->domain, resolve_callback, service)) == NULL)
	    exit(IPPFIND_EXIT_BONJOUR);

	  active ++;
	}
      }
      else if (service->is_resolved && !service->is_processed)
      {
        // Resolved, not process this service against the expressions...
	cupsDNSSDResolveDelete(service->resolve);
	service->resolve = NULL;

        if (getenv("IPPFIND_DEBUG"))
          fprintf(stderr, "EVAL %s\n", service->uri);

	if (eval_expr(service, expressions))
	  status = IPPFIND_EXIT_TRUE;

	service->is_processed = true;
      }
      else if (service->resolve)
      {
	active ++;
      }
    }
    cupsRWUnlock(&services.rwlock);

    // If we have processed all services we have discovered, then we are done.
    if (getenv("IPPFIND_DEBUG"))
      fprintf(stderr, "STATUS processed=%u, resolved=%u, count=%u\n", (unsigned)processed, (unsigned)resolved, (unsigned)count);

    if (processed > 0 && processed == cupsArrayGetCount(services.services) && bonjour_timeout <= 1.0)
      break;

    // Give the browsers/resolvers some time...
    usleep(250000);
  }

  if (bonjour_error)
    exit(IPPFIND_EXIT_BONJOUR);
  else
    exit(status);
}


//
// 'browse_callback()' - Browse devices.
//

static void
browse_callback(
    cups_dnssd_browse_t *browse,	// I - Browse request
    void                *context,	// I - Services array
    cups_dnssd_flags_t  flags,		// I - Flags
    uint32_t            if_index,	// I - Interface
    const char          *serviceName,	// I - Name of service/device
    const char          *regtype,	// I - Type of service
    const char          *replyDomain)	// I - Service domain
{
  ippfind_srv_t	*service;		// Service


  if (getenv("IPPFIND_DEBUG"))
    fprintf(stderr, "B flags=0x%04X, if_index=%u, serviceName=\"%s\", regtype=\"%s\", replyDomain=\"%s\"\n", flags, if_index, serviceName, regtype, replyDomain);

  (void)browse;

  // Only process "add" data...
  if ((flags & CUPS_DNSSD_FLAGS_ERROR) || !(flags & CUPS_DNSSD_FLAGS_ADD))
    return;

  // Get the device...
  service = get_service((ippfind_srvs_t *)context, serviceName, regtype, replyDomain);
  if (if_index == CUPS_DNSSD_IF_INDEX_LOCAL)
    service->is_local = 1;
}


//
// 'compare_services()' - Compare two devices.
//

static int				// O - Result of comparison
compare_services(ippfind_srv_t *a,	// I - First device
                 ippfind_srv_t *b)	// I - Second device
{
  return (_cups_strcasecmp(a->name, b->name));
}


//
// 'eval_expr()' - Evaluate the expressions against the specified service.
//
// Returns 1 for true and 0 for false.
//

static int				// O - Result of evaluation
eval_expr(ippfind_srv_t  *service,	// I - Service
	  ippfind_expr_t *expressions)	// I - Expressions
{
  ippfind_op_t		logic;		// Logical operation
  int			result;		// Result of current expression
  ippfind_expr_t	*expression;	// Current expression
  const char		*val;		// TXT value

  // Loop through the expressions...
  if (expressions && expressions->parent)
    logic = expressions->parent->op;
  else
    logic = IPPFIND_OP_AND;

  for (expression = expressions; expression; expression = expression->next)
  {
    switch (expression->op)
    {
      default :
      case IPPFIND_OP_AND :
      case IPPFIND_OP_OR :
          if (expression->child)
            result = eval_expr(service, expression->child);
          else
            result = expression->op == IPPFIND_OP_AND;
          break;
      case IPPFIND_OP_TRUE :
          result = 1;
          break;
      case IPPFIND_OP_FALSE :
          result = 0;
          break;
      case IPPFIND_OP_IS_LOCAL :
          result = service->is_local;
          break;
      case IPPFIND_OP_IS_REMOTE :
          result = !service->is_local;
          break;
      case IPPFIND_OP_DOMAIN_REGEX :
          result = !regexec(&(expression->re), service->domain, 0, NULL, 0);
          break;
      case IPPFIND_OP_NAME_REGEX :
          result = !regexec(&(expression->re), service->name, 0, NULL, 0);
          break;
      case IPPFIND_OP_NAME_LITERAL :
          result = !_cups_strcasecmp(expression->name, service->name);
          break;
      case IPPFIND_OP_HOST_REGEX :
          result = !regexec(&(expression->re), service->host, 0, NULL, 0);
          break;
      case IPPFIND_OP_PORT_RANGE :
          result = service->port >= expression->range[0] && service->port <= expression->range[1];
          break;
      case IPPFIND_OP_PATH_REGEX :
          result = !regexec(&(expression->re), service->resource, 0, NULL, 0);
          break;
      case IPPFIND_OP_TXT_EXISTS :
          result = cupsGetOption(expression->name, service->num_txt, service->txt) != NULL;
          break;
      case IPPFIND_OP_TXT_REGEX :
          val = cupsGetOption(expression->name, service->num_txt, service->txt);
	  if (val)
	    result = !regexec(&(expression->re), val, 0, NULL, 0);
	  else
	    result = 0;

	  if (getenv("IPPFIND_DEBUG"))
	    printf("TXT_REGEX of \"%s\": %d\n", val, result);
          break;
      case IPPFIND_OP_URI_REGEX :
          result = !regexec(&(expression->re), service->uri, 0, NULL, 0);
          break;
      case IPPFIND_OP_EXEC :
          result = exec_program(service, expression->num_args, expression->args);
          break;
      case IPPFIND_OP_LIST :
          result = list_service(service);
          break;
      case IPPFIND_OP_PRINT_NAME :
          cupsLangPuts(stdout, service->name);
          result = 1;
          break;
      case IPPFIND_OP_PRINT_URI :
          cupsLangPuts(stdout, service->uri);
          result = 1;
          break;
      case IPPFIND_OP_QUIET :
          result = 1;
          break;
    }

    if (expression->invert)
      result = !result;

    if (logic == IPPFIND_OP_AND && !result)
      return (0);
    else if (logic == IPPFIND_OP_OR && result)
      return (1);
  }

  return (logic == IPPFIND_OP_AND);
}


//
// 'exec_program()' - Execute a program for a service.
//

static int				// O - 1 if program terminated successfully, 0 otherwise
exec_program(ippfind_srv_t *service,	// I - Service
             size_t        num_args,	// I - Number of command-line args
             char          **args)	// I - Command-line arguments
{
  char		**myargv,		// Command-line arguments
		**myenvp,		// Environment variables
		*ptr,			// Pointer into variable
		domain[1024],		// IPPFIND_SERVICE_DOMAIN
		hostname[1024],		// IPPFIND_SERVICE_HOSTNAME
		name[256],		// IPPFIND_SERVICE_NAME
		port[32],		// IPPFIND_SERVICE_PORT
		regtype[256],		// IPPFIND_SERVICE_REGTYPE
		scheme[128],		// IPPFIND_SERVICE_SCHEME
		uri[1024],		// IPPFIND_SERVICE_URI
		txt[100][256];		// IPPFIND_TXT_foo
  size_t	i,			// Looping var
		myenvc;			// Number of environment variables
  int		status;			// Exit status of program
#ifndef _WIN32
  char		program[1024];		// Program to execute
  int		pid;			// Process ID
#endif // !_WIN32


  // Environment variables...
  snprintf(domain, sizeof(domain), "IPPFIND_SERVICE_DOMAIN=%s", service->domain);
  snprintf(hostname, sizeof(hostname), "IPPFIND_SERVICE_HOSTNAME=%s", service->host);
  snprintf(name, sizeof(name), "IPPFIND_SERVICE_NAME=%s", service->name);
  snprintf(port, sizeof(port), "IPPFIND_SERVICE_PORT=%d", service->port);
  snprintf(regtype, sizeof(regtype), "IPPFIND_SERVICE_REGTYPE=%s", service->regtype);
  snprintf(scheme, sizeof(scheme), "IPPFIND_SERVICE_SCHEME=%s", !strncmp(service->regtype, "_http._tcp", 10) ? "http" : !strncmp(service->regtype, "_https._tcp", 11) ? "https" : !strncmp(service->regtype, "_ipp._tcp", 9) ? "ipp" : !strncmp(service->regtype, "_ipps._tcp", 10) ? "ipps" : "lpd");
  snprintf(uri, sizeof(uri), "IPPFIND_SERVICE_URI=%s", service->uri);
  for (i = 0; i < service->num_txt && i < 100; i ++)
  {
    snprintf(txt[i], sizeof(txt[i]), "IPPFIND_TXT_%s=%s", service->txt[i].name, service->txt[i].value);
    for (ptr = txt[i] + 12; *ptr && *ptr != '='; ptr ++)
      *ptr = (char)_cups_toupper(*ptr);
  }

  for (i = 0, myenvc = 7 + service->num_txt; environ[i]; i ++)
  {
    if (strncmp(environ[i], "IPPFIND_", 8))
      myenvc ++;
  }

  if ((myenvp = calloc(sizeof(char *), (size_t)(myenvc + 1))) == NULL)
  {
    cupsLangPrintf(stderr, _("%s: Out of memory."), "ippfind");
    exit(IPPFIND_EXIT_MEMORY);
  }

  for (i = 0, myenvc = 0; environ[i]; i ++)
  {
    if (strncmp(environ[i], "IPPFIND_", 8))
      myenvp[myenvc++] = environ[i];
  }

  myenvp[myenvc++] = domain;
  myenvp[myenvc++] = hostname;
  myenvp[myenvc++] = name;
  myenvp[myenvc++] = port;
  myenvp[myenvc++] = regtype;
  myenvp[myenvc++] = scheme;
  myenvp[myenvc++] = uri;

  for (i = 0; i < service->num_txt && i < 100; i ++)
    myenvp[myenvc++] = txt[i];

  // Allocate and copy command-line arguments...
  if ((myargv = calloc(sizeof(char *), (size_t)(num_args + 1))) == NULL)
  {
    cupsLangPrintf(stderr, _("%s: Out of memory."), "ippfind");
    exit(IPPFIND_EXIT_MEMORY);
  }

  for (i = 0; i < num_args; i ++)
  {
    if (strchr(args[i], '{'))
    {
      char	temp[2048],		// Temporary string
		*tptr,			// Pointer into temporary string
		keyword[256],		// {keyword}
		*kptr;			// Pointer into keyword

      for (ptr = args[i], tptr = temp; *ptr; ptr ++)
      {
        if (*ptr == '{')
        {
          // Do a {var} substitution...
          for (kptr = keyword, ptr ++; *ptr && *ptr != '}'; ptr ++)
          {
            if (kptr < (keyword + sizeof(keyword) - 1))
              *kptr++ = *ptr;
          }

          if (*ptr != '}')
          {
            cupsLangPrintf(stderr, _("%s: Missing close brace in substitution."), "ippfind");
            exit(IPPFIND_EXIT_SYNTAX);
          }

          *kptr = '\0';
          if (!keyword[0] || !strcmp(keyword, "service_uri"))
          {
	    cupsCopyString(tptr, service->uri, sizeof(temp) - (size_t)(tptr - temp));
	  }
	  else if (!strcmp(keyword, "service_domain"))
	  {
	    cupsCopyString(tptr, service->domain, sizeof(temp) - (size_t)(tptr - temp));
	  }
	  else if (!strcmp(keyword, "service_hostname"))
	  {
	    cupsCopyString(tptr, service->host, sizeof(temp) - (size_t)(tptr - temp));
	  }
	  else if (!strcmp(keyword, "service_name"))
	  {
	    cupsCopyString(tptr, service->name, sizeof(temp) - (size_t)(tptr - temp));
	  }
	  else if (!strcmp(keyword, "service_path"))
	  {
	    cupsCopyString(tptr, service->resource, sizeof(temp) - (size_t)(tptr - temp));
	  }
	  else if (!strcmp(keyword, "service_port"))
	  {
	    cupsCopyString(tptr, port + 21, sizeof(temp) - (size_t)(tptr - temp));
	  }
	  else if (!strcmp(keyword, "service_scheme"))
	  {
	    cupsCopyString(tptr, scheme + 22, sizeof(temp) - (size_t)(tptr - temp));
	  }
	  else if (!strncmp(keyword, "txt_", 4))
	  {
	    const char *val = cupsGetOption(keyword + 4, service->num_txt, service->txt);
	    if (val)
	      cupsCopyString(tptr, val, sizeof(temp) - (size_t)(tptr - temp));
	    else
	      *tptr = '\0';
	  }
	  else
	  {
	    cupsLangPrintf(stderr, _("%s: Unknown variable '{%s}'."), "ippfind", keyword);
	    exit(IPPFIND_EXIT_SYNTAX);
	  }

	  tptr += strlen(tptr);
	}
	else if (tptr < (temp + sizeof(temp) - 1))
	{
	  *tptr++ = *ptr;
	}
      }

      *tptr = '\0';
#if _WIN32
      myargv[i] = win32_escape_dup(temp);
    }
    else
    {
      myargv[i] = win32_escape_dup(args[i]);
    }
#else
      myargv[i] = strdup(temp);
    }
    else
    {
      myargv[i] = strdup(args[i]);
    }
#endif // _WIN32
  }

#if _WIN32
  if (getenv("IPPFIND_DEBUG"))
  {
    printf("\nProgram:\n    %s\n", args[0]);
    puts("\nArguments:");
    for (i = 0; i < num_args; i ++)
      printf("    %s\n", myargv[i]);
    puts("\nEnvironment:");
    for (i = 0; i < myenvc; i ++)
      printf("    %s\n", myenvp[i]);
  }

  status = _spawnvpe(_P_WAIT, args[0], myargv, myenvp);

#else
  // Execute the program...
  if (strchr(args[0], '/') && !access(args[0], X_OK))
  {
    cupsCopyString(program, args[0], sizeof(program));
  }
  else if (!cupsFileFind(args[0], getenv("PATH"), 1, program, sizeof(program)))
  {
    cupsLangPrintf(stderr, _("%s: Unable to execute '%s': %s"), "ippfind", args[0], strerror(ENOENT));
    exit(IPPFIND_EXIT_SYNTAX);
  }

  if (getenv("IPPFIND_DEBUG"))
  {
    printf("\nProgram:\n    %s\n", program);
    puts("\nArguments:");
    for (i = 0; i < num_args; i ++)
      printf("    %s\n", myargv[i]);
    puts("\nEnvironment:");
    for (i = 0; i < myenvc; i ++)
      printf("    %s\n", myenvp[i]);
  }

  if ((pid = fork()) == 0)
  {
    // Child comes here...
    execve(program, myargv, myenvp);
    exit(1);
  }
  else if (pid < 0)
  {
    cupsLangPrintf(stderr, _("%s: Unable to execute '%s': %s"), "ippfind", args[0], strerror(errno));
    exit(IPPFIND_EXIT_SYNTAX);
  }
  else
  {
    // Wait for it to complete...
    while (wait(&status) != pid)
      ;
  }
#endif // _WIN32

  // Free memory...
  for (i = 0; i < num_args; i ++)
    free(myargv[i]);

  free(myargv);
  free(myenvp);

  // Return whether the program succeeded or crashed...
  if (getenv("IPPFIND_DEBUG"))
  {
#ifdef _WIN32
    printf("Exit Status: %d\n", status);
#else
    if (WIFEXITED(status))
      printf("Exit Status: %d\n", WEXITSTATUS(status));
    else
      printf("Terminating Signal: %d\n", WTERMSIG(status));
#endif // _WIN32
  }

  return (status == 0);
}


//
// 'get_service()' - Create or update a device.
//

static ippfind_srv_t *			// O - Service
get_service(ippfind_srvs_t *services,	// I - Service array
	    const char     *serviceName,// I - Name of service/device
	    const char     *regtype,	// I - Type of service
	    const char     *replyDomain)// I - Service domain
{
  size_t	i,			// Looping var
		count;			// Number of services
  ippfind_srv_t	key,			// Search key
		*service;		// Service
  char		fullName[1024];		// Full name for query


  // See if this is a new device...
  key.name    = (char *)serviceName;
  key.regtype = (char *)regtype;

  cupsRWLockRead(&services->rwlock);
  for (i = 0, count = cupsArrayGetCount(services->services); i < count; i ++)
  {
    service = (ippfind_srv_t *)cupsArrayGetElement(services->services, i);

    if (!_cups_strcasecmp(service->name, key.name) && !strcmp(service->regtype, key.regtype))
    {
      cupsRWUnlock(&services->rwlock);
      return (service);
    }
    else if (_cups_strcasecmp(service->name, key.name) > 0)
    {
      break;
    }
  }
  cupsRWUnlock(&services->rwlock);

  // Yes, add the service...
  if ((service = calloc(sizeof(ippfind_srv_t), 1)) == NULL)
    return (NULL);

  service->name     = strdup(serviceName);
  service->domain   = strdup(replyDomain);
  service->regtype  = strdup(regtype);

  cupsRWLockWrite(&services->rwlock);
  cupsArrayAdd(services->services, service);
  cupsRWUnlock(&services->rwlock);

  // Set the "full name" of this service, which is used for queries and resolves...
  cupsDNSSDAssembleFullName(fullName, sizeof(fullName), serviceName, regtype, replyDomain);
  service->fullName = strdup(fullName);

  return (service);
}


//
// 'get_time()' - Get the current time-of-day in seconds.
//

static double
get_time(void)
{
#ifdef _WIN32
  struct _timeb curtime;		// Current Windows time

  _ftime(&curtime);

  return (curtime.time + 0.001 * curtime.millitm);

#else
  struct timeval	curtime;	// Current UNIX time

  if (gettimeofday(&curtime, NULL))
    return (0.0);
  else
    return (curtime.tv_sec + 0.000001 * curtime.tv_usec);
#endif // _WIN32
}


//
// 'list_service()' - List the contents of a service.
//

static int				// O - 1 if successful, 0 otherwise
list_service(ippfind_srv_t *service)	// I - Service
{
  http_addrlist_t	*addrlist;	// Address(es) of service
  char			port[10];	// Port number of service


  snprintf(port, sizeof(port), "%d", service->port);

  if ((addrlist = httpAddrGetList(service->host, address_family, port)) == NULL)
  {
    cupsLangPrintf(stdout, "%s unreachable", service->uri);
    return (0);
  }

  if (!strncmp(service->regtype, "_ipp._tcp", 9) || !strncmp(service->regtype, "_ipps._tcp", 10))
  {
    // IPP/IPPS printer
    http_t		*http;		// HTTP connection
    ipp_t		*request,	// IPP request
			*response;	// IPP response
    ipp_attribute_t	*attr;		// IPP attribute
    size_t		i,		// Looping var
			count;		// Number of values
    int			version;	// IPP version
    bool		paccepting;	// printer-is-accepting-jobs value
    ipp_pstate_t	pstate;		// printer-state value
    char		preasons[1024],	// Comma-delimited printer-state-reasons
			*ptr,		// Pointer into reasons
			*end;		// End of reasons buffer
    static const char * const rattrs[] =// Requested attributes
    {
      "printer-is-accepting-jobs",
      "printer-state",
      "printer-state-reasons"
    };

    // Connect to the printer...
    http = httpConnect(service->host, service->port, addrlist, address_family, !strncmp(service->regtype, "_ipps._tcp", 10) ? HTTP_ENCRYPTION_ALWAYS : HTTP_ENCRYPTION_IF_REQUESTED, true, 30000, NULL);

    httpAddrFreeList(addrlist);

    if (!http)
    {
      cupsLangPrintf(stdout, "%s unavailable", service->uri);
      return (0);
    }

    // Get the current printer state...
    response = NULL;
    version  = ipp_version;

    do
    {
      request = ippNewRequest(IPP_OP_GET_PRINTER_ATTRIBUTES);
      ippSetVersion(request, version / 10, version % 10);
      ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri", NULL, service->uri);
      ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name", NULL, cupsGetUser());
      ippAddStrings(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD, "requested-attributes", sizeof(rattrs) / sizeof(rattrs[0]), NULL, rattrs);

      response = cupsDoRequest(http, request, service->resource);

      if (cupsGetError() == IPP_STATUS_ERROR_BAD_REQUEST && version > 11)
        version = 11;
    }
    while (cupsGetError() > IPP_STATUS_OK_EVENTS_COMPLETE && version > 11);

    // Show results...
    if (cupsGetError() > IPP_STATUS_OK_EVENTS_COMPLETE)
    {
      cupsLangPrintf(stdout, "%s unavailable", service->uri);
      return (0);
    }

    if ((attr = ippFindAttribute(response, "printer-state", IPP_TAG_ENUM)) != NULL)
      pstate = (ipp_pstate_t)ippGetInteger(attr, 0);
    else
      pstate = IPP_PSTATE_STOPPED;

    if ((attr = ippFindAttribute(response, "printer-is-accepting-jobs", IPP_TAG_BOOLEAN)) != NULL)
      paccepting = ippGetBoolean(attr, 0);
    else
      paccepting = 0;

    if ((attr = ippFindAttribute(response, "printer-state-reasons", IPP_TAG_KEYWORD)) != NULL)
    {
      cupsCopyString(preasons, ippGetString(attr, 0, NULL), sizeof(preasons));

      for (i = 1, count = ippGetCount(attr), ptr = preasons + strlen(preasons), end = preasons + sizeof(preasons) - 1; i < count && ptr < end; i ++, ptr += strlen(ptr))
      {
        *ptr++ = ',';
        cupsCopyString(ptr, ippGetString(attr, i, NULL), (size_t)(end - ptr + 1));
      }
    }
    else
    {
      cupsCopyString(preasons, "none", sizeof(preasons));
    }

    ippDelete(response);
    httpClose(http);

    cupsLangPrintf(stdout, "%s %s %s %s", service->uri, ippEnumString("printer-state", (int)pstate), paccepting ? "accepting-jobs" : "not-accepting-jobs", preasons);
  }
  else if (!strncmp(service->regtype, "_http._tcp", 10) || !strncmp(service->regtype, "_https._tcp", 11))
  {
    // HTTP/HTTPS web page
    http_t		*http;		// HTTP connection
    http_status_t	status;		// HEAD status

    // Connect to the web server...
    http = httpConnect(service->host, service->port, addrlist, address_family, !strncmp(service->regtype, "_ipps._tcp", 10) ? HTTP_ENCRYPTION_ALWAYS : HTTP_ENCRYPTION_IF_REQUESTED, 1, 30000, NULL);

    httpAddrFreeList(addrlist);

    if (!http)
    {
      cupsLangPrintf(stdout, "%s unavailable", service->uri);
      return (0);
    }

    if (!httpWriteRequest(http, "GET", service->resource))
    {
      cupsLangPrintf(stdout, "%s unavailable", service->uri);
      return (0);
    }

    do
    {
      status = httpUpdate(http);
    }
    while (status == HTTP_STATUS_CONTINUE);

    httpFlush(http);
    httpClose(http);

    if (status >= HTTP_STATUS_BAD_REQUEST)
    {
      cupsLangPrintf(stdout, "%s unavailable", service->uri);
      return (0);
    }

    cupsLangPrintf(stdout, "%s available", service->uri);
  }
  else if (!strncmp(service->regtype, "_printer._tcp", 13))
  {
    // LPD printer
    int	sock;				// Socket

    if (!httpAddrConnect(addrlist, &sock, 30000, NULL))
    {
      cupsLangPrintf(stdout, "%s unavailable", service->uri);
      httpAddrFreeList(addrlist);
      return (0);
    }

    cupsLangPrintf(stdout, "%s available", service->uri);
    httpAddrFreeList(addrlist);

    httpAddrClose(NULL, sock);
  }
  else
  {
    cupsLangPrintf(stdout, "%s unsupported", service->uri);
    httpAddrFreeList(addrlist);
    return (0);
  }

  return (1);
}


//
// 'new_expr()' - Create a new expression.
//

static ippfind_expr_t *			// O - New expression
new_expr(ippfind_op_t op,		// I - Operation
         bool         invert,		// I - Invert result?
         const char   *value,		// I - TXT key or port range
	 const char   *regex,		// I - Regular expression
	 char         **args)		// I - Pointer to argument strings
{
  ippfind_expr_t	*temp;		// New expression


  if ((temp = calloc(1, sizeof(ippfind_expr_t))) == NULL)
    exit(IPPFIND_EXIT_MEMORY);

  temp->op     = op;
  temp->invert = invert;

  if (op == IPPFIND_OP_TXT_EXISTS || op == IPPFIND_OP_TXT_REGEX || op == IPPFIND_OP_NAME_LITERAL)
  {
    temp->name = (char *)value;
  }
  else if (op == IPPFIND_OP_PORT_RANGE)
  {
    // Pull port number range of the form "number", "-number" (0-number),
    // "number-" (number-65535), and "number-number".
    if (*value == '-')
    {
      temp->range[1] = atoi(value + 1);
    }
    else if (strchr(value, '-'))
    {
      if (sscanf(value, "%d-%d", temp->range, temp->range + 1) == 1)
        temp->range[1] = 65535;
    }
    else
    {
      temp->range[0] = temp->range[1] = atoi(value);
    }
  }

  if (regex)
  {
    int err = regcomp(&(temp->re), regex, REG_NOSUB | REG_ICASE | REG_EXTENDED);

    if (err)
    {
      char	message[256];		// Error message

      regerror(err, &(temp->re), message, sizeof(message));
      cupsLangPrintf(stderr, _("%s: Bad regular expression: %s"), "ippfind", message);
      exit(IPPFIND_EXIT_SYNTAX);
    }
  }

  if (args)
  {
    size_t	num_args;		// Number of arguments

    for (num_args = 1; args[num_args]; num_args ++)
    {
      if (!strcmp(args[num_args], ";"))
        break;
    }

    temp->num_args = num_args;
    if ((temp->args = malloc(num_args * sizeof(char *))) == NULL)
    {
      regfree(&temp->re);
      free(temp);
      return (NULL);
    }
    memcpy(temp->args, args, (size_t)num_args * sizeof(char *));
  }

  return (temp);
}


//
// 'resolve_callback()' - Process resolve data.
//

static void
resolve_callback(
    cups_dnssd_resolve_t *resolve,	// I - Resolver
    void                 *context,	// I - Service
    cups_dnssd_flags_t   flags,		// I - Flags
    uint32_t             if_index,	// I - Interface index
    const char           *fullName,	// I - Full service name
    const char           *hostTarget,	// I - Hostname
    uint16_t             port,		// I - Port number
    size_t               num_txt,	// I - Number of TXT key/value pairs
    cups_option_t        *txt)		// I - TXT key/value pairs
{
  ippfind_srv_t		*service = (ippfind_srv_t *)context;
					// Service
  size_t		i;		// Looping var
  char			*value;		// Pointer into value


  if (getenv("IPPFIND_DEBUG"))
    fprintf(stderr, "R flags=0x%04X, if_index=%u, fullName=\"%s\", hostTarget=\"%s\", port=%u, num_txt=%u, txt=%p\n", flags, if_index, fullName, hostTarget, port, (unsigned)num_txt, txt);

  (void)resolve;
  (void)if_index;
  (void)fullName;

  // Only process "add" data...
  if (flags & CUPS_DNSSD_FLAGS_ERROR)
  {
    bonjour_error = true;
    return;
  }

  service->is_resolved = true;
  service->host        = strdup(hostTarget);
  service->port        = port;

  value = service->host + strlen(service->host) - 1;
  if (value >= service->host && *value == '.')
    *value = '\0';

  // Loop through the TXT key/value pairs and add them to an array...
  for (i = 0; i < num_txt; i ++)
    service->num_txt = cupsAddOption(txt[i].name, txt[i].value, service->num_txt, &service->txt);

  set_service_uri(service);
}


//
// 'set_service_uri()' - Set the URI of the service.
//

static void
set_service_uri(ippfind_srv_t *service)	// I - Service
{
  char		uri[1024];		// URI
  const char	*path,			// Resource path
		*scheme;		// URI scheme


  if (!strncmp(service->regtype, "_http.", 6))
  {
    scheme = "http";
    path   = cupsGetOption("path", service->num_txt, service->txt);
  }
  else if (!strncmp(service->regtype, "_https.", 7))
  {
    scheme = "https";
    path   = cupsGetOption("path", service->num_txt, service->txt);
  }
  else if (!strncmp(service->regtype, "_ipp.", 5))
  {
    scheme = "ipp";
    path   = cupsGetOption("rp", service->num_txt, service->txt);
  }
  else if (!strncmp(service->regtype, "_ipps.", 6))
  {
    scheme = "ipps";
    path   = cupsGetOption("rp", service->num_txt, service->txt);
  }
  else if (!strncmp(service->regtype, "_printer.", 9))
  {
    scheme = "lpd";
    path   = cupsGetOption("rp", service->num_txt, service->txt);
  }
  else
    return;

  if (!path || !*path)
    path = "/";

  if (*path == '/')
  {
    service->resource = strdup(path);
  }
  else
  {
    snprintf(uri, sizeof(uri), "/%s", path);
    service->resource = strdup(uri);
  }

  httpAssembleURI(HTTP_URI_CODING_ALL, uri, sizeof(uri), scheme, NULL, service->host, service->port, service->resource);
  service->uri = strdup(uri);
}


//
// 'usage()' - Show program usage.
//

static int				// O - Exit status
usage(FILE *out)			// I - Output file
{
  cupsLangPuts(out, _("Usage: ippfind [OPTIONS] REGTYPE[,SUBTYPE][.DOMAIN.] ... [EXPRESSION]\n"
		      "       ippfind [OPTIONS] NAME[.REGTYPE[.DOMAIN.]] ... [EXPRESSION]\n"
		      "       ippfind --help\n"
		      "       ippfind --version"));
  cupsLangPuts(out, _("Options:"));
  cupsLangPuts(out, _("--help                         Show this help"));
  cupsLangPuts(out, _("--version                      Show the program version"));
  cupsLangPuts(out, _("-4                             Connect using IPv4"));
  cupsLangPuts(out, _("-6                             Connect using IPv6"));
  cupsLangPuts(out, _("-T SECONDS                     Set the browse timeout in seconds"));
  cupsLangPuts(out, _("-V VERSION                     Set default IPP version"));
  cupsLangPuts(out, _("Expressions:"));
  cupsLangPuts(out, _("-P NUMBER[-NUMBER]             Match port to number or range"));
  cupsLangPuts(out, _("-d REGEX                       Match domain to regular expression"));
  cupsLangPuts(out, _("-h REGEX                       Match hostname to regular expression"));
  cupsLangPuts(out, _("-l                             List attributes"));
  cupsLangPuts(out, _("-n REGEX                       Match service name to regular expression"));
  cupsLangPuts(out, _("-N NAME                        Match service name to literal name value"));
  cupsLangPuts(out, _("-p                             Print URI if true"));
  cupsLangPuts(out, _("-q                             Quietly report match via exit code"));
  cupsLangPuts(out, _("-r                             True if service is remote"));
  cupsLangPuts(out, _("-s                             Print service name if true"));
  cupsLangPuts(out, _("-t KEY                         True if the TXT record contains the key"));
  cupsLangPuts(out, _("-u REGEX                       Match URI to regular expression"));
  cupsLangPuts(out, _("-x UTILITY [ARGS ...] ';'      Execute program if true"));
  cupsLangPuts(out, _("--domain REGEX                 Match domain to regular expression"));
  cupsLangPuts(out, _("--exec UTILITY [ARGS ...] ';'  Execute program if true"));
  cupsLangPuts(out, _("--host REGEX                   Match hostname to regular expression"));
  cupsLangPuts(out, _("--literal-name NAME            Match service name to literal name value"));
  cupsLangPuts(out, _("--local                        True if service is local"));
  cupsLangPuts(out, _("--ls                           List attributes"));
  cupsLangPuts(out, _("--name REGEX                   Match service name to regular expression"));
  cupsLangPuts(out, _("--path REGEX                   Match resource path to regular expression"));
  cupsLangPuts(out, _("--port NUMBER[-NUMBER]         Match port to number or range"));
  cupsLangPuts(out, _("--print                        Print URI if true"));
  cupsLangPuts(out, _("--print-name                   Print service name if true"));
  cupsLangPuts(out, _("--quiet                        Quietly report match via exit code"));
  cupsLangPuts(out, _("--remote                       True if service is remote"));
  cupsLangPuts(out, _("--txt KEY                      True if the TXT record contains the key"));
  cupsLangPuts(out, _("--txt-* REGEX                  Match TXT record key to regular expression"));
  cupsLangPuts(out, _("--uri REGEX                    Match URI to regular expression"));
  cupsLangPuts(out, _("Modifiers:"));
  cupsLangPuts(out, _("( EXPRESSIONS )                Group expressions"));
  cupsLangPuts(out, _("! EXPRESSION                   Unary NOT of expression"));
  cupsLangPuts(out, _("--not EXPRESSION               Unary NOT of expression"));
  cupsLangPuts(out, _("--false                        Always false"));
  cupsLangPuts(out, _("--true                         Always true"));
  cupsLangPuts(out, _("EXPRESSION EXPRESSION          Logical AND"));
  cupsLangPuts(out, _("EXPRESSION --and EXPRESSION    Logical AND"));
  cupsLangPuts(out, _("EXPRESSION --or EXPRESSION     Logical OR"));
  cupsLangPuts(out, _("Substitutions:"));
  cupsLangPuts(out, _("{}                             URI"));
  cupsLangPuts(out, _("{service_domain}               Domain name"));
  cupsLangPuts(out, _("{service_hostname}             Fully-qualified domain name"));
  cupsLangPuts(out, _("{service_name}                 Service instance name"));
  cupsLangPuts(out, _("{service_port}                 Port number"));
  cupsLangPuts(out, _("{service_regtype}              DNS-SD registration type"));
  cupsLangPuts(out, _("{service_scheme}               URI scheme"));
  cupsLangPuts(out, _("{service_uri}                  URI"));
  cupsLangPuts(out, _("{txt_*}                        Value of TXT record key"));
  cupsLangPuts(out, _("Environment Variables:"));
  cupsLangPuts(out, _("IPPFIND_SERVICE_DOMAIN         Domain name"));
  cupsLangPuts(out, _("IPPFIND_SERVICE_HOSTNAME       Fully-qualified domain name"));
  cupsLangPuts(out, _("IPPFIND_SERVICE_NAME           Service instance name"));
  cupsLangPuts(out, _("IPPFIND_SERVICE_PORT           Port number"));
  cupsLangPuts(out, _("IPPFIND_SERVICE_REGTYPE        DNS-SD registration type"));
  cupsLangPuts(out, _("IPPFIND_SERVICE_SCHEME         URI scheme"));
  cupsLangPuts(out, _("IPPFIND_SERVICE_URI            URI"));
  cupsLangPuts(out, _("IPPFIND_TXT_*                  Value of TXT record key"));

  return (out == stdout ? IPPFIND_EXIT_TRUE : IPPFIND_EXIT_FALSE);
}


//
// 'win32_escape_dup()' - Escape and duplicate a string.
//
// This function puts the string in double quotes, escaping characters in the
// string as needed using Windows' insane command-line argument parsing rules.
//

#if _WIN32
static char *				// O - Duplicated string
win32_escape_dup(const char *s)		// I - Original string
{
  char		*d,			// Output string
		*dptr;			// Pointer into output string
  size_t	dlen;			// Length of output string
  const char	*sptr;			// Pointer into original string


  // Figure out the length of the escaped string...
  for (dlen = 2, sptr = s; *sptr; sptr ++)
  {
    if (*sptr == '\\')
    {
      if (sptr[1] == '\"' || sptr[1] == '\\')
      {
        // \" and \\ need to be converted to \\\" or \\\\...
        dlen += 4;
        sptr ++;
      }
      else
      {
        // A lone \ can stand on its own...
        dlen ++;
      }
    }
    else if (*sptr == '\"')
    {
      // Need to replace " with ""...
      dlen += 2;
    }
    else
    {
      // Not a special character so it is fine on its own...
      dlen ++;
    }
  }

  // Allocate memory (plus nul)...
  if ((d = calloc(1, dlen + 1)) == NULL)
    return (NULL);

  // Copy and escape...
  *d = '\"';
  for (dptr = d + 1, sptr = s; *sptr; sptr ++)
  {
    if (*sptr == '\\')
    {
      if (sptr[1] == '\"' || sptr[1] == '\\')
      {
        // \" and \\ need to be converted to \\\" or \\\\...
        sptr ++;

        *dptr++ = '\\';
        *dptr++ = '\\';
        *dptr++ = '\\';
        *dptr++ = *sptr;
      }
      else
      {
        // A lone \ can stand on its own...
        *dptr++ = '\\';
      }
    }
    else if (*sptr == '\"')
    {
      // Need to replace " with ""...
      *dptr++ = '\"';
      *dptr++ = '\"';
    }
    else
    {
      // Not a special character so it is fine on its own...
      *dptr++ = *sptr;
    }
  }

  *dptr++ = '\"';
  *dptr   = '\0';

  return (d);
}
#endif // _WIN32
