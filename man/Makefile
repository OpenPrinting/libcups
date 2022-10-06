#
# Man page makefile for libcups.
#
# Copyright © 2021 by OpenPrinting.
# Copyright © 2007-2019 by Apple Inc.
# Copyright © 1993-2006 by Easy Software Products.
#
# Licensed under Apache License v2.0.  See the file "LICENSE" for more
# information.
#

include ../Makedefs


#
# Man pages...
#

MAN1	=	\
		ippeveprinter.1 \
		ippfind.1 \
		ipptool.1
MAN5	=	\
		ipptoolfile.5
MAN7	=	\
		ippevepcl.7


#
# Make everything...
#

all:		$(LOCALTARGET)


#
# Make unit tests...
#

test:


#
# Clean all config and object files...
#

clean:


#
# Dummy depend target...
#

depend:


#
# Install all targets...
#

install:
	echo "Installing man pages in $(BUILDROOT)$(mandir)/man1..."
	$(INSTALL_DIR) -m 755 $(BUILDROOT)$(mandir)/man1
	for file in $(MAN1); do \
		$(INSTALL_MAN) $$file $(BUILDROOT)$(mandir)/man1; \
	done
	echo "Installing man pages in $(BUILDROOT)$(mandir)/man5..."
	$(INSTALL_DIR) -m 755 $(BUILDROOT)$(mandir)/man5
	for file in $(MAN5); do \
		$(INSTALL_MAN) $$file $(BUILDROOT)$(mandir)/man5; \
	done
	echo "Installing man pages in $(BUILDROOT)$(mandir)/man7..."
	$(INSTALL_DIR) -m 755 $(BUILDROOT)$(mandir)/man7
	for file in $(MAN7); do \
		$(INSTALL_MAN) $$file $(BUILDROOT)$(mandir)/man7; \
	done
	$(RM) $(BUILDROOT)$(mandir)/man7/ippeveps.7
	$(LN) ippevepcl.7 $(BUILDROOT)$(mandir)/man7/ippeveps.7


#
# Uninstall files...
#

uninstall:
	echo "Uninstalling man pages from $(BUILDROOT)$(mandir)/man1..."
	for file in $(MAN1); do \
		$(RM) $(BUILDROOT)$(mandir)/man1/$$file; \
	done
	-$(RMDIR) $(BUILDROOT)$(mandir)/man1
	echo "Uninstalling man pages from $(BUILDROOT)$(mandir)/man5..."
	for file in $(MAN5); do \
		$(RM) $(BUILDROOT)$(mandir)/man5/$$file; \
	done
	-$(RMDIR) $(BUILDROOT)$(mandir)/man5
	echo "Uninstalling man pages from $(BUILDROOT)$(mandir)/man7..."
	for file in $(MAN7) ippeveps.7; do \
		$(RM) $(BUILDROOT)$(mandir)/man7/$$file; \
	done
	-$(RMDIR) $(BUILDROOT)$(mandir)/man7


#
# Local programs (not built when cross-compiling...)
#

local:


#
# Make html versions of man pages using <https://www.msweet.org/mantohtml>.
#

html:
	echo Converting man pages to HTML...
	for file in $(MAN1); do \
		echo "    $$file..."; \
		mantohtml --css man.css $$file >../doc/`basename $$file .1`.html; \
	done
	for file in $(MAN5); do \
		echo "    $$file..."; \
		mantohtml --css man.css $$file >../doc/`basename $$file .5`.html; \
	done
	for file in $(MAN7); do \
		echo "    $$file..."; \
		mantohtml --css man.css $$file >../doc/`basename $$file .7`.html; \
	done
