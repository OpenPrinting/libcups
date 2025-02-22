Building and Installing the CUPS Library (libcups)
==================================================

This file describes how to compile and install the CUPS library from source
code.  For more information on libcups see the file called `README.md`.


Before You Begin
----------------

You'll need a C99-compliant C compiler, POSIX make and shell programs, ZLIB 1.2
or later (<https://www.zlib.net>), and GNU TLS 3.0 or later
(<https://www.gnutls.org>) on platforms other than macOS® and Windows®.  Besides
these prerequisites, you'll want Avahi 0.7 or later (<https://www.avahi.org>)
*or* mDNSResponder (<https://opensource.apple.com/source/mDNSResponder/>) for
mDNS/DNS-SD support.

The GNU compiler tools and Bash work well and we have tested the current CUPS
code against several versions of Clang and GCC with excellent results.  The
makefiles used by the project should work with POSIX-compliant versions of
`make`.  We've tested them with GNU make as well as several vendor make
programs.  BSD users should use GNU make (`gmake`) since BSD make is not
POSIX-compliant and does not support the `include` directive.

On a stock Ubuntu install, the following command will install the required
prerequisites:

    sudo apt-get install autoconf build-essential libavahi-client-dev \
         libnss-mdns libpng-dev libssl-dev zlib1g-dev


Configuration
-------------

The CUPS library uses GNU autoconf, so you will find the usual "configure"
script in the main source directory.  To configure libcups for your system,
type:

    ./configure

The default installation will put the CUPS library under the `/usr/local`
directory on your system.  Use the `--prefix` option to install the CUPS library
in another location:

    ./configure --prefix=/SOME/DIRECTORY

To see a complete list of configuration options, use the `--help` option:

    ./configure --help

The `--enable-debug` option compiles CUPS with debugging information and logging
enabled.  Debug logging is enabled using the `CUPS_DEBUG_xxx` environment
variables at run-time - see "Enabling Debug Logging" below.

Once you have configured things, just type the following to build the software:

    make


Enabling Debug Logging
----------------------

When configured with the `--enable-debug-printfs` option, libcups includes
additional debug logging support.  The following environment variables are used
to enable and control debug logging:

- `CUPS_DEBUG_FILTER`: Specifies a POSIX regular expression to control which
  messages are logged.
- `CUPS_DEBUG_LEVEL`: Specifies a number from 0 to 9 to control the verbosity of
  the logging. The default level is 1.
- `CUPS_DEBUG_LOG`: Specifies a log file to use.  Specify the name "-" to send
  the messages to stderr.  Prefix a filename with "+" to append to an existing
  file.  You can include a single "%d" in the filename to embed the current
  process ID.


Testing the Software
--------------------

Aside from the built-in unit tests, libcups includes an automated test framework
for the client/server support.  Type the following to run the unit tests and
test suite:

    make test


Installing the Software
-----------------------

Once you have built the software you need to install it.  The "install" target
provides a quick way to install the software on your local system:

    make install

Use the `BUILDROOT` variable to install to an alternate root directory:

    make BUILDROOT=/some/other/root/directory install
