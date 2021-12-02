Migrating CUPS 2.x Code to the CUPS Library (libcups)
=====================================================

The CUPS Library (libcups) removes all of the deprecated and obsolete APIs from
CUPS 2.x and earlier and makes other naming changes for consistency.  As a
result, the CUPS 3.x library is no longer binary compatible with programs
compiled against the CUPS 2.x library and earlier, and some source code may need
minor changes to compile with the new library.  This file describes the changes
and how to migrate to the new library.


Removed Functions
-----------------

The following CUPS 2.x API functions have been removed from the CUPS library:

- Old class/printer functions: `cupsGetClasses` and `cupsGetPrinters`.
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
  `cupsCreateJob`, `cupsCloseJob`, `cupsFinishDocument`, `cupsGetDefault`, `cupsGetDefault2`,
  `cupsPrintFile`, `cupsPrintFile2`, `cupsPrintFiles`, `cupsPrintFiles2`,
  `cupsSendDocument`


Renamed Functions
-----------------

| Old Name | New Name |
+----------+----------+
| `cupsEncryption`     | `cupsGetEncryption` |
| `cupsTempFile2`      | `cupsTempFile`      |
| `cupsGetDests2`      | `cupsGetDests`      |
| `cupsServer`         | `cupsGetServer`     |
| `cupsGetPassword2`   | `cupsGetPassword`   |
| `cupsUser`           | `cupsGetUser`       |
| `cupsUserAgent`      | `cupsGetUserAgent`  |
| `cupsSetPasswordCB2` | `cupsSetPasswordCB` |
