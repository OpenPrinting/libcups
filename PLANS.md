Plans for libcups Repository
============================

Cleanup:

- Simplified configure script
- Remove all deprecated/obsolete APIs (including PPD APIs and Kerberos support)
- Remove all of the shadow entries in the http_t structure
- Normalize function/structure naming
- Bump DSO version
- Use `bool` type for boolean values


New Prerequisites:

- C99
- poll function (WSAPoll on Windows)
- POSIX threads (emulation on Windows)
- ZLIB 1.1.x or later (with inflateCopy function)


Feature work:

- Transition to OpenSSL API for TLS support
- Need API for getting/setting TLS certificates (something to share certs between printer
  applications and potentially users)
- Better localization support
- Add PAPPL random number functions
- Add mOAuth JSON functions
- Updated/improved test suite and fuzzing support
- Public threading API
