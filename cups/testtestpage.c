//
// Raster test page generator unit test for CUPS.
//
// Copyright © 2020-2022 by OpenPrinting
// Copyright © 2017-2019 by Apple Inc.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#include "raster-testpage.h"
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>


//
// Local functions...
//

static void	usage(void);


//
// 'main()' - Generate a test raster file.
//

int					// O - Exit status
main(int  argc,				// I - Number of command-line arguments
     char *argv[])			// I - Command-line arguments
{
  int			i;		// Looping var
  const char		*opt;		// Current option
  const char		*filename = NULL,
  					// Output filename
			*media_name = "na_letter_8.5x11in",
					// Media size name
			*sheet_back = "normal",
					// Back side transform
			*sides = "one-sided",
					// Duplex mode
			*type = "srgb_8";
					// Output type
  int			xres = 300,	// Horizontal resolution
			yres = 300,	// Vertical resolution
			num_copies = 1,	// Number of copies
			num_pages = 2;	// Number of pages
  ipp_orient_t		orientation = IPP_ORIENT_PORTRAIT;
					// Output orientation
  int			fd;		// File descriptor
  cups_raster_t		*ras;		// Raster stream
  cups_page_header_t	header;		// Page header
  pwg_media_t		*media;		// Media information
  static const char * const sheet_backs[4] =
  {					// Back side values
    "normal",
    "flipped",
    "manual-tumble",
    "rotated"
  };


  // Parse command-line options...
  if (argc > 1)
  {
    for (i = 1; i < argc; i ++)
    {
      if (!strcmp(argv[i], "--help"))
      {
        usage();
        return (0);
      }
      else if (!strncmp(argv[i], "--", 2))
      {
        fprintf(stderr, "testtestpage: Unknown option '%s'.\n", argv[i]);
        usage();
        return (1);
      }
      else if (argv[i][0] == '-')
      {
        for (opt = argv[i] + 1; *opt; opt ++)
        {
          switch (*opt)
          {
            case 'b' : // -b SHEET-BACK
                i ++;
                if (i >= argc)
                {
                  fputs("testtestpage: Expected sheet-back after '-b'.\n", stderr);
                  usage();
                  return (1);
                }

                if (strcmp(argv[i], "normal") && strcmp(argv[i], "flip") && strcmp(argv[i], "rotate") && strcmp(argv[i], "manual-tumble"))
                {
                  fprintf(stderr, "testtestpage: Unexpected sheet-back '-b %s'.\n", argv[i]);
                  usage();
                  return (1);
                }

                sheet_back = argv[i];
                break;

            case 'c' : // -c NUM-COPIES
                i ++;
                if (i >= argc || !isdigit(argv[i][0] & 255))
                {
                  fputs("testtestpage: Expected number of copies after '-c'.\n", stderr);
                  usage();
                  return (1);
                }

                num_copies = atoi(argv[i]);
                break;

            case 'm' : // -m MEDIA-NAME
                i ++;
                if (i >= argc)
                {
                  fputs("testtestpage: Expected media size name after '-m'.\n", stderr);
                  usage();
                  return (1);
                }

                media_name = argv[i];
                break;

            case 'n' : // -n NUM-PAGES
                i ++;
                if (i >= argc || !isdigit(argv[i][0] & 255))
                {
                  fputs("testtestpage: Expected number of pages after '-p'.\n", stderr);
                  usage();
                  return (1);
                }

                num_pages = atoi(argv[i]);
                break;

            case 'o' : // -o ORIENTATION
                i ++;
                if (i >= argc)
                {
                  fputs("testtestpage: Expected orientation after '-o'.\n", stderr);
                  usage();
                  return (1);
                }

                if (!strcmp(argv[i], "portrait"))
                  orientation = IPP_ORIENT_PORTRAIT;
                else if (!strcmp(argv[i], "landscape"))
                  orientation = IPP_ORIENT_LANDSCAPE;
                else if (!strcmp(argv[i], "reverse-portrait"))
                  orientation = IPP_ORIENT_REVERSE_PORTRAIT;
                else if (!strcmp(argv[i], "reverse-landscape"))
                  orientation = IPP_ORIENT_REVERSE_LANDSCAPE;
                else
                {
                  fprintf(stderr, "testtestpage: Unexpected orientation '-o %s'.\n", argv[i]);
                  usage();
                  return (1);
                }
                break;

            case 'r' : // -r RES or -r XRESxYRES
                i ++;
                if (i >= argc)
                {
                  fputs("testtestpage: Expected resolution after '-r'.\n", stderr);
                  usage();
                  return (1);
                }

                switch (sscanf(argv[i], "%dx%d", &xres, &yres))
                {
                  case 0 :
                      fprintf(stderr, "testtestpage: Unexpected resolution '-r %s'.\n", argv[i]);
		      usage();
		      return (1);
		  case 1 :
		      yres = xres;
		      break;
                }
                break;

            case 's' : // -s SIDES
                i ++;
                if (i >= argc)
                {
                  fputs("testtestpage: Expected sides after '-s'.\n", stderr);
                  usage();
                  return (1);
                }

                if (strcmp(argv[i], "one-sided") && strcmp(argv[i], "two-sided-long-edge") && strcmp(argv[i], "two-sided-short-edge"))
                {
                  fprintf(stderr, "testtestpage: Unexpected sides '-s %s'.\n", argv[i]);
                  usage();
                  return (1);
                }

                sides = argv[i];
                break;

            case 't' : // -t TYPE
                i ++;
                if (i >= argc)
                {
                  fputs("testtestpage: Expected type after '-t'.\n", stderr);
                  usage();
                  return (1);
                }

		if (!strcmp(argv[i], "color"))
		  type = "srgb_8";
		else if (!strcmp(argv[i], "gray"))
		  type = "sgray_8";
                else if (strcmp(argv[i], "black_1") && strcmp(argv[i], "black_8") && strcmp(argv[i], "black_16") && strcmp(argv[i], "cmyk_8") && strcmp(argv[i], "cmyk_16") && strcmp(argv[i], "sgray_8") && strcmp(argv[i], "sgray_16") && strcmp(argv[i], "srgb_8") && strcmp(argv[i], "srgb_16"))
                {
                  fprintf(stderr, "testtestpage: Unexpected sheet-back '-b %s'.\n", argv[i]);
                  usage();
                  return (1);
                }
                else
                  type = argv[i];
                break;

            default :
                fprintf(stderr, "testtestpage: Unknown option '-%c'.\n", *opt);
                usage();
                return (1);
          }
        }
      }
      else if (!filename)
      {
        filename = argv[i];
      }
      else
      {
        fprintf(stderr, "testtestpage: Unknown option '%s'.\n", argv[i]);
        usage();
        return (1);
      }

      if ((media = pwgMediaForPWG(media_name)) == NULL)
      {
        fprintf(stderr, "testtestpage: Unable to lookup media '%s'.\n", media_name);
        return (1);
      }

      if (filename)
      {
        if ((fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0666)) < 0)
        {
          fprintf(stderr, "testtestpage: Unable to create '%s': %s\n", filename, strerror(errno));
          return (1);
        }
      }
      else
        fd = 1;

      if ((ras = cupsRasterOpen(fd, CUPS_RASTER_WRITE_PWG)) == NULL)
      {
	fprintf(stderr, "testtestpage: Unable to open raster stream for '%s': %s\n", filename ? filename : "(stdout)", cupsLastErrorString());
	close(fd);
	return (1);
      }

      cupsRasterInitPWGHeader(&header, media, type, xres, yres, sides, sheet_back);
      cupsRasterWriteTest(ras, &header, sheet_back, orientation, num_copies, num_pages);
      cupsRasterClose(ras);
    }
  }
  else
  {
    // Do unit tests...
    if ((fd = open("test.ras", O_WRONLY | O_CREAT | O_TRUNC, 0666)) < 0)
    {
      perror("test.ras");
      return (1);
    }

    if ((ras = cupsRasterOpen(fd, CUPS_RASTER_WRITE_PWG)) == NULL)
    {
      fprintf(stderr, "test.ras: %s\n", cupsLastErrorString());
      close(fd);
      return (1);
    }

    media = pwgMediaForPWG("na_letter_8.5x11in");

    for (orientation = IPP_ORIENT_PORTRAIT; orientation <= IPP_ORIENT_REVERSE_PORTRAIT; orientation ++)
    {
      cupsRasterInitPWGHeader(&header, media, "black_1", 300, 300, "one-sided", "normal");
      if (orientation == IPP_ORIENT_PORTRAIT)
	cupsRasterWriteTest(ras, &header, "normal", orientation, 2, 3);
      else
	cupsRasterWriteTest(ras, &header, "normal", orientation, 1, 1);

      cupsRasterInitPWGHeader(&header, media, "black_8", 300, 300, "one-sided", "normal");
      cupsRasterWriteTest(ras, &header, "normal", orientation, 1, 1);

      cupsRasterInitPWGHeader(&header, media, "black_16", 300, 300, "one-sided", "normal");
      cupsRasterWriteTest(ras, &header, "normal", orientation, 1, 1);

      cupsRasterInitPWGHeader(&header, media, "srgb_8", 300, 300, "one-sided", "normal");
      cupsRasterWriteTest(ras, &header, "normal", orientation, 1, 1);

      cupsRasterInitPWGHeader(&header, media, "srgb_16", 300, 300, "one-sided", "normal");
      cupsRasterWriteTest(ras, &header, "normal", orientation, 1, 1);

      cupsRasterInitPWGHeader(&header, media, "sgray_8", 300, 300, "one-sided", "normal");
      cupsRasterWriteTest(ras, &header, "normal", orientation, 1, 1);

      cupsRasterInitPWGHeader(&header, media, "sgray_8", 300, 300, "one-sided", "normal");
      cupsRasterWriteTest(ras, &header, "normal", orientation, 1, 1);

      cupsRasterInitPWGHeader(&header, media, "cmyk_8", 300, 300, "one-sided", "normal");
      cupsRasterWriteTest(ras, &header, "normal", orientation, 1, 1);

      cupsRasterInitPWGHeader(&header, media, "cmyk_16", 300, 300, "one-sided", "normal");
      cupsRasterWriteTest(ras, &header, "normal", orientation, 1, 1);
    }

    for (i = 0; i < 4; i ++)
    {
      for (orientation = IPP_ORIENT_PORTRAIT; orientation <= IPP_ORIENT_REVERSE_PORTRAIT; orientation ++)
      {
	cupsRasterInitPWGHeader(&header, media, "black_1", 300, 300, "two-sided-long-edge", "normal");
	cupsRasterWriteTest(ras, &header, sheet_backs[i], orientation, 1, 2);

	cupsRasterInitPWGHeader(&header, media, "black_8", 300, 300, "two-sided-long-edge", sheet_backs[i]);
	cupsRasterWriteTest(ras, &header, sheet_backs[i], orientation, 1, 2);

	cupsRasterInitPWGHeader(&header, media, "srgb_8", 300, 300, "two-sided-long-edge", sheet_backs[i]);
	cupsRasterWriteTest(ras, &header, sheet_backs[i], orientation, 1, 2);

	cupsRasterInitPWGHeader(&header, media, "cmyk_8", 300, 300, "two-sided-long-edge", sheet_backs[i]);
	cupsRasterWriteTest(ras, &header, sheet_backs[i], orientation, 1, 2);
      }
    }
    cupsRasterClose(ras);
  }

  return (0);
}


//
// 'usage()' - Show program usage.
//

static void
usage(void)
{
  puts("Usage: ./testtestpage [OPTIONS] [FILENAME]");
  puts("Options:");
  puts("-b SHEET-BACK       Specify the back side transform for duplex output (normal, flip, manual-tumble, or rotate)");
  puts("-c NUM-COPIES       Specify the number of copies (default 1)");
  puts("-m MEDIA-SIZE-NAME  Specify the PWG media size name (default 'na_letter_8.5x11in')");
  puts("-n NUM-PAGES        Specify the number of pages (default 2)");
  puts("-o ORIENTATION      Specify the orientation (portrait, landscape, reverse-landscape, reverse-portrait)");
  puts("-r RESOLUTION       Specify the output resolution (NNN or NNNxNNN)");
  puts("-t TYPE             Specify the output color space and bit depth");
}
