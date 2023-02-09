The CUPS Library v3 (libcups)
=============================

![Version](https://img.shields.io/github/v/release/michaelrsweet/libcups?include_prereleases)
![Apache 2.0](https://img.shields.io/github/license/michaelrsweet/libcups)
[![Build and Test](https://github.com/michaelrsweet/libcups/workflows/Build%20and%20Test/badge.svg)](https://github.com/michaelrsweet/libcups/actions/workflows/build.yml)
[![Coverity Scan](https://img.shields.io/coverity/scan/24180)](https://scan.coverity.com/projects/michaelrsweet-libcups)

> *Note:* This is a major release update of the CUPS library that breaks both
> binary and source compatibility with prior releases of CUPS.

The CUPS library (libcups) provides a common C API for HTTP/HTTPS and IPP
communications on Unix®-like operating systems and Microsoft Windows®.  It is
used by many printing-related projects such as [CUPS][1] and [PAPPL][2].  This
project is part of OpenPrinting's CUPS 3.0 development, which will provide a
100% driverless printing system for Unix®-life operating systems.

This version of the CUPS library removes all of the deprecated and obsolete APIs
from CUPS 2.x and earlier and is *not* binary compatible with older releases.
See the file `MIGRATING.md` for a description of the changes and how to migrate
your code to the new library.

The CUPS library is licensed under the Apache License Version 2.0 with an
exception to allow linking against GNU GPL2-only software.  See the files
`LICENSE` and `NOTICE` for more information.


Reading the Documentation
-------------------------

Initial documentation to get you started is provided in the root directory of
the CUPS sources:

- `CHANGES.md`: A list of changes for each release of libcups.
- `CONTRIBUTING.md`: Guidelines for contributing to the CUPS project.
- `CREDITS.md`: A list of past contributors to the CUPS project.
- `DEVELOPING.md`: Guidelines for developing code for the CUPS project.
- `INSTALL.md`: Instructions for building and installing the CUPS library.
- `LICENSE`: The CUPS license agreement (Apache 2.0).
- `MIGRATING.md`: Guidance on migrating CUPS 2.x and earlier code to the new
  CUPS library.
- `NOTICE`: Copyright notices and exceptions to the CUPS license agreement.
- `README.md`: This file.

You will find the CUPS Programing Guide in HTML and EPUB formats as well as man
pages in the `doc` directory.

*Please read the documentation before asking questions.*


Legal Stuff
-----------

Copyright © 2020-2023 by OpenPrinting.

Copyright © 2007-2020 by Apple Inc.

Copyright © 1997-2007 by Easy Software Products.

CUPS is provided under the terms of the Apache License, Version 2.0 with
exceptions for GPL2/LGPL2 software.  A copy of this license can be found in the
file `LICENSE`.  Additional legal information is provided in the file `NOTICE`.

Unless required by applicable law or agreed to in writing, software distributed
under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
CONDITIONS OF ANY KIND, either express or implied.  See the License for the
specific language governing permissions and limitations under the License.


[1]: https://openprinting.github.io/cups
[2]: https://www.msweet.org/pappl
