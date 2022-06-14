Migrating CUPS 2.x Code to the CUPS Library v3 (libcups)
========================================================

The CUPS Library v3 (libcups) removes all of the deprecated and obsolete APIs
from CUPS 2.x and earlier and makes other naming changes for consistency.  As a
result, the CUPS 3.x library is no longer binary compatible with programs
compiled against the CUPS 2.x library and earlier, and some source code may need
minor changes to compile with the new library.  This file describes the changes
and how to migrate to the new library.


General Changes
---------------

The following general changes have been made to the CUPS API:

- Boolean values now use the C99 `bool` type.
- Counts, indices, and lengths now use the `size_t` type - this affects the
  array, HTTP, IPP, and option APIs in particular.
- Accessor functions have been renamed (as necessary) to use the "get" and "set"
  verbs, for example `cupsServer` is now `cupsGetServer` and `httpEncryption` is
  now `httpSetEncryption`.


Removed Functions
-----------------

The following CUPS 2.x API functions have been removed from the CUPS library:

- Old class/printer functions: `cupsGetClasses` and `cupsGetPrinters`.
- HTTP functions: `httpConnect2`, `httpConnectEncrypt`, `httpDecode64_2`, and
  `httpEncode64_2`.
- PPD file functions: `ppdClose`, `ppdCollect`, `ppdCollect2`, `ppdConflicts`,
  `ppdEmit`, `ppdEmitAfterOrder`, `ppdEmitFd`, `ppdEmitJCL`, `ppdEmitJCLEnd`,
  `ppdEmitString`, `ppdErrorString`, `ppdFindAttr`, `ppdFindChoice`,
  `ppdFindCustomOption`, `ppdFindCustomParam`, `ppdFindMarkedChoice`,
  `ppdFindNextAttr`, `ppdFindOption`, `ppdFirstCustomParam`, `ppdFirstOption`,
  `ppdInstallableConflict`, `ppdIsMarked`, `ppdLastError`, `ppdLocalize`,
  `ppdLocalizeAttr`, `ppdLocalizeIPPReason`, `ppdLocalizeMarkerName`,
  `ppdMarkDefaults`, `ppdMarkOption`, `ppdNextCustomParam`, `ppdNextOption`,
  `ppdOpen`, `ppdOpen2`, `ppdOpenFd`, `ppdOpenFile`, `ppdPageLength`,
  `ppdPageSize`, `ppdPageSizeLimits`, `ppdPageWidth`, and `ppdSetConformance`.
- PPD helper functions: `cupsGetConflicts`, `cupsGetPPD`, `cupsGetPPD2`,
  `cupsGetPPD3`, `cupsGetServerPPD`, `cupsMarkOptions`,
  `cupsRasterInterpretPPD`, and `cupsResolveConflicts`.
- Deprecated functions: `cupsTempFile`.
- Non-destination print functions: `cupsCancelJob`, `cupsCancelJob2`,
  `cupsCreateJob`, `cupsCloseJob`, `cupsFinishDocument`, `cupsGetDefault`,
  `cupsGetDefault2`, `cupsPrintFile`, `cupsPrintFile2`, `cupsPrintFiles`,
  `cupsPrintFiles2`, and `cupsSendDocument`.
- Array functions: `cupsArrayNew2` and `cupsArrayNew3`.
- Raster functions: `cupsRasterReadHeader2` and `cupsRasterWriteHeader2`.


Renamed Functions
-----------------

| Old CUPS 2.x Name        | New CUPS 3.0 Name       |
|--------------------------|-------------------------|
| `cupsArrayCount`         | `cupsArrayGetCount`     |
| `cupsArrayFirst`         | `cupsArrayGetFirst`     |
| `cupsArrayIndex`         | `cupsArrayGetElement`   |
| `cupsArrayLast`          | `cupsArrayGetLast`      |
| `cupsArrayNew3`          | `cupsArrayNew`          |
| `cupsArrayNext`          | `cupsArrayGetNext`      |
| `cupsArrayPrev`          | `cupsArrayGetPrev`      |
| `cupsEncryption`         | `cupsGetEncryption`     |
| `cupsFileCompression`    | `cupsFileIsCompressed`  |
| `cupsGetDests2`          | `cupsGetDests`          |
| `cupsGetPassword2`       | `cupsGetPassword`       |
| `cupsRasterReadHeader2`  | `cupsRasterReadHeader`  |
| `cupsRasterWriteHeader2` | `cupsRasterWriteHeader` |
| `cupsServer`             | `cupsGetServer`         |
| `cupsSetPasswordCB2`     | `cupsSetPasswordCB`     |
| `cupsTempFile2`          | `cupsTempFile`          |
| `cupsUser`               | `cupsGetUser`           |
| `cupsUserAgent`          | `cupsGetUserAgent`      |
| `httpAddrAny`            | `httpAddrIsAny`         |
| `httpAddrEqual`          | `httpAddrIsEqual`       |
| `httpAddrFamily`         | `httpAddrGetFamily`     |
| `httpAddrLength`         | `httpAddrGetLength`     |
| `httpAddrLocalhost`      | `httpAddrIsLocalhost`   |
| `httpAddrPort`           | `httpAddrGetPort`       |
| `httpAddrString`         | `httpAddrGetString`     |
| `httpBlocking`           | `httpSetBlocking`       |
| `httpConnect2`           | `httpConnect`           |
| `httpDecode64_2`         | `httpDecode64`          |
| `httpDelete`             | `httpWriteRequest`      |
| `httpEncode64_2`         | `httpEncode64`          |
| `httpEncryption`         | `httpSetEncryption`     |
| `httpGet`                | `httpWriteRequest`      |
| `httpGetDateString2`     | `httpGetDateString`     |
| `httpGetLength2`         | `httpGetLength`         |
| `httpOptions`            | `httpWriteRequest`      |
| `httpPost`               | `httpWriteRequest`      |
| `httpPut`                | `httpWriteRequest`      |
| `httpRead2`              | `httpRead`              |
| `httpReconnect2`         | `httpReconnect`         |
| `httpStatus`             | `httpStatusString`      |
| `httpTrace`              | `httpWriteRequest`      |
| `httpWrite2`             | `httpWrite`             |
| `ippFirstAttribute`      | `ippGetFirstAttribute`  |
| `ippNextAttribute`       | `ippGetNextAttribute`   |
| `ippPort`                | `ippGetPort`            |


Renamed Types
-------------

| Old Name              | New Name             |
|-----------------------|----------------------|
| `cups_acopy_func_t`   | `cups_acopy_cb_t`    |
| `cups_afree_func_t`   | `cups_afree_cb_t`    |
| `cups_array_func_t`   | `cups_array_cb_t`    |
| `cups_mode_t`         | `cups_raster_mode_t` |
| `cups_raster_iocb_t`  | `cups_raster_cb_t`   |
| `cups_password_cb2_t` | `cups_password_cb_t` |
| `cups_page_header2_t` | `cups_page_header_t` |
| `ipp_copycb_t`        | `ipp_copy_cb_t`      |
| `ipp_iocb_t`          | `ipp_io_cb_t`        |


API Changes
-----------

- `httpGets` now has the `http_t` pointer as the first argument.
- The `cups_size_t` structure now includes `source` and `type` members to allow
  specification of media source (input tray/roll) and type.
