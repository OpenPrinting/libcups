#!/bin/sh
#
# Test ipptransform directly and through a virtual IPP Everywhere printer.
#
# Copyright © 2026 by OpenPrinting.
#
# Licensed under Apache License v2.0.  See the file "LICENSE" for more
# information.
#

cd .. || exit 1

name="8-Up Test Printer $(date +%H%M%S)-$$"
status=0
testdir="${TMPDIR:-/tmp}/libcups-8up-$$"
printer=""

cleanup()
{
    if test -n "$printer"; then
        kill "$printer" 2>/dev/null
        wait "$printer" 2>/dev/null
    fi

    rm -rf "$testdir"
}

if ! mkdir "$testdir"; then
    echo "Unable to create 8-up test directory '$testdir'."
    exit 1
fi

trap cleanup 0
trap 'exit 1' 1 2 3 15

unset CONTENT_TYPE OUTPUT_TYPE DEVICE_URI IPP_NUMBER_UP

rm -f tools/test-ipptransform.log

echo "Running ipptransform directly..."
if ! tools/ipptransform-static -v \
    -f "$testdir/direct.pdf" \
    -i application/pdf \
    -m application/pdf \
    -o "number-up=8" \
    examples/testfile.pdf >"$testdir/direct.log" 2>&1; then
    echo "Direct 8-up conversion failed."
    status=1
fi

if ! test -s "$testdir/direct.pdf"; then
    echo "Direct 8-up conversion did not produce an output document."
    status=1
fi

if ! grep -q "Using page 8 .*cell=8/8, current=0" "$testdir/direct.log" || \
   ! grep -q "Doing full layout of 1 pages." "$testdir/direct.log" || \
   ! grep -q "Page 1, cell 8/8" "$testdir/direct.log"; then
    echo "Direct conversion did not group all eight input pages on one output page."
    status=1
fi

echo "Running ipptransform through ippeveprinter..."
SERVER_LOGLEVEL=debug tools/ippeveprinter-static -vvv \
    -a tools/test-8up.conf \
    -c "$PWD/tools/ipptransform-static" \
    -d "$testdir" \
    -F application/pdf \
    -k \
    -n localhost \
    -L tools/test-ipptransform.log \
    "$name" &
printer=$!

if ! tools/ippfind-static -T 30 "$name" \
    --exec tools/ipptool-static -V 2.0 -tf examples/testfile.pdf \
    '{}' tools/test-8up.test \;
then
    echo "Unable to test the virtual 8-up printer."
    status=1
fi

if ! grep -q "number-up (integer) 8" tools/test-ipptransform.log; then
    echo "8-up job attribute was not received by ippeveprinter."
    status=1
fi

if ! grep -q "Using page 8 .*cell=8/8, current=0" tools/test-ipptransform.log; then
    echo "ipptransform did not group all eight input pages on one output page."
    status=1
fi

if ! grep -q "Doing full layout of 1 pages." tools/test-ipptransform.log || \
   ! grep -q "Page 1, cell 8/8" tools/test-ipptransform.log; then
    echo "ipptransform did not produce the expected 8-up layout."
    status=1
fi

outputs=0
for file in "$testdir"/*.prn; do
    if test -f "$file"; then
        outputs=$((outputs + 1))

        if ! test -s "$file"; then
            echo "ipptransform produced an empty output document."
            status=1
        fi
    fi
done

if test "$outputs" -ne 1; then
    echo "Expected one output document, found $outputs."
    status=1
fi

exit "$status"
