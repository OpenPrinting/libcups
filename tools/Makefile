#
# IPP tools makefile for libcups.
#
# Copyright © 2021-2023 by OpenPrinting.
# Copyright © 2007-2019 by Apple Inc.
# Copyright © 1997-2006 by Easy Software Products, all rights reserved.
#
# Licensed under Apache License v2.0.  See the file "LICENSE" for more
# information.
#

include ../Makedefs


OBJS		=	\
			ipp-options.o \
			ippeveprinter.o \
			ippfind.o \
			ipptool.o \
			ipptransform.o
IPPTOOLS	=	\
			ippeveprinter \
			ippfind \
			ipptool \
			ipptransform
TARGETS		=	\
			$(IPPTOOLS)


#
# Make all targets...
#

all:		$(TARGETS)


#
# Make library targets...
#

libs:


#
# Make unit tests...
#

unittests:


#
# Clean all object files...
#

clean:
	$(RM) $(IPPTOOLS) $(OBJS)
	$(RM) test.log test-cups.log


#
# Update dependencies (without system header dependencies...)
#

depend:
	$(CC) -MM -I.. -D_CUPS_SOURCE $(CPPFLAGS) $(OPTIONS) $(OBJS:.o=.c) | sed -e '1,$$s/ \/usr\/include\/[^ ]*//g' -e '1,$$s/ \/usr\/local\/include\/[^ ]*//g' | grep -e '^  \\$$' -v >Dependencies


#
# Install all targets...
#

install:	all
	echo "Installing IPP tools to $(BUILDROOT)$(bindir)..."
	$(INSTALL_DIR) $(BUILDROOT)$(bindir)
	for file in $(IPPTOOLS); do \
		$(INSTALL_BIN) $$file $(BUILDROOT)$(bindir); \
	done
	for file in ippevepcl ippeveps; do \
		$(RM) $(BUILDROOT)$(bindir)/$$file; \
		$(LN) ipptransform $(BUILDROOT)$(bindir)/$$file; \
	done
	if test "x$(SYMROOT)" != "x"; then \
		echo "Copying debug symbols to $(SYMROOT)..."; \
		$(INSTALL_DIR) $(SYMROOT); \
		for file in $(IPPTOOLS); do \
			cp $$file $(SYMROOT); \
			dsymutil $(SYMROOT)/$$file; \
		done; \
	fi


#
# Unnstall all targets...
#

uninstall:
	echo "Uninstalling IPP tools from $(BUILDROOT)$(bindir)..."
	for file in $(IPPTOOLS) ippevepcl ippeveps; do \
		$(RM) $(BUILDROOT)$(bindir)/$$file; \
	done
	-$(RMDIR) $(BUILDROOT)$(bindir)


#
# Unit test programs (not built when cross-compiling...)
#

unittests:	ippeveprinter-static ippfind-static ipptool-static


#
# Test everything...
#

test:		unittests
	./run-tests.sh


#
# ippeveprinter
#

ippeveprinter:	ippeveprinter.o ../cups/$(LIBCUPS) ippeveprinter-static
	echo Linking $@...
	$(CC) $(LDFLAGS) $(OPTIM) -o $@ ippeveprinter.o $(LINKCUPS) $(LIBS)
	$(CODE_SIGN) $(CSFLAGS) $@


#
# ippeveprinter-static
#

ippeveprinter-static:	ippeveprinter.o ../cups/$(LIBCUPS_STATIC)
	echo Linking $@...
	$(CC) $(LDFLAGS) $(OPTIM) -o $@ ippeveprinter.o $(PAMLIBS) ../cups/$(LIBCUPS_STATIC) $(LIBS)
	$(CODE_SIGN) $(CSFLAGS) $@


#
# ippfind
#

ippfind:	ippfind.o ../cups/$(LIBCUPS) ippfind-static
	echo Linking $@...
	$(CC) $(LDFLAGS) $(OPTIM) -o $@ ippfind.o $(DNSSDLIBS) $(LINKCUPS) $(LIBS)
	$(CODE_SIGN) $(CSFLAGS) $@


#
# ippfind-static
#

ippfind-static:	ippfind.o ../cups/$(LIBCUPS_STATIC)
	echo Linking $@
	$(CC) $(LDFLAGS) $(OPTIM) -o $@ ippfind.o ../cups/$(LIBCUPS_STATIC) $(LIBS)
	$(CODE_SIGN) $(CSFLAGS) $@


#
# ipptool
#

ipptool:	ipptool.o ../cups/$(LIBCUPS) ipptool-static
	echo Linking $@...
	$(CC) $(LDFLAGS) $(OPTIM) -o $@ ipptool.o $(LINKCUPS) $(LIBS)
	$(CODE_SIGN) $(CSFLAGS) $@


#
# ipptool-static
#

ipptool-static:	ipptool.o ../cups/$(LIBCUPS_STATIC)
	echo Linking $@...
	$(CC) $(LDFLAGS) $(OPTIM) -o $@ ipptool.o ../cups/$(LIBCUPS_STATIC) $(LIBS)
	$(CODE_SIGN) $(CSFLAGS) $@


#
# ipptransform
#

ipptransform:	ipptransform.o ipp-options.o libpdfio.a ../cups/$(LIBCUPS) ipptransform-static
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ ipptransform.o ipp-options.o $(LINKCUPS) $(XFORMLIBS) $(LIBS)
	$(CODE_SIGN) $(CSFLAGS) $@


#
# ipptransform-static
#

ipptransform-static:	ipptransform.o ipp-options.o libpdfio.a ../cups/$(LIBCUPS_STATIC)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ ipptransform.o ipp-options.o ../cups/$(LIBCUPS_STATIC) $(XFORMLIBS) $(LIBS)
	$(CODE_SIGN) $(CSFLAGS) $@


#
# Generate the headers containing the data for printer*.png...
#

pngheaders:
	for file in printer-sm.png printer.png printer-lg.png; do \
		hfile="$$(basename $$file .png)-png.h"; \
		cdef="$$(basename $$file .png | tr '-' '_' | awk '{print toupper($$0)}')_PNG_H"; \
		echo "Generating $$hfile from $$file..."; \
		echo "#ifndef $$cdef" >$$hfile; \
		echo "#  define $$cdef" >>$$hfile; \
		echo "static const unsigned char $$(basename $$file .png | tr '-' '_')_png[] =" >>$$hfile; \
		echo "{" >>$$hfile; \
		od -t u1 -A n -v "$$file" | awk '{for (i = 1; i <= NF; i ++) printf("%s,", $$i); print "";}' >>$$hfile; \
		echo "};" >>$$hfile; \
		echo "#endif // !$$cdef" >>$$hfile; \
	done


#
# Dependencies...
#

include Dependencies
