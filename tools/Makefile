#
# IPP tools makefile for libcups.
#
# Copyright © 2021-2022 by OpenPrinting.
# Copyright © 2007-2019 by Apple Inc.
# Copyright © 1997-2006 by Easy Software Products, all rights reserved.
#
# Licensed under Apache License v2.0.  See the file "LICENSE" for more
# information.
#

include ../Makedefs


OBJS		=	\
			ippevepcl.o \
			ippeveprinter.o \
			ippeveps.o \
			ippfind.o \
			ipptool.o
IPPEVECOMMANDS	=	\
			ippevepcl \
			ippeveps
IPPTOOLS	=	\
			ippeveprinter \
			ippfind \
			ipptool
TARGETS		=	\
			$(IPPEVECOMMANDS) \
			$(IPPTOOLS) \
			$(LOCALTARGET)


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
	$(RM) $(IPPTOOLS) $(IPPEVECOMMANDS) $(OBJS)
	$(RM) ippeveprinter-static ippfind-static ipptool-static


#
# Update dependencies (without system header dependencies...)
#

depend:
	$(CC) -MM -I.. -D_CUPS_SOURCE $(CPPFLAGS) $(OPTIONS) $(OBJS:.o=.c) | sed -e '1,$$s/ \/usr\/include\/[^ ]*//g' -e '1,$$s/ \/usr\/local\/include\/[^ ]*//g' >Dependencies


#
# Install all targets...
#

install:	all
	echo "Installing IPP tools to $(BUILDROOT)$(bindir)..."
	$(INSTALL_DIR) $(BUILDROOT)$(bindir)
	for file in $(IPPTOOLS); do \
		$(INSTALL_BIN) $$file $(BUILDROOT)$(bindir); \
	done
	echo "Installing printer commands to $(BUILDROOT)$(libexecdir)/command..."
	$(INSTALL_DIR) $(BUILDROOT)$(libexecdir)/command
	for file in $(IPPEVECOMMANDS); do \
		$(INSTALL_BIN) $$file $(BUILDROOT)$(bindir); \
		$(LN) $(bindir)/$$file $(BUILDROOT)$(libexecdir)/ippeveprinter; \
	done
	if test "x$(SYMROOT)" != "x"; then \
		echo "Copying debug symbols to $(SYMROOT)..."; \
		$(INSTALL_DIR) $(SYMROOT); \
		for file in $(IPPTOOLS) $(IPPEVECOMMANDS); do \
			cp $$file $(SYMROOT); \
			dsymutil $(SYMROOT)/$$file; \
		done; \
	fi


#
# Unnstall all targets...
#

uninstall:
	echo "Uninstalling IPP tools from $(BUILDROOT)$(bindir)..."
	for file in $(IPPTOOLS); do \
		$(RM) $(BUILDROOT)$(bindir)/$$file; \
	done
	-$(RMDIR) $(BUILDROOT)$(bindir)
	echo "Uninstalling print commands from $(BUILDROOT)$(libexecdir)/command..."
	for file in $(IPPEVECOMMANDS); do \
		$(RM) $(BUILDROOT)$(bindir)/$$file; \
		$(RM) $(BUILDROOT)$(libexecdir)/command/$$file; \
	done
	-$(RMDIR) $(BUILDROOT)$(libexecdir)/command


#
# Local programs (not built when cross-compiling...)
#

local:		ippeveprinter-static ipptool-static


#
# Test everything...
#

test:		all


#
# ippeveprinter
#

ippeveprinter:	ippeveprinter.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CC) $(LDFLAGS) $(OPTIM) -o $@ ippeveprinter.o -L../cups -lcups $(LIBS)
	$(CODE_SIGN) $(CSFLAGS) $@


#
# ippeveprinter-static
#

ippeveprinter-static:	ippeveprinter.o ../cups/libcups.a
	echo Linking $@...
	$(CC) $(LDFLAGS) $(OPTIM) -o $@ ippeveprinter.o $(PAMLIBS) ../cups/libcups.a $(LIBS)
	$(CODE_SIGN) $(CSFLAGS) $@


#
# ippevepcl
#

ippevepcl:	ippevepcl.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CC) $(LDFLAGS) $(OPTIM) -o $@ ippevepcl.o -L../cups -lcups $(LIBS)
	$(CODE_SIGN) $(CSFLAGS) $@


#
# ippeveps
#

ippeveps:	ippeveps.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CC) $(LDFLAGS) $(OPTIM) -o $@ ippeveps.o -L../cups -lcups $(LIBS)
	$(CODE_SIGN) $(CSFLAGS) $@


#
# ippfind
#

ippfind:	ippfind.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CC) $(LDFLAGS) $(OPTIM) -o $@ ippfind.o $(DNSSDLIBS) -L../cups -lcups $(LIBS)
	$(CODE_SIGN) $(CSFLAGS) $@


#
# ippfind-static
#

ippfind-static:	ippfind.o ../cups/libcups.a
	echo Linking $@
	$(CC) $(LDFLAGS) $(OPTIM) -o $@ ippfind.o ../cups/libcups.a $(LIBS)
	$(CODE_SIGN) $(CSFLAGS) $@


#
# ipptool
#

ipptool:	ipptool.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CC) $(LDFLAGS) $(OPTIM) -o $@ ipptool.o -L../cups -lcups $(LIBS)
	$(CODE_SIGN) $(CSFLAGS) $@


#
# ipptool-static
#

ipptool-static:	ipptool.o ../cups/libcups.a
	echo Linking $@...
	$(CC) $(LDFLAGS) $(OPTIM) -o $@ ipptool.o ../cups/libcups.a $(LIBS)
	$(CODE_SIGN) $(CSFLAGS) $@


#
# Generate the header containing the data for printer.png...
#

pngheader:
	echo "Generating printer-png.h from printer.png..."
	echo "static const unsigned char printer_png[] =" >printer-png.h
	echo "{" >>printer-png.h
	od -t x1 printer.png | cut -b12- | awk '{printf("  "); for (i = 1; i <= NF; i ++) printf("0x%s,", $$i); print "";}' >>printer-png.h
	echo "};" >>printer-png.h


#
# Dependencies...
#

include Dependencies
