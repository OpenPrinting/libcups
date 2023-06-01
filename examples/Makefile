#
# Example files makefile for libcups.
#
# Copyright © 2021-2023 by OpenPrinting.
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
			onepage-a4.pdf \
			onepage-a4.ps \
			onepage-letter.pdf \
			onepage-letter.ps \
			testfile.jpg \
			testfile.pcl \
			testfile.pdf \
			testfile.ps \
			testfile.txt
TESTFILES	=	\
			cancel-current-job.test \
			create-job-format.test \
			create-job-sheets.test \
			create-job-timeout.test \
			create-job.test \
			create-printer-subscription.test \
			cups-create-local-printer.test \
			fax-job.test \
			get-completed-jobs.test \
			get-devices.test \
			get-job-attributes.test \
			get-job-attributes2.test \
			get-job-template-attributes.test \
			get-jobs.test \
			get-notifications.test \
			get-ppd-printer.test \
			get-ppd.test \
			get-ppds-drv-only.test \
			get-ppds-language.test \
			get-ppds-make-and-model.test \
			get-ppds-make.test \
			get-ppds-product.test \
			get-ppds-psversion.test \
			get-ppds.test \
			get-printer-attributes-suite.test \
			get-printer-attributes.test \
			get-printer-description-attributes.test \
			get-printers-printer-id.test \
			get-printers.test \
			get-subscriptions.test \
			identify-printer-display.test \
			identify-printer-multiple.test \
			identify-printer.test \
			ipp-1.1.test \
			ipp-2.0.test \
			ipp-2.1.test \
			ipp-2.2.test \
			ipp-backend.test \
			ipp-everywhere.test \
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
			set-attrs-hold.test \
			validate-job.test


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
