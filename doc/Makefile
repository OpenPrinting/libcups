#
# Documentation makefile for libcups.
#
# Copyright © 2021-2025 by OpenPrinting.
#
# Licensed under Apache License v2.0.  See the file "LICENSE" for more
# information.
#

include ../Makedefs

#
# Documentation files...
#

DOCFILES	=	\
			cups.png \
			cups.svg \
			cups-oauth.html \
			cups-x509.html \
			cupspm3.epub \
			cupspm3.html \
			index.html \
			ippeveprinter.html \
			ippfind.html \
			ipptool.html \
			ipptoolfile.html \
			ipptransform.html \
			libcups.css \
			libcups.html \
			raster.png \
			raster-organization.png \
			sample-image.png \
			spec-ipp.html \
			spec-raster.html


#
# Make all documents...
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
# Remove all generated files...
#

clean:


#
# Dummy depend target...
#

depend:


#
# Install all documentation files...
#

install:
	echo "Installing documentation files to $(BUILDROOT)$(datadir)/doc/libcups3..."
	$(INSTALL_DIR) $(BUILDROOT)$(datadir)/doc/libcups3
	for file in $(DOCFILES); do \
		$(INSTALL_MAN) $$file $(BUILDROOT)$(datadir)/doc/libcups3; \
	done


#
# Uninstall all documentation files...
#

uninstall:
	echo "Uninstalling documentation files from $(BUILDROOT)$(datadir)/doc/libcups3..."
	for file in $(DOCFILES); do \
		$(RM) $(BUILDROOT)$(datadir)/doc/libcups3/$$file; \
	done
	-$(RMDIR) $(BUILDROOT)$(datadir)/doc/libcups3
