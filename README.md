The CUPS Library v3 (libcups)
=============================

![Version](https://img.shields.io/github/v/release/michaelrsweet/libcups?include_prereleases)
![Apache 2.0](https://img.shields.io/github/license/michaelrsweet/libcups)
[![Build and Test](https://github.com/michaelrsweet/libcups/workflows/Build%20and%20Test/badge.svg)](https://github.com/michaelrsweet/libcups/actions/workflows/build.yml)
[![Coverity Scan](https://img.shields.io/coverity/scan/24180)](https://scan.coverity.com/projects/michaelrsweet-libcups)

> *Note:* This is a major release update of the CUPS library that breaks both
> binary and source compatibility with the 1.x and 2.x releases of CUPS.

The CUPS library (libcups) provides a common C API for HTTP/HTTPS and IPP
communications on Unix®-like operating systems and Microsoft Windows®.  It is
used by many printing-related projects such as [CUPS][CUPS] and [PAPPL][PAPPL].
This project is part of OpenPrinting's CUPS 3.0 development, which will provide
a 100% driverless printing system for Unix®-like operating systems.

This version of the CUPS library removes all of the deprecated and obsolete APIs
from CUPS 2.x and earlier and is *not* binary compatible with older releases.
See the file `doc/cupspm.html` for a description of the changes and how to
migrate your code to the new library.

The CUPS library is licensed under the Apache License Version 2.0 with an
(optional) exception to allow linking against GNU GPL2-only software.  See the
files `LICENSE` and `NOTICE` for more information.


Getting the Code
----------------

*Do not use the ZIP file available via the Github "Code" button on the main*
*project page, as that archive is missing the PDFio submodule and will not*
*compile.*

The source code is available in release tarballs or via the Github repository.
For a release tarball, run the following commands:

    tar xvzf libcups-VERSION.tar.gz
    cd libcups-VERSION

Similarly, the release ZIP file can be extracted with the following commands:

    unzip libcups-VERSION.zip
    cd libcups-VERSION

From the Github sources, clone the repository with the `--recurse-submodules`
option *or* use the `git submodule` commands:

    git clone --recurse-submodules git@github.com:OpenPrinting/libcups.git
    cd libcups

    git clone git@github.com:OpenPrinting/libcups.git
    cd libcups
    git submodule update --init --recursive

To update an already-cloned repository:

    git pull
    git submodule update --init --recursive


Reading the Documentation
-------------------------

Initial documentation to get you started is provided in the root directory of
the CUPS sources:

- `CHANGES.md`: A list of changes for each release of libcups.
- `CODE_OF_CONDUCT.md`: Code of conduct for the project.
- `CONTRIBUTING.md`: Guidelines for contributing to the CUPS project.
- `CREDITS.md`: A list of past contributors to the CUPS project.
- `DEVELOPING.md`: Guidelines for developing code for the CUPS project.
- `INSTALL.md`: Instructions for building and installing the CUPS library.
- `LICENSE`: The CUPS license agreement (Apache 2.0).
- `NOTICE`: Copyright notices and exceptions to the CUPS license agreement.
- `README.md`: This file.
- `SECURITY.md`: How to report security issues.

You will find the CUPS Programing Guide in HTML and EPUB formats as well as man
pages and other documentation in the `doc` directory.

*Please read the documentation before asking questions.*


Contributing Code and Translations
----------------------------------

Code contributions should be submitted as pull requests on the Github site:

    http://github.com/OpenPrinting/libcups/pulls

See the file `CONTRIBUTING.md` for more details.

CUPS uses [Weblate][WL] to manage the localization of the command-line programs
and common IPP attributes and values, and those likewise end up as pull requests
on Github.

[WL]: https://hosted.weblate.org


Legal Stuff
-----------

Copyright © 2020-2026 by OpenPrinting.

Copyright © 2007-2020 by Apple Inc.

Copyright © 1997-2007 by Easy Software Products.

CUPS is provided under the terms of the Apache License, Version 2.0 with
(optional) exceptions for GPL2/LGPL2 software.  A copy of this license can be
found in the file `LICENSE`.  Additional legal information is provided in the
file `NOTICE`.

Unless required by applicable law or agreed to in writing, software distributed
under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
CONDITIONS OF ANY KIND, either express or implied.  See the License for the
specific language governing permissions and limitations under the License.


[CUPS]: https://openprinting.github.io/cups
[PAPPL]: https://www.msweet.org/pappl
