#
# Example files makefile for libcups.
#
# Copyright © 2021-2024 by OpenPrinting.
#
# Licensed under Apache License v2.0.  See the file "LICENSE" for more
# information.
#

#
# Include standard definitions...
#

include ../Makedefs


#
# Examples...
#

DATAFILES	=	\
			color.jpg \
			document-a4.pdf \
			document-a4.ps \
			document-letter.pdf \
			document-letter.ps \
			gray.jpg \
			onepage-a4-300-black-1.pwg \
			onepage-a4.pdf \
			onepage-a4.ps \
			onepage-letter-300-black-1.pwg \
			onepage-letter.pdf \
			onepage-letter.ps \
			testfile.jpg \
			testfile.pcl \
			testfile.pdf \
			testfile.ps \
			testfile.txt
TESTFILES	=	\
			cancel-current-job.test \
			create-job.test \
			create-job-sheets.test \
			create-job-timeout.test \
			create-printer-subscription.test \
			cups-create-local-printer.test \
			fax-job.test \
			get-completed-jobs.test \
			get-job-attributes.test \
			get-job-template-attributes.test \
			get-jobs.test \
			get-notifications.test \
			get-printer-attributes-suite.test \
			get-printer-attributes.test \
			get-printers.test \
			get-subscriptions.test \
			identify-printer-display.test \
			identify-printer.test \
			ipp-1.1.test \
			ipp-2.0.test \
			ipp-2.1.test \
			ipp-2.2.test \
			print-job.test \
			print-job-and-wait.test \
			print-job-deflate.test \
			print-job-gzip.test \
			print-job-hold.test \
			print-job-letter.test \
			print-job-manual.test \
			print-job-media-col.test \
			print-job-media-needed.test \
			print-job-password.test \
			print-job-raster.test \
			print-uri.test \
			pwg5100.1.test \
			pwg5100.2.test \
			pwg5100.3.test \
			pwg5100.5.test \
			pwg5100.6.test \
			pwg5100.7.test \
			pwg5100.8.test \
			pwg5100.9.test \
			pwg5100.11.test \
			pwg5101.1.test \
			rfc3380.test \
			rfc3995-3996.test \
			rfc3998.test \
			set-attrs-hold.test \
			validate-job.test \
			validate-resources.test


#
# Make everything...
#

all:


#
# Make library targets...
#

libs:


#
# Run unit tests...
#

test:


#
# Make unit tests...
#

unittests:


#
# Clean everything...
#

clean:


#
# Dummy depend...
#

depend:


#
# Install all targets...
#

install:
	echo "Installing sample ipptool files in $(BUILDROOT)$(CUPS_DATADIR)/ipptool..."
	$(INSTALL_DIR) $(BUILDROOT)$(CUPS_DATADIR)/ipptool
	for file in $(DATAFILES) $(TESTFILES); do \
		$(INSTALL_DATA) $$file $(BUILDROOT)$(CUPS_DATADIR)/ipptool; \
	done


#
# Uninstall files...
#

uninstall:
	echo "Uninstalling sample ipptool files from $(BUILDROOT)$(CUPS_DATADIR)/ipptool..."
	for file in $(DATAFILES) $(TESTFILES); do \
		$(RM) $(BUILDROOT)$(CUPS_DATADIR)/ipptool/$$file; \
	done
	-$(RMDIR) $(BUILDROOT)$(CUPS_DATADIR)/ipptool
