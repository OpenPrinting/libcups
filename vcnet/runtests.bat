:: Script to run unit tests
::
:: Usage:
::
::   .\runtests.bat x64\{Debug|Release}
::

:: Copy DLLs for dependent packages to the debug directory...
copy packages\*\build\native\bin\x64\Debug\*.dll %1
copy packages\*\build\native\bin\x64\Release\*.dll %1
copy ..\pdfio\packages\*\build\native\bin\x64\Debug\*.dll %1
copy ..\pdfio\packages\*\build\native\bin\x64\Release\*.dll %1

;; Run tests from the build directory...
cd %1

.\testdnssd.exe
.\testfile.exe
.\testhttp.exe

start "" cmd /c ".\ippeveprinter.exe -vvv -a ../../../tools/test.conf -n localhost 'Test Printer' >test.log"

.\ippfind.exe -T 30 --literal-name "Test Printer" --exec ipptool.exe -V 2.0 -tIf ../../../examples/document-letter.pdf '{}' ../../../examples/ipp-2.0.test \;

:: taskkill /im ippeveprinter.exe
