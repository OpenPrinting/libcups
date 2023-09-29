#
# Documentation makefile for libcups.
#
# Copyright © 2021-2023 by OpenPrinting.
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
			ipptransform.html


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
