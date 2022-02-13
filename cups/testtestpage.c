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
#include <fcntl.h>
#include <unistd.h>


//
// 'main()' - Generate a test raster file.
//

int					// O - Exit status
main(int  argc,				// I - Number of command-line arguments
     char *argv[])			// I - Command-line arguments
{
  int			fd;		// File descriptor
  cups_raster_t		*ras;		// Raster stream
  cups_page_header_t	header;		// Page header
  pwg_media_t		*media;		// Media information


  (void)argc;
  (void)argv;

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

  cupsRasterInitPWGHeader(&header, media, "black_1", 300, 300, "two-sided-long-edge", "normal");
  cupsRasterWriteTest(ras, &header, "normal", IPP_ORIENT_PORTRAIT, 2, 2);

  cupsRasterInitPWGHeader(&header, media, "black_8", 300, 300, "two-sided-long-edge", "normal");
  cupsRasterWriteTest(ras, &header, "normal", IPP_ORIENT_PORTRAIT, 2, 2);

  cupsRasterInitPWGHeader(&header, media, "black_16", 300, 300, "two-sided-long-edge", "normal");
  cupsRasterWriteTest(ras, &header, "normal", IPP_ORIENT_PORTRAIT, 2, 2);

  cupsRasterInitPWGHeader(&header, media, "srgb_8", 300, 300, "two-sided-long-edge", "normal");
  cupsRasterWriteTest(ras, &header, "normal", IPP_ORIENT_PORTRAIT, 2, 2);

  cupsRasterInitPWGHeader(&header, media, "srgb_16", 300, 300, "two-sided-long-edge", "normal");
  cupsRasterWriteTest(ras, &header, "normal", IPP_ORIENT_PORTRAIT, 2, 2);

  cupsRasterInitPWGHeader(&header, media, "sgray_8", 300, 300, "two-sided-long-edge", "normal");
  cupsRasterWriteTest(ras, &header, "normal", IPP_ORIENT_PORTRAIT, 2, 2);

  cupsRasterInitPWGHeader(&header, media, "sgray_8", 300, 300, "two-sided-long-edge", "normal");
  cupsRasterWriteTest(ras, &header, "normal", IPP_ORIENT_PORTRAIT, 2, 2);

  cupsRasterInitPWGHeader(&header, media, "cmyk_8", 300, 300, "two-sided-long-edge", "normal");
  cupsRasterWriteTest(ras, &header, "normal", IPP_ORIENT_PORTRAIT, 2, 2);

  cupsRasterInitPWGHeader(&header, media, "cmyk_16", 300, 300, "two-sided-long-edge", "normal");
  cupsRasterWriteTest(ras, &header, "normal", IPP_ORIENT_PORTRAIT, 2, 2);

  cupsRasterClose(ras);
}
