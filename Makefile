#
# Top-level Makefile for libcups.
#
# Copyright © 2020-2021 by OpenPrinting
# Copyright © 2007-2019 by Apple Inc.
# Copyright © 1997-2007 by Easy Software Products, all rights reserved.
#
# Licensed under Apache License v2.0.  See the file "LICENSE" for more
# information.
#

include Makedefs


#
# Directories to make...
#

DIRS	=	cups tools


#
# Make all targets...
#

all:
	echo Using CC="$(CC)"
	echo Using CFLAGS="-I.. -D_CUPS_SOURCE $(CPPFLAGS) $(CFLAGS) $(OPTIM) $(WARNINGS)"
	echo Using CPPFLAGS="-I.. -D_CUPS_SOURCE $(CPPFLAGS) $(OPTIONS)"
	echo Using DSO="$(DSO)"
	echo Using DSOFLAGS="$(DSOFLAGS)"
	echo Using LDFLAGS="$(LDFLAGS)"
	echo Using LIBS="$(LIBS)"
	for dir in $(DIRS); do\
		echo Making all in $$dir... ;\
		(cd $$dir ; $(MAKE) $(MFLAGS) all) || exit 1;\
	done


#
# Remove object and target files...
#

clean:
	for dir in $(DIRS); do\
		echo Cleaning in $$dir... ;\
		(cd $$dir; $(MAKE) $(MFLAGS) clean) || exit 1;\
	done


#
# Remove all non-distribution files...
#

distclean:	clean
	$(RM) Makedefs config.h config.log config.status cups.pc
	-$(RM) -r autom4te*.cache


#
# Make dependencies
#

depend:
	echo Using CC="$(CC)"
	echo Using CPPFLAGS="-I.. -D_CUPS_SOURCE $(CPPFLAGS) $(OPTIONS)"
	for dir in $(DIRS); do\
		echo Making dependencies in $$dir... ;\
		(cd $$dir; $(MAKE) $(MFLAGS) depend) || exit 1;\
	done


#
# Install everything...
#

install:
	for dir in $(DIRS); do\
		echo Installing all in $$dir... ;\
		(cd $$dir; $(MAKE) $(MFLAGS) install) || exit 1;\
	done
	echo Installing cups.pc file...
	$(INSTALL_DIR) -m 755 $(BUILDROOT)$(libdir)/pkgconfig
	$(INSTALL_DATA) cups.pc $(BUILDROOT)$(libdir)/pkgconfig/cups.pc


#
# Uninstall everything...
#

uninstall:
	for dir in $(DIRS); do\
		echo Uninstalling in $$dir... ;\
		(cd $$dir; $(MAKE) $(MFLAGS) uninstall) || exit 1;\
	done
	$(RM) $(BUILDROOT)$(libdir)/pkgconfig/cups.pc
	-$(RMDIR) $(BUILDROOT)$(libdir)/pkgconfig


#
# Test everything...
#

test:		all
	for dir in $(DIRS); do\
		echo Making all in $$dir... ;\
		(cd $$dir ; $(MAKE) $(MFLAGS) test) || exit 1;\
	done


#
# Create HTML documentation using codedoc (http://www.msweet.org/codedoc)...
#

doc:
	cd cups; $(MAKE) $(MFLAGS) doc


#
# Lines of code computation...
#

sloc:
	for dir in $(DIRS); do \
		(cd $$dir; $(MAKE) $(MFLAGS) sloc) || exit 1;\
	done


#
# Don't run top-level build targets in parallel...
#

.NOTPARALLEL:
