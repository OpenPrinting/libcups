//
// Sorted array routines for CUPS.
//
// Copyright © 2021-2025 by OpenPrinting.
// Copyright © 2007-2014 by Apple Inc.
// Copyright © 1997-2007 by Easy Software Products.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#include <cups/cups.h>
#include "string-private.h"
#include "debug-internal.h"


//
// Limits...
//

#define _CUPS_MAXSAVE	32		// Maximum number of saves


//
// Types and structures...
//

struct _cups_array_s			// CUPS array structure
{
  // The current implementation uses an insertion sort into an array of
  // sorted pointers.  We leave the array type private/opaque so that we
  // can change the underlying implementation without affecting the users
  // of this API.

  size_t		num_elements,	// Number of array elements
			alloc_elements,	// Allocated array elements
			current,	// Current element
			insert,		// Last inserted element
			num_saved,	// Number of saved elements
			saved[_CUPS_MAXSAVE];
					// Saved elements
  void			**elements;	// Array elements
  cups_array_cb_t	compare;	// Element comparison function
  bool			unique;		// Are all elements unique?
  void			*data;		// User data passed to compare
  cups_ahash_cb_t	hashfunc;	// Hash function
  size_t		hashsize,	// Size of hash
			*hash;		// Hash array
  cups_acopy_cb_t	copyfunc;	// Copy function
  cups_afree_cb_t	freefunc;	// Free function
};


//
// Local functions...
//

static bool	cups_array_add(cups_array_t *a, void *e, bool insert);
static size_t	cups_array_find(cups_array_t *a, void *e, size_t prev, int *rdiff);


//
// 'cupsArrayAdd()' - Add an element to an array.
//
// This function adds an element to an array.  When adding an element to a
// sorted array, non-unique elements are appended at the end of the run of
// identical elements.  For unsorted arrays, the element is appended to the end
// of the array.
//

bool					// O - `true` on success, `false` on failure
cupsArrayAdd(cups_array_t *a,		// I - Array
             void         *e)		// I - Element
{
  // Range check input...
  if (!a || !e)
    return (false);

  // Append the element...
  return (cups_array_add(a, e, false));
}


//
// 'cupsArrayAddStrings()' - Add zero or more delimited strings to an array.
//
// This function adds zero or more delimited strings to an array created using
// the @link cupsArrayNewStrings@ function. Duplicate strings are *not* added.
// If the string pointer "s" is `NULL` or the empty string, no strings are
// added to the array.  If "delim" is the space character, then all whitespace
// is recognized as a delimiter.
//
// Strings can be delimited by quotes ("foo", 'bar') and curly braces ("{foo}"),
// and characters can be escaped using the backslash (\) character.  Outer
// quotes are stripped but inner ones are preserved.
//

bool					// O - `true` on success, `false` on failure
cupsArrayAddStrings(cups_array_t *a,	// I - Array
                    const char   *s,	// I - Delimited strings
                    char         delim)	// I - Delimiter character
{
  char		*buffer,		// Copy of string
		*start,			// Start of string
		*end;			// End of string
  bool		status = true;		// Status of add
  char		stack[_CUPS_MAXSAVE];	// Quoting stack
  int		spos = -1;		// Stack position


  // Range check input...
  if (!a)
    return (false);

  if (!a || !s || !*s || !delim)
    return (true);

  if (delim == ' ')
  {
    // Skip leading whitespace...
    while (*s && isspace(*s & 255))
      s ++;
  }

  if (!strchr(s, delim) && (delim != ' ' || (!strchr(s, '\t') && !strchr(s, '\n'))) && *s != '\'' && *s != '\"')
  {
    // String doesn't contain a delimiter, so add it as a single value...
    if (!cupsArrayFind(a, (void *)s))
      status = cupsArrayAdd(a, (void *)s);
  }
  else if ((buffer = strdup(s)) == NULL)
  {
    status = false;
  }
  else
  {
    for (start = end = buffer; *end; start = end)
    {
      // Find the end of the current delimited string and see if we need to add it...
      while (*end)
      {
        if (*end == '\\' && end[1])
        {
          // Skip escaped character...
          _cups_strcpy(end, end + 1);
          end ++;
        }
        else if (spos >= 0 && *end == stack[spos])
        {
          // End of quoted value...
          spos --;
          if (spos >= 0 || *end == '}')
            end ++;
          else
            _cups_strcpy(end, end + 1);
        }
        else if (*end == '{' && spos < (int)(sizeof(stack) - 1))
        {
          // Value in curly braces...
          spos ++;
          stack[spos] = '}';
        }
        else if ((*end == '\'' || *end == '\"') && spos < (int)(sizeof(stack) - 1))
        {
          // Value in quotes...
          spos ++;
          stack[spos] = *end;
          if (spos == 0)
            _cups_strcpy(end, end + 1);
        }
        else if (*end == delim || (delim == ' ' && isspace(*end & 255)))
        {
          // Delimiter
          *end++ = '\0';
          break;
        }
        else
          end ++;
      }

      if (!cupsArrayFind(a, start))
        status &= cupsArrayAdd(a, start);
    }

    free(buffer);
  }

  return (status);
}


//
// 'cupsArrayClear()' - Clear an array.
//
// This function is equivalent to removing all elements in the array, so the
// free callback (if any) is called for each element that is removed.
//

void
cupsArrayClear(cups_array_t *a)		// I - Array
{
  // Range check input...
  if (!a)
    return;

  // Free the existing elements as needed..
  if (a->freefunc)
  {
    size_t	i;			// Looping var
    void	**e;			// Current element

    for (i = a->num_elements, e = a->elements; i > 0; i --, e ++)
      (a->freefunc)(*e, a->data);
  }

  // Set the number of elements to 0; we don't actually free the memory
  // here - that is done in cupsArrayDelete()...
  a->num_elements = 0;
  a->current      = SIZE_MAX;
  a->insert       = SIZE_MAX;
  a->unique       = true;
  a->num_saved    = 0;
}


//
// 'cupsArrayDelete()' - Free all memory used by an array.
//
// This function frees all memory used by an array.  The free callback (if any)
// is called for each element in the array.
//

void
cupsArrayDelete(cups_array_t *a)	// I - Array
{
  // Range check input...
  if (!a)
    return;

  // Clear the array...
  cupsArrayClear(a);

  // Free the other buffers...
  free(a->elements);
  free(a->hash);
  free(a);
}


//
// 'cupsArrayDup()' - Duplicate an array.
//

cups_array_t *				// O - Duplicate array
cupsArrayDup(cups_array_t *a)		// I - Array
{
  cups_array_t	*da;			// Duplicate array


  // Range check input...
  if (!a)
    return (NULL);

  // Allocate memory for the array...
  da = calloc(1, sizeof(cups_array_t));
  if (!da)
    return (NULL);

  da->compare   = a->compare;
  da->copyfunc  = a->copyfunc;
  da->freefunc  = a->freefunc;
  da->data      = a->data;
  da->current   = a->current;
  da->insert    = a->insert;
  da->unique    = a->unique;
  da->num_saved = a->num_saved;

  memcpy(da->saved, a->saved, sizeof(a->saved));

  if (a->num_elements)
  {
    // Allocate memory for the elements...
    da->elements = malloc((size_t)a->num_elements * sizeof(void *));
    if (!da->elements)
    {
      free(da);
      return (NULL);
    }

    // Copy the element pointers...
    if (a->copyfunc)
    {
      // Use the copy function to make a copy of each element...
      size_t	i;			// Looping var

      for (i = 0; i < a->num_elements; i ++)
	da->elements[i] = (a->copyfunc)(a->elements[i], a->data);
    }
    else
    {
      // Just copy raw pointers...
      memcpy(da->elements, a->elements, (size_t)a->num_elements * sizeof(void *));
    }

    da->num_elements   = a->num_elements;
    da->alloc_elements = a->num_elements;
  }

  // Return the new array...
  return (da);
}


//
// 'cupsArrayFind()' - Find an element in an array.
//

void *					// O - Element found or @code NULL@
cupsArrayFind(cups_array_t *a,		// I - Array
              void         *e)		// I - Element
{
  size_t	current,		// Current element
		hash;			// Hash index
  int		diff;			// Difference


  // Range check input...
  if (!a || !a->num_elements || !e)
    return (NULL);

  // Look for a match...
  if (a->hash)
  {
    if ((hash = (*(a->hashfunc))(e, a->data)) >= a->hashsize)
    {
      current = a->current;
      hash    = SIZE_MAX;
    }
    else if ((current = a->hash[hash]) >= a->num_elements)
      current = a->current;
  }
  else
  {
    current = a->current;
    hash    = SIZE_MAX;
  }

  current = cups_array_find(a, e, current, &diff);
  if (!diff)
  {
    // Found a match!  If the array does not contain unique values, find the
    // first element that is the same...
    if (!a->unique && a->compare)
    {
      // The array is not unique, find the first match...
      while (current > 0 && !(*(a->compare))(e, a->elements[current - 1], a->data))
        current --;
    }

    a->current = current;

    if (hash < a->hashsize)
      a->hash[hash] = current;

    return (a->elements[current]);
  }
  else
  {
    // No match...
    a->current = SIZE_MAX;

    return (NULL);
  }
}


//
// 'cupsArrayGetCount()' - Get the number of elements in an array.
//

size_t					// O - Number of elements
cupsArrayGetCount(cups_array_t *a)	// I - Array
{
  return (a ? a->num_elements : 0);
}


//
// '_cupsArrayFree()' - Free a string in an array.
//

void
_cupsArrayFree(void *s,			// I - String to free
               void *data)		// I - Callback data (unused)
{
  (void)data;

  free(s);
}


//
// 'cupsArrayGetCurrent()' - Return the current element in an array.
//
// This function returns the current element in an array.  The current element
// is undefined until you call @link cupsArrayFind@, @link cupsArrayGetElement@,
// @link cupsArrayGetFirst@, or @link cupsArrayGetLast@.
//

void *					// O - Element
cupsArrayGetCurrent(cups_array_t *a)	// I - Array
{
  // Range check input...
  if (!a)
    return (NULL);

  // Return the current element...
  if (a->current < a->num_elements)
    return (a->elements[a->current]);
  else
    return (NULL);
}


//
// 'cupsArrayGetFirst()' - Get the first element in an array.
//

void *					// O - First element or `NULL` if the array is empty
cupsArrayGetFirst(cups_array_t *a)	// I - Array
{
  return (cupsArrayGetElement(a, 0));
}


//
// 'cupsArrayGetIndex()' - Get the index of the current element.
//
// This function returns the index of the current element or `SIZE_MAX` if
// there is no current element.  The current element is undefined until you call
// @link cupsArrayFind@, @link cupsArrayGetElement@, @link cupsArrayGetFirst@,
// or @link cupsArrayGetLast@.
//

size_t					// O - Index of the current element, starting at 0
cupsArrayGetIndex(cups_array_t *a)	// I - Array
{
  return (a ? a->current : SIZE_MAX);
}


//
// 'cupsArrayGetInsert()' - Get the index of the last added or inserted element.
//
// This function returns the index of the last added or inserted element or
// `SIZE_MAX` if no elements have been added or inserted.
//

size_t					// O - Index of the last added or inserted element, starting at 0
cupsArrayGetInsert(cups_array_t *a)	// I - Array
{
  return (a ? a->insert : SIZE_MAX);
}


//
// 'cupsArrayGetElement()' - Get the N-th element in the array.
//

void *					// O - N-th element or `NULL`
cupsArrayGetElement(cups_array_t *a,	// I - Array
                    size_t       n)	// I - Index into array, starting at 0
{
  // Range check input...
  if (!a || n >= a->num_elements)
    return (NULL);

  a->current = n;

  return (a->elements[n]);
}


//
// 'cupsArrayGetLast()' - Get the last element in the array.
//

void *					// O - Last element or`NULL` if the array is empty
cupsArrayGetLast(cups_array_t *a)	// I - Array
{
  // Range check input...
  if (!a || a->num_elements == 0)
    return (NULL);

  // Return the last element...
  return (cupsArrayGetElement(a, a->num_elements - 1));
}


//
// 'cupsArrayGetNext()' - Get the next element in an array.
//
// This function returns the next element in an array.  The next element is
// undefined until you call @link cupsArrayFind@, @link cupsArrayGetElement@,
// @link cupsArrayGetFirst@, or @link cupsArrayGetLast@ to set the current
// element.
//

void *					// O - Next element or @code NULL@
cupsArrayGetNext(cups_array_t *a)	// I - Array
{
  // Range check input...
  if (!a || a->num_elements == 0)
    return (NULL);
  else if (a->current == SIZE_MAX)
    return (cupsArrayGetElement(a, 0));
  else
    return (cupsArrayGetElement(a, a->current + 1));
}


//
// 'cupsArrayGetPrev()' - Get the previous element in an array.
//
// This function returns the previous element in an array.  The previous element
// is undefined until you call @link cupsArrayFind@, @link cupsArrayGetElement@,
// @link cupsArrayGetFirst@, or @link cupsArrayGetLast@ to set the current
// element.
//

void *					// O - Previous element or @code NULL@
cupsArrayGetPrev(cups_array_t *a)	// I - Array
{
  // Range check input...
  if (!a || a->num_elements == 0 || a->current == 0 || a->current == SIZE_MAX)
    return (NULL);
  else
    return (cupsArrayGetElement(a, a->current - 1));
}


//
// 'cupsArrayGetUserData()' - Return the user data for an array.
//

void *					// O - User data
cupsArrayGetUserData(cups_array_t *a)	// I - Array
{
  return (a ? a->data : NULL);
}


//
// 'cupsArrayInsert()' - Insert an element in an array.
//
// This function inserts an element in an array.  When inserting an element
// in a sorted array, non-unique elements are inserted at the beginning of the
// run of identical elements.  For unsorted arrays, the element is inserted at
// the beginning of the array.
//

bool					// O - `true` on success, `false` on failure
cupsArrayInsert(cups_array_t *a,	// I - Array
		void         *e)	// I - Element
{
  // Range check input...
  if (!a || !e)
    return (false);

  // Insert the element...
  return (cups_array_add(a, e, true));
}


//
// 'cupsArrayNew()' - Create a new array with callback functions.
//
// This function creates a new array with optional callback functions.  The
// comparison callback function ("f") is used to create a sorted array.  The
// function receives pointers to two elements and the user data pointer ("d").
// The user data pointer argument can safely be omitted when not required so
// functions like `strcmp` can be used for sorted string arrays.
//
// ```
// int // Return -1 if a < b, 0 if a == b, and 1 if a > b
// compare_cb(void *a, void *b, void *d)
// {
//   ... "a" and "b" are the elements, "d" is the user data pointer
// }
// ```
//
// The hash callback function ("hf") is used to implement cached lookups with
// the specified hash size ("hsize").  The function receives a pointer to an
// element and the user data pointer ("d") and returns an unsigned integer
// representing a hash into the array.  The hash value is of type `size_t` which
// provides at least 32-bits of resolution.
//
// ```
// size_t // Return hash value from 0 to (hashsize - 1)
// hash_cb(void *e, void *d)
// {
//   ... "e" is the element, "d" is the user data pointer
// }
// ```
//
// The copy callback function ("cf") is used to automatically copy/retain
// elements when added to the array or the array is copied with
// @link cupsArrayDup@.  The function receives a pointer to the element and the
// user data pointer ("d") and returns a new pointer that is stored in the array.
//
// ```
// void * // Return pointer to copied/retained element or NULL
// copy_cb(void *e, void *d)
// {
//   ... "e" is the element, "d" is the user data pointer
// }
// ```
//
// Finally, the free callback function ("cf") is used to automatically
// free/release elements when removed or the array is deleted.  The function
// receives a pointer to the element and the user data pointer ("d").
//
// ```
// void
// free_cb(void *e, void *d)
// {
//   ... "e" is the element, "d" is the user data pointer
// }
// ```
//

cups_array_t *				// O - Array
cupsArrayNew(cups_array_cb_t  f,	// I - Comparison callback function or `NULL` for an unsorted array
             void             *d,	// I - User data or `NULL`
             cups_ahash_cb_t  hf,	// I - Hash callback function or `NULL` for unhashed lookups
	     size_t           hsize,	// I - Hash size (>= `0`)
	     cups_acopy_cb_t  cf,	// I - Copy callback function or `NULL` for none
	     cups_afree_cb_t  ff)	// I - Free callback function or `NULL` for none
{
  cups_array_t	*a;			// Array


  // Allocate memory for the array...
  if ((a = calloc(1, sizeof(cups_array_t))) == NULL)
    return (NULL);

  a->compare   = f;
  a->data      = d;
  a->current   = SIZE_MAX;
  a->insert    = SIZE_MAX;
  a->num_saved = 0;
  a->unique    = true;

  if (hsize > 0 && hf)
  {
    a->hashfunc  = hf;
    a->hashsize  = hsize;
    a->hash      = malloc((size_t)hsize * sizeof(size_t));

    if (!a->hash)
    {
      free(a);
      return (NULL);
    }

    memset(a->hash, -1, (size_t)hsize * sizeof(size_t));
  }

  a->copyfunc = cf;
  a->freefunc = ff;

  return (a);
}


//
// 'cupsArrayNewStrings()' - Create a new array of delimited strings.
//
// This function creates an array that holds zero or more strings.  The created
// array automatically manages copies of the strings passed and sorts them in
// ascending order using a case-sensitive comparison.  If the string pointer "s"
// is `NULL` or the empty string, no strings are added to the newly created
// array.
//
// Additional strings can be added using the @link cupsArrayAdd@ or
// @link cupsArrayAddStrings@ functions.
//

cups_array_t *				// O - Array
cupsArrayNewStrings(const char *s,	// I - Delimited strings or `NULL` to create an empty array
                    char       delim)	// I - Delimiter character
{
  cups_array_t	*a;			// Array


  if ((a = cupsArrayNew(_cupsArrayStrcmp, NULL, NULL, 0, _cupsArrayStrdup, _cupsArrayFree)) != NULL)
    cupsArrayAddStrings(a, s, delim);

  return (a);
}


//
// 'cupsArrayRemove()' - Remove an element from an array.
//
// This function removes an element from an array.  If more than one element
// matches "e", only the first matching element is removed.
//

bool					// O - `true` on success, `false` on failure
cupsArrayRemove(cups_array_t *a,	// I - Array
                void         *e)	// I - Element
{
  size_t	i,			// Looping var
		current;		// Current element
  int		diff;			// Difference


  // Range check input...
  if (!a || a->num_elements == 0 || !e)
    return (false);

  // See if the element is in the array...
  current = cups_array_find(a, e, a->current, &diff);
  if (diff)
    return (false);

  // Yes, now remove it...
  a->num_elements --;

  if (a->freefunc)
    (a->freefunc)(a->elements[current], a->data);

  if (current < a->num_elements)
    memmove(a->elements + current, a->elements + current + 1, (a->num_elements - current) * sizeof(void *));

  if (current <= a->current)
  {
    if (a->current)
      a->current --;
    else
      a->current = SIZE_MAX;
  }

  if (current < a->insert)
    a->insert --;
  else if (current == a->insert)
    a->insert = SIZE_MAX;

  for (i = 0; i < a->num_saved; i ++)
  {
    if (current <= a->saved[i])
      a->saved[i] --;
  }

  if (a->num_elements <= 1)
    a->unique = true;

  return (true);
}


//
// 'cupsArrayRestore()' - Reset the current element to the last @link cupsArraySave@.
//

void *					// O - New current element
cupsArrayRestore(cups_array_t *a)	// I - Array
{
  // Range check input...
  if (!a || a->num_saved == 0)
    return (NULL);

  a->num_saved --;
  a->current = a->saved[a->num_saved];

  if (a->current < a->num_elements)
    return (a->elements[a->current]);
  else
    return (NULL);
}


//
// 'cupsArraySave()' - Mark the current element for a later @link cupsArrayRestore@.
//
// The current element is undefined until you call @link cupsArrayFind@,
// @link cupsArrayGetElement@, @link cupsArrayGetFirst@, or
// @link cupsArrayGetLast@ to set the current element.
//
// The save/restore stack is guaranteed to be at least 32 elements deep.
//

bool					// O - `true` on success, `false` on failure
cupsArraySave(cups_array_t *a)		// I - Array
{
  // Range check input...
  if (!a || a->num_saved >= _CUPS_MAXSAVE)
    return (false);

  a->saved[a->num_saved] = a->current;
  a->num_saved ++;

  return (true);
}


//
// '_cupsArrayStrcmp()' - Compare two strings in an array.
//

int					// O - Result of comparison
_cupsArrayStrcmp(void *s,		// I - first string to compare
	         void *t,		// I - second string to compare
                 void *data)		// I - Callback data (unused)
{
  (void)data;

  return (strcmp((const char *)s, (const char *)t));
}


//
// '_cupsArrayStrdup()' - Copy a string in an array.
//

void *					// O - Copy of string
_cupsArrayStrdup(void *s,		// I - String to copy
                 void *data)		// I - Callback data (unused)
{
  (void)data;

  return (strdup((const char *)s));
}


//
// 'cups_array_add()' - Insert or append an element to the array.
//

static bool				// O - `true` on success, `false` on failure
cups_array_add(cups_array_t *a,		// I - Array
               void         *e,		// I - Element to add
	       bool         insert)	// I - `true` = insert, `false` = append
{
  size_t	i,			// Looping var
		current;		// Current element
  int		diff;			// Comparison with current element


  // Verify we have room for the new element...
  if (a->num_elements >= a->alloc_elements)
  {
    // Allocate additional elements; start with 16 elements, then double the
    // size until 1024 elements, then add 1024 elements thereafter...
    void	**temp;			// New array elements
    size_t	count;			// New allocation count

    if (a->alloc_elements == 0)
      count = 16;
    else if (a->alloc_elements < 1024)
      count = a->alloc_elements * 2;
    else
      count = a->alloc_elements + 1024;

    if ((temp = realloc(a->elements, count * sizeof(void *))) == NULL)
      return (false);

    a->alloc_elements = count;
    a->elements       = temp;
  }

  // Find the insertion point for the new element; if there is no compare
  // function or elements, just add it to the beginning or end...
  if (!a->num_elements || !a->compare)
  {
    // No elements or comparison function, insert/append as needed...
    if (insert)
      current = 0;			// Insert at beginning
    else
      current = a->num_elements;	// Append to the end
  }
  else
  {
    // Do a binary search for the insertion point...
    current = cups_array_find(a, e, a->insert, &diff);

    if (diff > 0)
    {
      // Insert after the current element...
      current ++;
    }
    else if (!diff)
    {
      // Compared equal, make sure we add to the begining or end of the current
      // run of equal elements...
      a->unique = false;

      if (insert)
      {
        // Insert at beginning of run...
	while (current > 0 && !(*(a->compare))(e, a->elements[current - 1], a->data))
          current --;
      }
      else
      {
        // Append at end of run...
	do
	{
          current ++;
	}
	while (current < a->num_elements && !(*(a->compare))(e, a->elements[current], a->data));
      }
    }
  }

  // Insert or append the element...
  if (current < a->num_elements)
  {
    // Shift other elements to the right...
    memmove(a->elements + current + 1, a->elements + current, (a->num_elements - current) * sizeof(void *));

    if (a->current >= current)
      a->current ++;

    for (i = 0; i < a->num_saved; i ++)
    {
      if (a->saved[i] >= current)
	a->saved[i] ++;
    }
  }

  if (a->copyfunc)
  {
    if ((a->elements[current] = (a->copyfunc)(e, a->data)) == NULL)
      return (false);
  }
  else
  {
    a->elements[current] = e;
  }

  a->num_elements ++;
  a->insert = current;

  return (true);
}


//
// 'cups_array_find()' - Find an element in the array.
//

static size_t				// O - Index of match
cups_array_find(cups_array_t *a,	// I - Array
        	void         *e,	// I - Element
		size_t       prev,	// I - Previous index
		int          *rdiff)	// O - Difference of match
{
  size_t	left,			// Left side of search
		right,			// Right side of search
		current;		// Current element
  int		diff;			// Comparison with current element


  if (a->compare)
  {
    // Do a binary search for the element...
    if (prev < a->num_elements)
    {
      // Start search on either side of previous...
      if ((diff = (*(a->compare))(e, a->elements[prev], a->data)) == 0 || (diff < 0 && prev == 0) || (diff > 0 && prev == (a->num_elements - 1)))
      {
        // Exact or edge match, return it!
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
	right = a->num_elements - 1;
      }
    }
    else
    {
      // Start search in the middle...
      left  = 0;
      right = a->num_elements - 1;
    }

    do
    {
      current = (left + right) / 2;
      diff    = (*(a->compare))(e, a->elements[current], a->data);

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
      if ((diff = (*(a->compare))(e, a->elements[left], a->data)) <= 0)
      {
        current = left;
      }
      else
      {
        diff    = (*(a->compare))(e, a->elements[right], a->data);
        current = right;
      }
    }
  }
  else
  {
    // Do a linear pointer search...
    diff = 1;

    for (current = 0; current < a->num_elements; current ++)
    {
      if (a->elements[current] == e)
      {
        diff = 0;
        break;
      }
    }
  }

  // Return the closest element and the difference...
  *rdiff = diff;

  return (current);
}
