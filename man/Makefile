#
# Man page makefile for libcups.
#
# Copyright © 2021-2025 by OpenPrinting.
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
		cups-oauth.1 \
		cups-x509.1 \
		ippeveprinter.1 \
		ippfind.1 \
		ipptool.1 \
		ipptransform.1
MAN3	=	\
		libcups.3
MAN5	=	\
		client.conf.5 \
		ipptoolfile.5


#
# Make everything...
#

all:		$(LOCALTARGET)


#
# Run unit tests...
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
	$(RM) $(BUILDROOT)$(mandir)/man1/ippevepcl.1
	$(LN) ipptransform.1 $(BUILDROOT)$(mandir)/man1/ippevepcl.1
	$(RM) $(BUILDROOT)$(mandir)/man1/ippeveps.1
	$(LN) ipptransform.1 $(BUILDROOT)$(mandir)/man1/ippeveps.1
	echo "Installing man pages in $(BUILDROOT)$(mandir)/man3..."
	$(INSTALL_DIR) -m 755 $(BUILDROOT)$(mandir)/man3
	for file in $(MAN3); do \
		$(INSTALL_MAN) $$file $(BUILDROOT)$(mandir)/man3; \
	done
	echo "Installing man pages in $(BUILDROOT)$(mandir)/man5..."
	$(INSTALL_DIR) -m 755 $(BUILDROOT)$(mandir)/man5
	for file in $(MAN5); do \
		$(INSTALL_MAN) $$file $(BUILDROOT)$(mandir)/man5; \
	done


#
# Uninstall files...
#

uninstall:
	echo "Uninstalling man pages from $(BUILDROOT)$(mandir)/man1..."
	for file in $(MAN1) ippevepcl.1 ippeveps.1; do \
		$(RM) $(BUILDROOT)$(mandir)/man1/$$file; \
	done
	-$(RMDIR) $(BUILDROOT)$(mandir)/man1
	echo "Uninstalling man pages from $(BUILDROOT)$(mandir)/man5..."
	for file in $(MAN5); do \
		$(RM) $(BUILDROOT)$(mandir)/man5/$$file; \
	done
	-$(RMDIR) $(BUILDROOT)$(mandir)/man5


#
# Unit test programs (not built when cross-compiling...)
#

unittests:


#
# Make html versions of man pages using <https://www.msweet.org/mantohtml>.
#

html:
	echo Converting man pages to HTML...
	for file in $(MAN1); do \
		echo "    $$file..."; \
		mantohtml --css ../doc/libcups.css $$file >../doc/`basename $$file .1`.html; \
	done
	for file in $(MAN3); do \
		echo "    $$file..."; \
		mantohtml --css ../doc/libcups.css $$file >../doc/`basename $$file .3`.html; \
	done
	for file in $(MAN5); do \
		echo "    $$file..."; \
		mantohtml --css ../doc/libcups.css $$file >../doc/`basename $$file .5`.html; \
	done
