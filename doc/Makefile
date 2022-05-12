#
# Documentation makefile for libcups.
#
# Copyright © 2021-2022 by OpenPrinting.
#
# Licensed under Apache License v2.0.  See the file "LICENSE" for more
# information.
#

include ../Makedefs

#
# Documentation files...
#

DOCFILES	=	\
			cupspm.epub \
			cupspm.html \
			ippevepcl.html \
			ippeveprinter.html \
			ipptool.html \
			ipptoolfile.html \
			raster-organization.png \
			raster.png \
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
# Make unit tests...
#

test:


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
	echo "Installing documentation files to $(BUILDROOT)$(datadir)/doc/libcups..."
	$(INSTALL_DIR) $(BUILDROOT)$(datadir)/doc/libcups
	for file in $(DOCFILES); do \
		$(INSTALL_MAN) $$file $(BUILDROOT)$(datadir)/doc/libcups; \
	done


#
# Uninstall all documentation files...
#

uninstall:
	echo "Uninstalling documentation files from $(BUILDROOT)$(datadir)/doc/libcups..."
	for file in $(DOCFILES); do \
		$(RM) $(BUILDROOT)$(datadir)/doc/libcups/$$file; \
	done
	-$(RMDIR) $(BUILDROOT)$(datadir)/doc/libcups
