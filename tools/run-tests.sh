#!/bin/sh
#
# Script to test ippfind, ipptool, and ippeveprinter end-to-end.
#
# Copyright © 2022-2023 by OpenPrinting.
#
# Licensed under Apache License v2.0.  See the file "LICENSE" for more
# information.
#

# Support running with shared libraries as necessary on macOS and Linux...
cd ..
DYLD_LIBRARY_PATH="$(pwd)/cups"; export DYLD_LIBRARY_PATH
LD_LIBRARY_PATH="$(pwd)/cups"; export LD_LIBRARY_PATH

# Run ippeveprinter to provide an endpoint for testing...
name="Test Printer $(date +%H%M%S)"
status=0

echo "Running ippeveprinter..."
CUPS_DEBUG_LOG=test-cups.log CUPS_DEBUG_LEVEL=4 CUPS_DEBUG_FILTER='^(http|_http|ipp|_ipp|cupsDNSSD|cupsDo|cupsGet|cupsSend)' tools/ippeveprinter -vvv -a tools/test.conf "$name" 2>tools/test.log &
ippeveprinter=$!

# Test the instance...
echo "Running ippfind + ipptool..."
tools/ippfind -T 30 --literal-name "$name" --exec tools/ipptool -V 2.0 -tIf examples/document-letter.pdf '{}' examples/ipp-2.0.test \; || status=1

if test $status = 1; then
    echo "Unable to find test printer."
fi

# Clean up
kill $ippeveprinter

exit $status
