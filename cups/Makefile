#
# Library Makefile for libcups.
#
# Copyright © 2021 by OpenPrinting.
# Copyright © 2007-2021 by Apple Inc.
# Copyright © 1997-2006 by Easy Software Products, all rights reserved.
#
# Licensed under Apache License v2.0.  See the file "LICENSE" for more
# information.
#

include ../Makedefs


#
# Object files...
#

LIBOBJS		= \
		array.o \
		auth.o \
		debug.o \
		dest.o \
		dest-job.o \
		dest-localization.o \
		dest-options.o \
		dir.o \
		encode.o \
		file.o \
		getputfile.o \
		globals.o \
		hash.o \
		http.o \
		http-addr.o \
		http-addrlist.o \
		http-support.o \
		ipp.o \
		ipp-file.o \
		ipp-vars.o \
		ipp-support.o \
		langprintf.o \
		language.o \
		md5.o \
		md5passwd.o \
		notify.o \
		options.o \
		pwg-media.o \
		raster-error.o \
		raster-stream.o \
		raster-stubs.o \
		request.o \
		snprintf.o \
		string.o \
		tempfile.o \
		thread.o \
		tls.o \
		transcode.o \
		usersys.o \
		util.o
TESTOBJS	= \
		fuzzipp.o \
		rasterbench.o \
		testarray.o \
		testclient.o \
		testcreds.o \
		testdest.o \
		testfile.o \
		testgetdests.o \
		testhttp.o \
		testi18n.o \
		testipp.o \
		testoptions.o \
		testraster.o \
		testthreads.o \
		tlscheck.o
OBJS	=	\
		$(LIBOBJS) \
		$(TESTOBJS)

#		testcups.o \
#		testlang.o \
#		testpwg.o \


#
# Header files to install...
#

HEADERS	=	\
		array.h \
		cups.h \
		dir.h \
		file.h \
		http.h \
		ipp.h \
		language.h \
		pwg.h \
		raster.h \
		transcode.h \
		versioning.h


#
# Targets in this directory...
#

LIBTARGETS =	\
		libcups.a \
		$(LIBCUPS)

UNITTARGETS =	\
		fuzzipp \
		rasterbench \
		testadmin \
		testarray \
		testcache \
		testclient \
		testconflicts \
		testcreds \
		testdest \
		testfile \
		testgetdests \
		testhttp \
		testi18n \
		testipp \
		testoptions \
		testraster \
		testsnmp \
		testthreads \
		tlscheck

#		testcups \
#		testlang \
#		testpwg \

TARGETS	=	\
		$(LIBTARGETS)


#
# Make all targets...
#

all:		$(TARGETS)


#
# Make unit tests...
#

unittests:	$(UNITTARGETS)


#
# Remove object and target files...
#

clean:
	$(RM) $(OBJS) $(TARGETS) $(UNITTARGETS)
	$(RM) libcups.so libcups.dylib


#
# Update dependencies (without system header dependencies...)
#

depend:
	$(CC) -MM -I.. -D_CUPS_SOURCE $(CPPFLAGS) $(OPTIONS) $(OBJS:.o=.c) >Dependencies


#
# Install all targets...
#

install:	all $(INSTALLSTATIC)
	echo Installing header files into $(BUILDROOT)$(includedir)/cups...
	$(INSTALL_DIR) -m 755 $(BUILDROOT)$(includedir)/cups
	for file in $(HEADERS); do \
		$(INSTALL_DATA) $$file $(BUILDROOT)$(includedir)/cups; \
	done
	echo Installing libraries in $(BUILDROOT)$(libdir)...
	$(INSTALL_DIR) -m 755 $(BUILDROOT)$(libdir)
	$(INSTALL_LIB) $(LIBCUPS) $(BUILDROOT)$(libdir)
	if test $(LIBCUPS) = "libcups.so.3"; then \
		$(RM) $(BUILDROOT)$(libdir)/`basename $(LIBCUPS) .3`; \
		$(LN) $(LIBCUPS) $(BUILDROOT)$(libdir)/`basename $(LIBCUPS) .3`; \
	fi
	if test $(LIBCUPS) = "libcups.3.dylib"; then \
		$(RM) $(BUILDROOT)$(libdir)/libcups.dylib; \
		$(LN) $(LIBCUPS) $(BUILDROOT)$(libdir)/libcups.dylib; \
	fi
	if test "x$(SYMROOT)" != "x"; then \
		$(INSTALL_DIR) $(SYMROOT); \
		cp $(LIBCUPS) $(SYMROOT); \
		dsymutil $(SYMROOT)/$(LIBCUPS); \
	fi

installstatic:
	$(INSTALL_DIR) -m 755 $(BUILDROOT)$(libdir)
	$(INSTALL_LIB) -m 755 $(LIBCUPSSTATIC) $(BUILDROOT)$(libdir)
	$(RANLIB) $(BUILDROOT)$(libdir)/$(LIBCUPSSTATIC)
	$(CHMOD) 555 $(BUILDROOT)$(libdir)/$(LIBCUPSSTATIC)


#
# Uninstall object and target files...
#

uninstall:
	$(RM) $(BUILDROOT)$(libdir)/libcups.3.dylib
	$(RM) $(BUILDROOT)$(libdir)/$(LIBCUPSSTATIC)
	$(RM) $(BUILDROOT)$(libdir)/libcups.dylib
	$(RM) $(BUILDROOT)$(libdir)/libcups.so
	$(RM) $(BUILDROOT)$(libdir)/libcups.so.3
	-$(RMDIR) $(BUILDROOT)$(libdir)
	for file in $(HEADERS); do \
		$(RM) $(BUILDROOT)$(includedir)/cups/$$file; \
	done
	-$(RMDIR) $(BUILDROOT)$(includedir)/cups


#
# libcups.so.3
#

libcups.so.3:	$(LIBOBJS)
	echo Linking $@...
	$(CC) $(DSOFLAGS) $(OPTIM) -o $@ $(LIBOBJS) $(LIBS)
	$(RM) `basename $@ .3`
	$(LN) $@ `basename $@ .3`


#
# libcups.3.dylib
#

libcups.3.dylib:	$(LIBOBJS)
	echo Linking $@...
	$(CC) $(DSOFLAGS) $(OPTIM) -o $@ -dynamiclib \
		-install_name $(libdir)/$@ \
		-current_version 3.0.0 -compatibility_version 3.0.0 \
		$(LIBOBJS) $(LIBS)
	$(CODE_SIGN) $(CSFLAGS) $@
	$(RM) libcups.dylib
	$(LN) $@ libcups.dylib


#
# libcups.a
#

libcups.a:	$(LIBOBJS)
	echo Archiving $@...
	$(RM) $@
	$(AR) $(ARFLAGS) $@ $(LIBOBJS)
	$(RANLIB) $@


#
# libcups3.def (Windows DLL exports file...)
#

libcups3.def: $(LIBOBJS) $(IMAGEOBJS) Makefile
	echo Generating $@...
	echo "LIBRARY libcups3" >$@
	echo "VERSION 3.0" >>$@
	echo "EXPORTS" >>$@
	(nm $(LIBOBJS) 2>/dev/null | grep "T _" | awk '{print $$3}'; \
	 echo __cups_strcpy; echo __cups_strlcpy; echo __cups_gettimeofday) | \
		grep -v -E -e '^cups_debug' -e 'Block$$' | \
		sed -e '1,$$s/^_//' | sort >>$@


#
# fuzzipp (dependency on static CUPS library is intentional)
#

fuzzipp:	fuzzipp.o $(LIBCUPSSTATIC)
	echo Linking $@...
	$(LD_CC) $(LDFLAGS) $(OPTIM) -o $@ fuzzipp.o $(LIBCUPSSTATIC) $(LIBS)
	$(CODE_SIGN) $(CSFLAGS) $@
	echo Fuzzing IPP API...
	./fuzzipp


#
# rasterbench (dependency on static CUPS library is intentional)
#

rasterbench:	rasterbench.o $(LIBCUPSSTATIC)
	echo Linking $@...
	$(LD_CC) $(LDFLAGS) $(OPTIM) -o $@ rasterbench.o $(LIBCUPSSTATIC) $(LIBS)
	$(CODE_SIGN) $(CSFLAGS) $@


#
# testarray (dependency on static CUPS library is intentional)
#

testarray:	testarray.o $(LIBCUPSSTATIC)
	echo Linking $@...
	$(LD_CC) $(LDFLAGS) $(OPTIM) -o $@ testarray.o $(LIBCUPSSTATIC) $(LIBS)
	$(CODE_SIGN) $(CSFLAGS) $@
	echo Running array API tests...
	./testarray


#
# testclient (dependency on static libraries is intentional)
#

testclient:	testclient.o $(LIBCUPSSTATIC)
	echo Linking $@...
	$(LD_CC) $(LDFLAGS) $(OPTIM) -o $@ testclient.o $(LIBCUPSSTATIC) $(LIBS)
	$(CODE_SIGN) $(CSFLAGS) $@


#
# testcreds (dependency on static CUPS library is intentional)
#

testcreds:	testcreds.o $(LIBCUPSSTATIC)
	echo Linking $@...
	$(LD_CC) $(LDFLAGS) $(OPTIM) -o $@ testcreds.o $(LIBCUPSSTATIC) $(LIBS)
	$(CODE_SIGN) $(CSFLAGS) $@


#
# testcups (dependency on static CUPS library is intentional)
#

testcups:	testcups.o $(LIBCUPSSTATIC)
	echo Linking $@...
	$(LD_CC) $(LDFLAGS) $(OPTIM) -o $@ testcups.o $(LIBCUPSSTATIC) $(LIBS)
	$(CODE_SIGN) $(CSFLAGS) $@


#
# testdest (dependency on static CUPS library is intentional)
#

testdest:	testdest.o $(LIBCUPSSTATIC)
	echo Linking $@...
	$(LD_CC) $(LDFLAGS) $(OPTIM) -o $@ testdest.o $(LIBCUPSSTATIC) $(LIBS)
	$(CODE_SIGN) $(CSFLAGS) $@


#
# testfile (dependency on static CUPS library is intentional)
#

testfile:	testfile.o $(LIBCUPSSTATIC)
	echo Linking $@...
	$(LD_CC) $(LDFLAGS) $(OPTIM) -o $@ testfile.o $(LIBCUPSSTATIC) $(LIBS)
	$(CODE_SIGN) $(CSFLAGS) $@
	echo Running file API tests...
	./testfile


#
# testgetdests (dependency on static CUPS library is intentional)
#

testgetdests:	testgetdests.o $(LIBCUPSSTATIC)
	echo Linking $@...
	$(LD_CC) $(LDFLAGS) $(OPTIM) -o $@ testgetdests.o $(LIBCUPSSTATIC) $(LIBS)
	$(CODE_SIGN) $(CSFLAGS) $@


#
# testhttp (dependency on static CUPS library is intentional)
#

testhttp:	testhttp.o $(LIBCUPSSTATIC)
	echo Linking $@...
	$(LD_CC) $(LDFLAGS) $(OPTIM) -o $@ testhttp.o $(LIBCUPSSTATIC) $(LIBS)
	$(CODE_SIGN) $(CSFLAGS) $@
	echo Running HTTP API tests...
	./testhttp


#
# testipp (dependency on static CUPS library is intentional)
#

testipp:	testipp.o $(LIBCUPSSTATIC)
	echo Linking $@...
	$(LD_CC) $(LDFLAGS) $(OPTIM) -o $@ testipp.o $(LIBCUPSSTATIC) $(LIBS)
	$(CODE_SIGN) $(CSFLAGS) $@
	echo Running IPP API tests...
	./testipp


#
# testi18n (dependency on static CUPS library is intentional)
#

testi18n:	testi18n.o $(LIBCUPSSTATIC)
	echo Linking $@...
	$(LD_CC) $(LDFLAGS) $(OPTIM) -o $@ testi18n.o $(LIBCUPSSTATIC) $(LIBS)
	$(CODE_SIGN) $(CSFLAGS) $@
	echo Running internationalization API tests...
	./testi18n


#
# testlang (dependency on static CUPS library is intentional)
#

testlang:	testlang.o $(LIBCUPSSTATIC)
	echo Linking $@...
	$(LD_CC) $(LDFLAGS) $(OPTIM) -o $@ testlang.o $(LIBCUPSSTATIC) $(LIBS)
	$(CODE_SIGN) $(CSFLAGS) $@
	echo Creating locale directory structure...
	$(RM) -r locale
	for po in ../locale/cups_*.po; do \
		lang=`basename $$po .po | sed -e '1,$$s/^cups_//'`; \
		$(MKDIR) locale/$$lang; \
		$(LN) ../../$$po locale/$$lang; \
	done
	echo Running language API tests...
	echo ls -al locale
	ls -al locale
	LOCALEDIR=locale ./testlang


#
# testoptions (dependency on static CUPS library is intentional)
#

testoptions:	testoptions.o $(LIBCUPSSTATIC)
	echo Linking $@...
	$(LD_CC) $(LDFLAGS) $(OPTIM) -o $@ testoptions.o $(LIBCUPSSTATIC) $(LIBS)
	$(CODE_SIGN) $(CSFLAGS) $@
	echo Running option API tests...
	./testoptions


#
# testpwg (dependency on static CUPS library is intentional)
#

testpwg:	testpwg.o $(LIBCUPSSTATIC) test.ppd
	echo Linking $@...
	$(LD_CC) $(LDFLAGS) $(OPTIM) -o $@ testpwg.o $(LIBCUPSSTATIC) $(LIBS)
	$(CODE_SIGN) $(CSFLAGS) $@
	echo Running PWG API tests...
	./testpwg test.ppd


#
# testraster (dependency on static CUPS library is intentional)
#

testraster:	testraster.o $(LIBCUPSSTATIC)
	echo Linking $@...
	$(LD_CC) $(LDFLAGS) $(OPTIM) -o $@ testraster.o $(LIBCUPSSTATIC) $(LIBS)
	$(CODE_SIGN) $(CSFLAGS) $@
	echo Running raster API tests...
	./testraster


#
# testthreads (dependency on static CUPS library is intentional)
#

testthreads:	testthreads.o $(LIBCUPSSTATIC)
	echo Linking $@...
	$(LD_CC) $(LDFLAGS) $(OPTIM) -o $@ testthreads.o $(LIBCUPSSTATIC) $(LIBS)
	$(CODE_SIGN) $(CSFLAGS) $@


#
# tlscheck (dependency on static CUPS library is intentional)
#

tlscheck:	tlscheck.o $(LIBCUPSSTATIC)
	echo Linking $@...
	$(LD_CC) $(LDFLAGS) $(OPTIM) -o $@ tlscheck.o $(LIBCUPSSTATIC) $(LIBS)
	$(CODE_SIGN) $(CSFLAGS) $@


#
# Generate documentation using Codedoc (https://www.msweet.org/codedoc)
#

doc:
	echo "Generating CUPS Programming Manual..."
	$(RM) cupspm.xml
	codedoc --section "Programming" --body cupspm.md \
		cupspm.xml \
		array.[ch] auth.c cups.h dest*.c dir.[ch] encode.c file.[ch] \
		http.h http*.c ipp.h ipp*.c options.c pwg.[ch] \
		raster.h raster-stubs.c tls-darwin.c usersys.c util.c \
		--coverimage cupspm.png \
		--epub ../doc/help/cupspm.epub
	codedoc --section "Programming" --body cupspm.md \
		cupspm.xml > ../doc/help/cupspm.html
	$(RM) cupspm.xml


#
# Lines of code computation...
#

sloc:
	echo "libcups: \c"
	sloccount $(LIBOBJS:.o=.c) 2>/dev/null | grep "Total Physical" | awk '{print $$9}'


#
# Dependencies...
#

include Dependencies
tls.o: tls-darwin.c tls-gnutls.c tls-sspi.c
