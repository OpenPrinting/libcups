Changes in libcups
==================

libcups v3.0b1 (Month DD, YYYY)
-------------------------------

- Split out libcups and tools from CUPS 2.4.0.
- Simplified the configure script.
- Now require a C99-capable C compiler.
- Now require GNU TLS, LibreSSL, or OpenSSL.
- Now require ZLIB.
- Now require POSIX or Windows threading support.
- Now require the `poll` function (`WSAPoll` on Windows).
- Added new `GENERATE-FILE` directive for `ipptool` test files.
- Added new `ATTR-IF-DEFINED` and `ATTR-IF-NOT-DEFINED` directives to IPP data
  files (Issue #3)
- Added `ippFile` API for working with IPP data files as used by `ipptool`,
  `ippexeprinter`, and other tools (Issue #14)
- Updated the CUPS API for consistency.
- Removed all obsolete/deprecated CUPS 2.x APIs.
- Removed (obsolete) Kerberos support.
- Removed support for SecureTransport (macOS) due to deprecation of the platform
  API.
- Removed support for SChannel (Windows) due to restrictions on its use in
  domain accounts.
