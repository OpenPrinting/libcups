# Script to run unit tests
#
# Usage:
#
#   .\runtests.ps1 x64\{Debug|Release}
#

param(
[string]$path
)

# Copy DLLs for dependent packages to the debug directory...
copy ..\pdfio\packages\libpng_native.redist.1.6.30\build\native\bin\$path\*.dll $path
copy packages\libressl_native.redist.4.0.0\build\native\bin\$path\*.dll $path
copy packages\zlib_native.redist.1.2.11\build\native\bin\$path\*.dll $path

# Run tests from the build directory...
cd $path

.\testdnssd.exe
.\testfile.exe
.\testhttp.exe

#$env:CUPS_DEBUG_LOG="test-debug%d.log"
#$env:CUPS_DEBUG_LEVEL="4"
#$env:CUPS_DEBUG_FILTER="^(http|_http|ipp|_ipp|cupsDNSSD|cupsCreate|cupsDo|cupsGet|cupsSend)"

#Start-Process -FilePath ".\ippeveprinter.exe" -ArgumentList "-vvv","-a","../../../tools/test.conf","-L","test.log","'Test Printer'"

#.\ippfind.exe -T 30 --literal-name "Test Printer" --exec ipptool.exe -V 2.0 -tIf ../../../examples/document-letter.pdf '{}' ../../../examples/ipp-2.0.test ../../../examples/pwg5100.1.test ../../../examples/pwg5100.2.test ../../../examples/pwg5100.7.test ../../../examples/pwg5100.13.test ";"

#Stop-Process -Name .\ippeveprinter.exe

cd ..\..
