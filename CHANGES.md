Changes in libcups
==================

libcups v3.0b1 (Month DD, YYYY)
-------------------------------

- Split out libcups and tools from CUPS 2.4.0
- Simplified the configure script
- Now require a C99-capable C compiler
- ZLIB is now required
- GNU TLS or platform TLS are now required
- The `poll` function (`WSAPoll` on Windows) is now required
- Threading support is now required
- Removed (obsolete) Kerberos support
- Added new `GENERATE-FILE` directive for `ipptool` test files
