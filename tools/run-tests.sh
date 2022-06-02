#!/bin/sh
#
# Script to test ippfind, ipptool, and ippeveprinter end-to-end.
#
# Copyright © 2022 by Open Printing.
#
# Licensed under Apache License v2.0.  See the file "LICENSE" for more
# information.
#

# Run ippeveprinter to provide an endpoint for testing...
name="Test Printer $(date +%H%M%S)"
status=0

echo "Running ippeveprinter..."
./ippeveprinter-static -vvv -a test.conf "$name" 2>test.log &
ippeveprinter=$!

# Test the instance...
echo "Running ippfind + ipptool..."
./ippfind-static -T 5 --literal-name "$name" --exec ./ipptool-static -tI '{}' ../examples/ipp-2.0.test \; || status=1

# Clean up
kill $ippeveprinter

exit $status
