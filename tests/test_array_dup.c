#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cups/array.h"

// Simple hash: sum of bytes modulo size
static size_t
simple_hash(void *element, void *data)
{
  const char *s = (const char *)element;
  size_t sum = 0;

  (void)data;

  for (; *s; s++)
    sum += (unsigned char)*s;

  return (sum % ((size_t)data));
}

int main(void)
{
  const char *values[] = { "apple", "banana", "cherry", "date", "elderberry", NULL };
  cups_array_t *a, *d;
  size_t i;

  // Create an array with hashing enabled (hash size 16). Use strdup/free for copy/free.
  a = cupsArrayNew((cups_array_cb_t)strcmp, NULL, (cups_ahash_cb_t)simple_hash, 16, (cups_acopy_cb_t)strdup, (cups_afree_cb_t)free);
  if (!a)
  {
    fprintf(stderr, "Failed to create array\n");
    return 2;
  }

  for (i = 0; values[i]; i++)
  {
    if (!cupsArrayAdd(a, (void *)values[i]))
    {
      fprintf(stderr, "Failed to add %s\n", values[i]);
      cupsArrayDelete(a);
      return 3;
    }
  }

  // Duplicate the array
  d = cupsArrayDup(a);
  if (!d)
  {
    fprintf(stderr, "cupsArrayDup() failed\n");
    cupsArrayDelete(a);
    return 4;
  }

  // Verify all values can be found in the duplicate
  for (i = 0; values[i]; i++)
  {
    void *found = cupsArrayFind(d, (void *)values[i]);
    if (!found)
    {
      fprintf(stderr, "Value '%s' not found in duplicated array\n", values[i]);
      cupsArrayDelete(a);
      cupsArrayDelete(d);
      return 5;
    }
    else
      printf("Found '%s' in duplicate\n", (char *)found);
  }

  cupsArrayDelete(a);
  cupsArrayDelete(d);

  // Regression test for integer overflow protection in array allocation.
  {
    cups_array_t *bad = calloc(1, sizeof(cups_array_t));
    size_t bad_count = SIZE_MAX / sizeof(void *) + 1;

    if (!bad)
    {
      fprintf(stderr, "Failed to allocate array skeleton for overflow test\n");
      return 6;
    }

    bad->num_elements = bad_count;
    bad->elements     = NULL;
    bad->copyfunc     = NULL;
    bad->freefunc     = NULL;

    if (cupsArrayDup(bad) != NULL)
    {
      fprintf(stderr, "cupsArrayDup() did not reject overflow count\n");
      free(bad);
      return 7;
    }

    free(bad);
  }

  printf("Test passed\n");
  return 0;
}
