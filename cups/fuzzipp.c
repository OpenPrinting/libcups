/*
 * IPP fuzzing program for CUPS.
 *
 * Copyright © 2021 by OpenPrinting.
 * Copyright © 2007-2021 by Apple Inc.
 * Copyright © 1997-2005 by Easy Software Products.
 *
 * Licensed under Apache License v2.0.  See the file "LICENSE" for more
 * information.
 */

/*
 * Include necessary headers...
 */

#include "file.h"
#include "string-private.h"
#include "ipp-private.h"
#include "test-internal.h"
#include <spawn.h>
#include <sys/wait.h>
#ifdef _WIN32
#  include <io.h>
#else
#  include <unistd.h>
#  include <fcntl.h>
#endif /* _WIN32 */


//
// Local constants...
//

#define NUM_FUZZ	10		// Number of fuzzers to run in parallel


/*
 * Local types...
 */

typedef struct _ippdata_t		// Data
{
  size_t	wused,			// Bytes used
		wsize;			// Max size of buffer
  ipp_uchar_t	*wbuffer;		// Buffer
} _ippdata_t;


/*
 * Local functions...
 */

void	fuzz_data(_ippdata_t *data);
int	wait_child(int pid, const char *filename);
ssize_t	write_cb(_ippdata_t *data, ipp_uchar_t *buffer, size_t bytes);


/*
 * 'main()' - Main entry.
 */

int				// O - Exit status
main(int  argc,			// I - Number of command-line arguments
     char *argv[])		// I - Command-line arguments
{
  int		status = 0;	// Exit status
  ipp_state_t	state;		// State
  cups_file_t	*fp;		// File pointer
  ipp_t		*request;	// Request


  if (argc == 1)
  {
    // Generate a Print-Job request with all common attribute types...
    int		i;		// Looping var
    const char	*tmpdir;	// Temporary directory
    char	filenames[NUM_FUZZ][256];
				// Test filenames
    const char	*fuzz_args[3];	// Arguments for sub-process
    int		num_fuzz_pids;	// Number of sub-processes
    pid_t	fuzz_pids[NUM_FUZZ];
				// Sub-process IDs
    ipp_t	*media_col,	// media-col collection
		*media_size;	// media-size collection
    _ippdata_t	data;		// IPP buffer
    ipp_uchar_t	buffer[262144];	// Write buffer data

    request = ippNewRequest(IPP_OP_PRINT_JOB);

    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri", NULL, "ipp://localhost/printers/foo");
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name", NULL, "john-doe");
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "job-name", NULL, "Test Job");
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_MIMETYPE, "document-format", NULL, "application/pdf");
    ippAddOctetString(request, IPP_TAG_OPERATION, "job-password", "8675309", 7);
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD, "job-password-encryption", NULL, "none");
    ippAddString(request, IPP_TAG_JOB, IPP_TAG_KEYWORD, "print-color-mode", NULL, "color");
    ippAddInteger(request, IPP_TAG_JOB, IPP_TAG_ENUM, "print-quality", IPP_QUALITY_HIGH);
    ippAddResolution(request, IPP_TAG_JOB, "printer-resolution", 1200, 1200, IPP_RES_PER_INCH);
    ippAddInteger(request, IPP_TAG_JOB, IPP_TAG_INTEGER, "copies", 42);
    ippAddBoolean(request, IPP_TAG_JOB, "some-boolean-option", 1);
    ippAddString(request, IPP_TAG_JOB, IPP_TAG_URISCHEME, "some-uri-scheme", NULL, "mailto");
    ippAddString(request, IPP_TAG_JOB, IPP_TAG_NAMELANG, "some-name-with-language", "es-MX", "Jose");
    ippAddString(request, IPP_TAG_JOB, IPP_TAG_TEXTLANG, "some-text-with-language", "es-MX", "¡Hola el mundo!");
    ippAddRange(request, IPP_TAG_JOB, "page-ranges", 1, 50);
    ippAddDate(request, IPP_TAG_JOB, "job-hold-until-time", ippTimeToDate(time(NULL) + 3600));
    ippAddString(request, IPP_TAG_JOB, IPP_TAG_TEXT, "job-message-to-operator", NULL, "This is a test job.");

    media_col  = ippNew();
    media_size = ippNew();
    ippAddInteger(media_size, IPP_TAG_ZERO, IPP_TAG_INTEGER, "x-dimension", 21590);
    ippAddInteger(media_size, IPP_TAG_ZERO, IPP_TAG_INTEGER, "y-dimension", 27940);
    ippAddCollection(media_col, IPP_TAG_JOB, "media-size", media_size);
    ippDelete(media_size);
    ippAddString(media_col, IPP_TAG_JOB, IPP_TAG_KEYWORD, "media-color", NULL, "blue");
    ippAddString(media_col, IPP_TAG_JOB, IPP_TAG_KEYWORD, "media-type", NULL, "stationery");

    ippAddCollection(request, IPP_TAG_JOB, "media-col", media_col);
    ippDelete(media_col);

    data.wused   = 0;
    data.wsize   = sizeof(buffer);
    data.wbuffer = buffer;

    while ((state = ippWriteIO(&data, (ipp_iocb_t)write_cb, 1, NULL, request)) != IPP_STATE_DATA)
    {
      if (state == IPP_STATE_ERROR)
	break;
    }

    testBegin("ippReadIO/ippWriteIO");

    if (state != IPP_STATE_DATA)
    {
      testEndMessage(false, "Failed to create base IPP message.");
      return (1);
    }

    ippDelete(request);

    // Find the temporary directory...
    if ((tmpdir = getenv("TMPDIR")) == NULL)
    {
#ifdef __APPLE__
      tmpdir = "/private/tmp";
#else
      tmpdir = "/tmp";
#endif // __APPLE__
    }

    // Now iterate 10000 times and test the fuzzed request...
    for (i = 0, num_fuzz_pids = 0; i < 10000; i ++)
    {
      fuzz_data(&data);

      snprintf(filenames[num_fuzz_pids], sizeof(filenames[0]), "%s/fuzz-%03d.ipp", tmpdir, i);
      if ((fp = cupsFileOpen(filenames[num_fuzz_pids], "w")) == NULL)
      {
        testEndMessage(false, "%s: %s", filenames[num_fuzz_pids], strerror(errno));
        status = 1;
        break;
      }

      cupsFileWrite(fp, (char *)buffer, data.wused);
      cupsFileClose(fp);

      fuzz_args[0] = argv[0];
      fuzz_args[1] = filenames[num_fuzz_pids];
      fuzz_args[2] = NULL;

      if (posix_spawn(fuzz_pids + num_fuzz_pids, argv[0], NULL, NULL, (char * const *)fuzz_args, NULL))
      {
        testEndMessage(false, "Unable to run '%s %s': %s", argv[0], filenames[num_fuzz_pids], strerror(errno));
        status = 1;
        break;
      }

      num_fuzz_pids ++;
      if (num_fuzz_pids >= NUM_FUZZ)
      {
        testProgress();

        while (num_fuzz_pids > 0)
        {
          num_fuzz_pids --;
          if (wait_child(fuzz_pids[num_fuzz_pids], filenames[num_fuzz_pids]))
            status = 1;
        }
      }
    }

    while (num_fuzz_pids > 0)
    {
      num_fuzz_pids --;
      if (wait_child(fuzz_pids[num_fuzz_pids], filenames[num_fuzz_pids]))
	status = 1;
    }

    if (!status)
      testEnd(true);
  }
  else
  {
    // Read an IPP file...
    cups_file_t	*fp;		// File pointer

    if ((fp = cupsFileOpen(argv[1], "r")) == NULL)
    {
      perror(argv[1]);
      return (1);
    }

    request = ippNew();
    do
    {
      state = ippReadIO(fp, (ipp_iocb_t)cupsFileRead, 1, NULL, request);
    }
    while (state == IPP_STATE_ATTRIBUTE);

    cupsFileClose(fp);

    fp = cupsFileOpen("/dev/null", "w");

    ippSetState(request, IPP_STATE_IDLE);

    do
    {
      state = ippWriteIO(fp, (ipp_iocb_t)cupsFileWrite, 1, NULL, request);
    }
    while (state == IPP_STATE_ATTRIBUTE);

    cupsFileClose(fp);
    ippDelete(request);
  }

  return (status);
}


//
// 'fuzz_data()' - Mutate a buffer for fuzzing purposes...
//

void
fuzz_data(_ippdata_t *data)		// I - Data buffer
{
  int		i,			// Looping vars
		pos,			// Position in buffer
		pos2,			// Second position in buffer
		len;			// Number of bytes
  ipp_uchar_t	temp[16];		// Temporary buffer


  // Mutate a few times...
  for (i = 0; i < 32; i ++)
  {
    // Each cycle replace or swap bytes
    switch ((len = CUPS_RAND() & 7))
    {
      case 0 :
          if (data->wused < data->wsize)
          {
            // Insert bytes
	    len  = (CUPS_RAND() & 7) + 1;
	    if (len > (int)(data->wsize - data->wused))
	      len = (int)(data->wsize - data->wused);

	    pos = CUPS_RAND() % (data->wused - len);
	    memmove(data->wbuffer + pos + len, data->wbuffer + pos, data->wused - pos);
	    for (i = 0; i < len; i ++)
	      data->wbuffer[pos + i] = CUPS_RAND();
	    break;
	  }

      case 1 :
      case 2 :
      case 3 :
      case 4 :
      case 5 :
      case 6 :
          // Replace bytes
          len ++;
          pos = CUPS_RAND() % (data->wused - len);
          while (len > 0)
          {
            data->wbuffer[pos ++] = CUPS_RAND();
            len --;
          }
          break;

      case 7 :
          // Swap bytes
          len  = (CUPS_RAND() & 7) + 1;
          pos  = CUPS_RAND() % (data->wused - len);
          pos2 = CUPS_RAND() % (data->wused - len);
          memmove(temp, data->wbuffer + pos, len);
          memmove(data->wbuffer + pos, data->wbuffer + pos2, len);
          memmove(data->wbuffer + pos2, temp, len);
          break;
    }
  }
}


//
// 'wait_child()' - Wait for a child process to finish and show any errors as
//                  needed.
//

int					// O - Exit status
wait_child(int        pid,		// I - Child process ID
           const char *filename)	// I - Test data
{
  int	status;				// Exit status


  while (waitpid(pid, &status, 0) < 0)
  {
    if (errno != EINTR && errno != EAGAIN)
    {
      printf("FAIL (%s: %s)\n", filename, strerror(errno));
      unlink(filename);
      return (1);
    }
  }

  if (status)
  {
    cups_file_t	*fp;			// File
    unsigned char buffer[262144];	// Data buffer
    ssize_t	bytes;			// Bytes read

    printf("FAIL (%s: %d)\n", filename, status);
    if ((fp = cupsFileOpen(filename, "r")) != NULL)
    {
      if ((bytes = cupsFileRead(fp, (char *)buffer, sizeof(buffer))) > 0)
        testHexDump(buffer, (size_t)bytes);

      cupsFileClose(fp);
    }
  }

  unlink(filename);

  return (status != 0);
}


/*
 * 'write_cb()' - Write data into a buffer.
 */

ssize_t					/* O - Number of bytes written */
write_cb(_ippdata_t   *data,		/* I - Data */
         ipp_uchar_t *buffer,		/* I - Buffer to write */
	 size_t      bytes)		/* I - Number of bytes to write */
{
  size_t	count;			/* Number of bytes */


 /*
  * Loop until all bytes are written...
  */

  if ((count = data->wsize - data->wused) > bytes)
    count = bytes;

  memcpy(data->wbuffer + data->wused, buffer, count);
  data->wused += count;

 /*
  * Return the number of bytes written...
  */

  return ((ssize_t)count);
}
