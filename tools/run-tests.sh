#!/bin/sh
#
# Script to test ippfind, ipptool, and ippeveprinter end-to-end.
#
# Copyright © 2022 by OpenPrinting.
#
# Licensed under Apache License v2.0.  See the file "LICENSE" for more
# information.
#

# Run ippeveprinter to provide an endpoint for testing...
name="Test Printer $(date +%H%M%S)"
status=0

echo "Running ippeveprinter..."
CUPS_DEBUG_LOG=test-cups.log CUPS_DEBUG_LEVEL=4 CUPS_DEBUG_FILTER='^(http|_http|ipp|_ipp|cupsDo|cupsGet|cupsSend)' ./ippeveprinter-static -vvv -a test.conf "$name" 2>test.log &
ippeveprinter=$!

# Test the instance...
echo "Running ippfind + ipptool..."
./ippfind-static -T 5 --literal-name "$name" --exec ./ipptool-static -V 2.0 -tIf ../examples/document-letter.pdf '{}' ../examples/ipp-2.0.test \; || status=1

# Clean up
kill $ippeveprinter

exit $status
