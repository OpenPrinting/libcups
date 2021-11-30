/*
 * Array test program for CUPS.
 *
 * Copyright © 2021 by OpenPrinting.
 * Copyright © 2007-2014 by Apple Inc.
 * Copyright © 1997-2006 by Easy Software Products.
 *
 * Licensed under Apache License v2.0.  See the file "LICENSE" for more
 * information.
 */

/*
 * Include necessary headers...
 */

#include "string-private.h"
#include "debug-private.h"
#include "array-private.h"
#include "dir.h"
#include "test-internal.h"


/*
 * Local functions...
 */

static double	get_seconds(void);
static int	load_words(const char *filename, cups_array_t *array);


/*
 * 'main()' - Main entry.
 */

int					/* O - Exit status */
main(void)
{
  int		i;			/* Looping var */
  cups_array_t	*array,			/* Test array */
		*dup_array;		/* Duplicate array */
  int		status;			/* Exit status */
  char		*text;			/* Text from array */
  char		word[256];		/* Word from file */
  double	start,			/* Start time */
		end;			/* End time */
  cups_dir_t	*dir;			/* Current directory */
  cups_dentry_t	*dent;			/* Directory entry */
  char		*saved[32];		/* Saved entries */
  void		*data;			/* User data for arrays */


 /*
  * No errors so far...
  */

  status = 0;

 /*
  * cupsArrayNew()
  */

  testBegin("cupsArrayNew3");

  data  = (void *)"testarray";
  array = cupsArrayNew3((cups_array_func_t)strcmp, data, NULL, 0, (cups_acopy_func_t)strdup, (cups_afree_func_t)free);

  if (array)
    testEnd(true);
  else
  {
    testEndMessage(false, "returned NULL, expected pointer");
    status ++;
  }

 /*
  * cupsArrayUserData()
  */

  testBegin("cupsArrayUserData");
  if (cupsArrayUserData(array) == data)
    testEnd(true);
  else
  {
    testEndMessage(false, "returned %p instead of %p", cupsArrayUserData(array), data);
    status ++;
  }

 /*
  * cupsArrayAdd()
  */

  testBegin("cupsArrayAdd");

  if (!cupsArrayAdd(array, "One Fish"))
  {
    testEndMessage(false, "\"One Fish\"");
    status ++;
  }
  else
  {
    if (!cupsArrayAdd(array, "Two Fish"))
    {
      testEndMessage(false, "\"Two Fish\"");
      status ++;
    }
    else
    {
      if (!cupsArrayAdd(array, "Red Fish"))
      {
	testEndMessage(false, "\"Red Fish\"");
	status ++;
      }
      else
      {
        if (!cupsArrayAdd(array, "Blue Fish"))
	{
	  testEndMessage(false, "\"Blue Fish\"");
	  status ++;
	}
	else
	  testEnd(true);
      }
    }
  }

 /*
  * cupsArrayCount()
  */

  testBegin("cupsArrayCount");
  if (cupsArrayCount(array) == 4)
    testEnd(true);
  else
  {
    testEndMessage(false, "returned %d, expected 4", cupsArrayCount(array));
    status ++;
  }

 /*
  * cupsArrayFirst()
  */

  testBegin("cupsArrayFirst");
  if ((text = (char *)cupsArrayFirst(array)) != NULL && !strcmp(text, "Blue Fish"))
    testEnd(true);
  else
  {
    testEndMessage(false, "returned \"%s\", expected \"Blue Fish\"", text);
    status ++;
  }

 /*
  * cupsArrayNext()
  */

  testBegin("cupsArrayNext");
  if ((text = (char *)cupsArrayNext(array)) != NULL && !strcmp(text, "One Fish"))
    testEnd(true);
  else
  {
    testEndMessage(false, "returned \"%s\", expected \"One Fish\"", text);
    status ++;
  }

 /*
  * cupsArrayLast()
  */

  testBegin("cupsArrayLast");
  if ((text = (char *)cupsArrayLast(array)) != NULL && !strcmp(text, "Two Fish"))
    testEnd(true);
  else
  {
    testEndMessage(false, "returned \"%s\", expected \"Two Fish\"", text);
    status ++;
  }

 /*
  * cupsArrayPrev()
  */

  testBegin("cupsArrayPrev");
  if ((text = (char *)cupsArrayPrev(array)) != NULL && !strcmp(text, "Red Fish"))
    testEnd(true);
  else
  {
    testEndMessage(false, "returned \"%s\", expected \"Red Fish\"", text);
    status ++;
  }

 /*
  * cupsArrayFind()
  */

  testBegin("cupsArrayFind");
  if ((text = (char *)cupsArrayFind(array, (void *)"One Fish")) != NULL && !strcmp(text, "One Fish"))
    testEnd(true);
  else
  {
    testEndMessage(false, "returned \"%s\", expected \"One Fish\"", text);
    status ++;
  }

 /*
  * cupsArrayCurrent()
  */

  testBegin("cupsArrayCurrent");
  if ((text = (char *)cupsArrayCurrent(array)) != NULL && !strcmp(text, "One Fish"))
    testEnd(true);
  else
  {
    testEndMessage(false, "returned \"%s\", expected \"One Fish\"", text);
    status ++;
  }

 /*
  * cupsArrayDup()
  */

  testBegin("cupsArrayDup");
  if ((dup_array = cupsArrayDup(array)) != NULL && cupsArrayCount(dup_array) == 4)
    testEnd(true);
  else
  {
    testEndMessage(false, "returned %p with %d elements, expected pointer with 4 elements", (void *)dup_array, cupsArrayCount(dup_array));
    status ++;
  }

 /*
  * cupsArrayRemove()
  */

  testBegin("cupsArrayRemove");
  if (cupsArrayRemove(array, (void *)"One Fish") && cupsArrayCount(array) == 3)
    testEnd(true);
  else
  {
    testEndMessage(false, "returned 0 with %d elements, expected 1 with 4 elements", cupsArrayCount(array));
    status ++;
  }

 /*
  * cupsArrayClear()
  */

  testBegin("cupsArrayClear");
  cupsArrayClear(array);
  if (cupsArrayCount(array) == 0)
    testEnd(true);
  else
  {
    testEndMessage(false, "%d elements, expected 0 elements", cupsArrayCount(array));
    status ++;
  }

 /*
  * Now load this source file and grab all of the unique words...
  */

  testBegin("Load unique words");

  start = get_seconds();

  if ((dir = cupsDirOpen(".")) == NULL)
  {
    testEndMessage(false, "cupsDirOpen failed");
    status ++;
  }
  else
  {
    bool load_status = true;		// Load status

    while ((dent = cupsDirRead(dir)) != NULL)
    {
      i = (int)strlen(dent->filename) - 2;

      if (i > 0 && dent->filename[i] == '.' && (dent->filename[i + 1] == 'c' || dent->filename[i + 1] == 'h'))
      {
	if (!load_words(dent->filename, array))
	{
	  load_status = false;
	  break;
	}
      }
    }

    cupsDirClose(dir);

    if (load_status)
    {
      end = get_seconds();

      for (text = (char *)cupsArrayFirst(array); text;)
      {
       /*
	* Copy this word to the word buffer (safe because we strdup'd from
	* the same buffer in the first place... :)
	*/

	strlcpy(word, text, sizeof(word));

       /*
	* Grab the next word and compare...
	*/

	if ((text = (char *)cupsArrayNext(array)) == NULL)
	  break;

	if (strcmp(word, text) >= 0)
	  break;
      }

      if (text)
      {
	testEndMessage(false, "\"%s\" >= \"%s\"", word, text);
	status ++;
      }
      else
	testEndMessage(true, "%d words in %.3f seconds - %.0f words/sec", cupsArrayCount(array), end - start, cupsArrayCount(array) / (end - start));
    }
  }

 /*
  * Test deleting with iteration...
  */

  testBegin("Delete While Iterating");

  text = (char *)cupsArrayFirst(array);
  cupsArrayRemove(array, text);

  text = (char *)cupsArrayNext(array);
  if (!text)
  {
    testEndMessage(false, "cupsArrayNext returned NULL");
    status ++;
  }
  else
    testEnd(true);

 /*
  * Test save/restore...
  */

  testBegin("cupsArraySave");

  for (i = 0, text = (char *)cupsArrayFirst(array); i < 32; i ++, text = (char *)cupsArrayNext(array))
  {
    saved[i] = text;

    if (!cupsArraySave(array))
      break;
  }

  if (i < 32)
    testEndMessage(false, "depth = %d", i);
  else
    testEnd(true);

  testBegin("cupsArrayRestore");

  while (i > 0)
  {
    i --;

    text = cupsArrayRestore(array);
    if (text != saved[i])
      break;
  }

  if (i)
    testEndMessage(false, "depth = %d", i);
  else
    testEnd(true);

 /*
  * Delete the arrays...
  */

  cupsArrayDelete(array);
  cupsArrayDelete(dup_array);

 /*
  * Test the array with string functions...
  */

  testBegin("_cupsArrayNewStrings(\" \\t\\nfoo bar\\tboo\\nfar\", ' ')");
  array = _cupsArrayNewStrings(" \t\nfoo bar\tboo\nfar", ' ');
  if (!array)
  {
    status = 1;
    testEndMessage(false, "unable to create array");
  }
  else if (cupsArrayCount(array) != 4)
  {
    status = 1;
    testEndMessage(false, "got %d elements, expected 4", cupsArrayCount(array));
  }
  else if (strcmp(text = (char *)cupsArrayFirst(array), "bar"))
  {
    status = 1;
    testEndMessage(false, "first element \"%s\", expected \"bar\"", text);
  }
  else if (strcmp(text = (char *)cupsArrayNext(array), "boo"))
  {
    status = 1;
    testEndMessage(false, "first element \"%s\", expected \"boo\"", text);
  }
  else if (strcmp(text = (char *)cupsArrayNext(array), "far"))
  {
    status = 1;
    testEndMessage(false, "first element \"%s\", expected \"far\"", text);
  }
  else if (strcmp(text = (char *)cupsArrayNext(array), "foo"))
  {
    status = 1;
    testEndMessage(false, "first element \"%s\", expected \"foo\"", text);
  }
  else
    testEnd(true);

  testBegin("_cupsArrayAddStrings(array, \"foo2,bar2\", ',')");
  _cupsArrayAddStrings(array, "foo2,bar2", ',');

  if (cupsArrayCount(array) != 6)
  {
    status = 1;
    testEndMessage(false, "got %d elements, expected 6", cupsArrayCount(array));
  }
  else if (strcmp(text = (char *)cupsArrayFirst(array), "bar"))
  {
    status = 1;
    testEndMessage(false, "first element \"%s\", expected \"bar\"", text);
  }
  else if (strcmp(text = (char *)cupsArrayNext(array), "bar2"))
  {
    status = 1;
    testEndMessage(false, "first element \"%s\", expected \"bar2\"", text);
  }
  else if (strcmp(text = (char *)cupsArrayNext(array), "boo"))
  {
    status = 1;
    testEndMessage(false, "first element \"%s\", expected \"boo\"", text);
  }
  else if (strcmp(text = (char *)cupsArrayNext(array), "far"))
  {
    status = 1;
    testEndMessage(false, "first element \"%s\", expected \"far\"", text);
  }
  else if (strcmp(text = (char *)cupsArrayNext(array), "foo"))
  {
    status = 1;
    testEndMessage(false, "first element \"%s\", expected \"foo\"", text);
  }
  else if (strcmp(text = (char *)cupsArrayNext(array), "foo2"))
  {
    status = 1;
    testEndMessage(false, "first element \"%s\", expected \"foo2\"", text);
  }
  else
    testEnd(true);

  cupsArrayDelete(array);

  return (status);
}


/*
 * 'get_seconds()' - Get the current time in seconds...
 */

#ifdef _WIN32
#  include <windows.h>


static double
get_seconds(void)
{
}
#else
#  include <sys/time.h>


static double
get_seconds(void)
{
  struct timeval	curtime;	/* Current time */


  gettimeofday(&curtime, NULL);
  return (curtime.tv_sec + 0.000001 * curtime.tv_usec);
}
#endif /* _WIN32 */


/*
 * 'load_words()' - Load words from a file.
 */

static int				/* O - 1 on success, 0 on failure */
load_words(const char   *filename,	/* I - File to load */
           cups_array_t *array)		/* I - Array to add to */
{
  FILE		*fp;			/* Test file */
  char		word[256];		/* Word from file */


  testProgress();

  if ((fp = fopen(filename, "r")) == NULL)
  {
    testEndMessage(false, "%s: %s", filename, strerror(errno));
    return (0);
  }

  while (fscanf(fp, "%255s", word) == 1)
  {
    if (!cupsArrayFind(array, word))
      cupsArrayAdd(array, word);
  }

  fclose(fp);

  return (1);
}
