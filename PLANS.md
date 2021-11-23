Plans for libcups Repository
============================

Cleanup:

- Simplified configure script
- Remove all deprecated/obsolete APIs (including PPD APIs and Kerberos support)
- Remove all of the shadow entries in the http_t structure
- Normalize function/structure naming
- Bump DSO version

Feature work:

- Transition to OpenSSL API for TLS support
- Need API for getting/setting TLS certificates (something to share certs between printer
  applications and potentially users)
- Better localization support
