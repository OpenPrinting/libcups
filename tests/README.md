Test helper: cupsArrayDup

Build and run (POSIX shell or MSYS2/WSL):

```bash
cd path/to/libcups
# build the project first so headers and deps are available
./configure
make -j

# then compile the test (from the project root)
gcc -I. tests/test_array_dup.c cups/array.c -o tests/test_array_dup

# run
./tests/test_array_dup
```

On Windows with MSVC, compile the project with Visual Studio and link the test
against the built library, or run the test from within the project build.
