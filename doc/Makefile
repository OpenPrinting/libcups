#
# Documentation makefile for libcups.
#
# Copyright © 2021 by OpenPrinting.
# Copyright © 2007-2019 by Apple Inc.
# Copyright © 1997-2007 by Easy Software Products.
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
	$(INSTALL_DIR) -m 755 $(BUILDROOT)$(datadir)/libcups
	for file in $(DOCFILES); do \
		$(INSTALL_MAN) $$file $(BUILDROOT)$(datadir)/libcups; \
	done


#
# Uninstall all documentation files...
#

uninstall:
	for file in $(DOCFILES); do \
		$(RM) $(BUILDROOT)$(datadir)/libcups/$$file; \
	done
	-$(RMDIR) $(BUILDROOT)$(datadir)/libcups
