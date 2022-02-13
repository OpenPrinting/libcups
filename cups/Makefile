#
# Library Makefile for libcups.
#
# Copyright © 2021-2022 by OpenPrinting.
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
		notify.o \
		options.o \
		pwg-media.o \
		raster-error.o \
		raster-stream.o \
		raster-stubs.o \
		request.o \
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
		testtestpage.o \
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
		base.h \
		cups.h \
		dir.h \
		file.h \
		http.h \
		ipp.h \
		language.h \
		pwg.h \
		raster.h \
		transcode.h


#
# Targets in this directory...
#

LIBTARGETS =	\
		libcups.a \
		$(LIBCUPS)

UNITTARGETS =	\
		fuzzipp \
		rasterbench \
		testarray \
		testclient \
		testcreds \
		testdest \
		testfile \
		testgetdests \
		testhttp \
		testi18n \
		testipp \
		testoptions \
		testraster \
		testtestpage \
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
# Make and run unit tests...
#

test:		$(UNITTARGETS)
	rm -f test.log
	echo Running array API tests...
	./testarray 2>>test.log
	echo Running file API tests...
	./testfile 2>>test.log
	echo Running HTTP API tests...
	./testhttp 2>>test.log
	echo Running IPP API tests...
	./testipp 2>>test.log
	echo Fuzzing IPP API...
	./fuzzipp 2>>test.log
	echo Running internationalization API tests...
	./testi18n 2>>test.log
	echo Running option API tests...
	./testoptions 2>>test.log
	echo Running raster API tests...
	./testraster 2>>test.log


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
	$(INSTALL_LIB) -m 755 libcups.a $(BUILDROOT)$(libdir)
	$(RANLIB) $(BUILDROOT)$(libdir)/libcups.a
	$(CHMOD) 555 $(BUILDROOT)$(libdir)/libcups.a


#
# Uninstall object and target files...
#

uninstall:
	$(RM) $(BUILDROOT)$(libdir)/libcups.3.dylib
	$(RM) $(BUILDROOT)$(libdir)/libcups.a
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
	$(CC) $(DSOFLAGS) $(OPTIM) -o $@ -install_name $(libdir)/$@ \
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
	 echo __cups_strcpy; echo __cups_strlcat; echo __cups_strlcpy; \
	 	echo __cups_gettimeofday; echo __cups_hstrerror) | \
		grep -v -E -e '^cups_debug' -e 'Block$$' -e '^__cupsApple' | \
		sed -e '1,$$s/^_//' | sort >>$@


#
# fuzzipp (dependency on static CUPS library is intentional)
#

fuzzipp:	fuzzipp.o libcups.a
	echo Linking $@...
	$(CC) $(LDFLAGS) $(OPTIM) -o $@ fuzzipp.o libcups.a $(LIBS)
	$(CODE_SIGN) $(CSFLAGS) $@


#
# rasterbench (dependency on static CUPS library is intentional)
#

rasterbench:	rasterbench.o libcups.a
	echo Linking $@...
	$(CC) $(LDFLAGS) $(OPTIM) -o $@ rasterbench.o libcups.a $(LIBS)
	$(CODE_SIGN) $(CSFLAGS) $@


#
# testarray (dependency on static CUPS library is intentional)
#

testarray:	testarray.o libcups.a
	echo Linking $@...
	$(CC) $(LDFLAGS) $(OPTIM) -o $@ testarray.o libcups.a $(LIBS)
	$(CODE_SIGN) $(CSFLAGS) $@


#
# testclient (dependency on static libraries is intentional)
#

testclient:	testclient.o libcups.a
	echo Linking $@...
	$(CC) $(LDFLAGS) $(OPTIM) -o $@ testclient.o libcups.a $(LIBS)
	$(CODE_SIGN) $(CSFLAGS) $@


#
# testcreds (dependency on static CUPS library is intentional)
#

testcreds:	testcreds.o libcups.a
	echo Linking $@...
	$(CC) $(LDFLAGS) $(OPTIM) -o $@ testcreds.o libcups.a $(LIBS)
	$(CODE_SIGN) $(CSFLAGS) $@


#
# testcups (dependency on static CUPS library is intentional)
#

testcups:	testcups.o libcups.a
	echo Linking $@...
	$(CC) $(LDFLAGS) $(OPTIM) -o $@ testcups.o libcups.a $(LIBS)
	$(CODE_SIGN) $(CSFLAGS) $@


#
# testdest (dependency on static CUPS library is intentional)
#

testdest:	testdest.o libcups.a
	echo Linking $@...
	$(CC) $(LDFLAGS) $(OPTIM) -o $@ testdest.o libcups.a $(LIBS)
	$(CODE_SIGN) $(CSFLAGS) $@


#
# testfile (dependency on static CUPS library is intentional)
#

testfile:	testfile.o libcups.a
	echo Linking $@...
	$(CC) $(LDFLAGS) $(OPTIM) -o $@ testfile.o libcups.a $(LIBS)
	$(CODE_SIGN) $(CSFLAGS) $@


#
# testgetdests (dependency on static CUPS library is intentional)
#

testgetdests:	testgetdests.o libcups.a
	echo Linking $@...
	$(CC) $(LDFLAGS) $(OPTIM) -o $@ testgetdests.o libcups.a $(LIBS)
	$(CODE_SIGN) $(CSFLAGS) $@


#
# testhttp (dependency on static CUPS library is intentional)
#

testhttp:	testhttp.o libcups.a
	echo Linking $@...
	$(CC) $(LDFLAGS) $(OPTIM) -o $@ testhttp.o libcups.a $(LIBS)
	$(CODE_SIGN) $(CSFLAGS) $@


#
# testipp (dependency on static CUPS library is intentional)
#

testipp:	testipp.o libcups.a
	echo Linking $@...
	$(CC) $(LDFLAGS) $(OPTIM) -o $@ testipp.o libcups.a $(LIBS)
	$(CODE_SIGN) $(CSFLAGS) $@


#
# testi18n (dependency on static CUPS library is intentional)
#

testi18n:	testi18n.o libcups.a
	echo Linking $@...
	$(CC) $(LDFLAGS) $(OPTIM) -o $@ testi18n.o libcups.a $(LIBS)
	$(CODE_SIGN) $(CSFLAGS) $@


#
# testlang (dependency on static CUPS library is intentional)
#

testlang:	testlang.o libcups.a
	echo Linking $@...
	$(CC) $(LDFLAGS) $(OPTIM) -o $@ testlang.o libcups.a $(LIBS)
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

testoptions:	testoptions.o libcups.a
	echo Linking $@...
	$(CC) $(LDFLAGS) $(OPTIM) -o $@ testoptions.o libcups.a $(LIBS)
	$(CODE_SIGN) $(CSFLAGS) $@


#
# testpwg (dependency on static CUPS library is intentional)
#

testpwg:	testpwg.o libcups.a test.ppd
	echo Linking $@...
	$(CC) $(LDFLAGS) $(OPTIM) -o $@ testpwg.o libcups.a $(LIBS)
	$(CODE_SIGN) $(CSFLAGS) $@
	echo Running PWG API tests...
	./testpwg test.ppd


#
# testraster (dependency on static CUPS library is intentional)
#

testraster:	testraster.o libcups.a
	echo Linking $@...
	$(CC) $(LDFLAGS) $(OPTIM) -o $@ testraster.o libcups.a $(LIBS)
	$(CODE_SIGN) $(CSFLAGS) $@


#
# testtestpage (dependency on static CUPS library is intentional)
#

testtestpage:	testtestpage.o libcups.a
	echo Compiling $@...
	$(CC) $(LDFLAGS) $(OPTIM) -o $@ testtestpage.o libcups.a $(LIBS)
	$(CODE_SIGN) $(CSFLAGS) $@


#
# testthreads (dependency on static CUPS library is intentional)
#

testthreads:	testthreads.o libcups.a
	echo Linking $@...
	$(CC) $(LDFLAGS) $(OPTIM) -o $@ testthreads.o libcups.a $(LIBS)
	$(CODE_SIGN) $(CSFLAGS) $@


#
# tlscheck (dependency on static CUPS library is intentional)
#

tlscheck:	tlscheck.o libcups.a
	echo Linking $@...
	$(CC) $(LDFLAGS) $(OPTIM) -o $@ tlscheck.o libcups.a $(LIBS)
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
