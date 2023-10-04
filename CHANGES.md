Changes in libcups
==================

libcups v3.0b2 (TBD)
--------------------

- Added the `ipptransform` command to replace/upgrade the `ippevepcl` and
  `ippeveps` commands (Issue #65)
- Added `cupsFormDecode` and `cupsFormEncode` APIs (Issue #49)
- Added `cupsJWT` APIs to support JSON Web Tokens (Issue #50, Issue #52)
- Added `ippAddCredentialsString` and `ippCopyCredentialsString` APIs
  (Issue #58)
- Added `cupsCreateCredentialsRequest` and `cupsSignCredentialsRequest` APIs and
  updated `cupsCreateCredentials` API to better support X.509 certificates
  (Issue #59)
- Updated the configure script to add `_FORTIFY_SOURCE=3` (previous level was 2)
  when not using address sanitizer and when it hasn't already been added
  (Issue #51)
- Updated the `httpAddrListen` function to use the maximum backlog value.
- Fixed ipptool limit on the size of an attribute value that would be printed
  (Issue #5)
- Fixed some configure script issues (Issue #48)
- Fixed JSON output bug in ipptool.
- Fixed `CUPS_DNSSD_IF_INDEX_LOCAL` when using Avahi.


libcups v3.0b1 (February 9, 2023)
---------------------------------

- Documentation updates (Issue #32)
- Split out libcups and tools from CUPS 2.x.
- Simplified the configure script.
- Now require a C99-capable C compiler.
- Now require GNU TLS, LibreSSL, or OpenSSL.
- Now require ZLIB.
- Now require POSIX or Windows threading support.
- Now require the `poll` function (`WSAPoll` on Windows).
- Now install with a prefix by default to allow coexistance with CUPS 2.x
  (Issue #21)
- Added new `GENERATE-FILE` directive for `ipptool` test files.
- Added new `ATTR-IF-DEFINED` and `ATTR-IF-NOT-DEFINED` directives to IPP data
  files (Issue #3)
- Added `ippFile` API for working with IPP data files as used by `ipptool`,
  `ippexeprinter`, and other tools (Issue #14)
- Added a roll to the default color ippeveprinter printer.
- Added new DNS-SD API (Issue #19)
- Added new PWG media sizes.
- Added new `WITH-ALL-VALUES-FROM` predicate for ipptool files (Issue #20)
- Added new `SAVE-ALL-CONTENT`, `WITH-ALL-CONTENT`, and `WITH-ALL-MIME-TYPES`
  predicates for ipptool files (Issue #22)
- Added, modernized, and promoted the localization interfaces to public API
  (Issue #24)
- Added public JSON API (Issue #31)
- Added new `basename` variable for use in ipptool files (Issue #44)
- Updated the CUPS API for consistency.
- Fixed ipptool's handling of EXPECT for member attributes (Issue #4)
- Fixed ipptool's support for octetString values (Issue #23)
- Removed all obsolete/deprecated CUPS 2.x APIs.
- Removed (obsolete) Kerberos support.
- Removed support for SecureTransport (macOS) due to deprecation of the platform
  API.
- Removed support for SChannel (Windows) due to restrictions on its use in
  domain accounts.
