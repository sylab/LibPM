Required Software
=================

    * CMake: CMake is cross-platform free and open-source software for managing
        the build process of software using a compiler-independent method.

How to compile the library
==========================

First, get a copy of the source. Assuming that we store the path to the code
folder in a variable called PMLIB_TOP, we compile the code by issuing the
following commands:

    $ cd $PMLIB_TOP

Now we create a folder in which we will compile the code. This is convenient
because it separates the binaries from the source.

    $ mkdir build
    $ cd build
    $ cmake ../src
    $ make

If everything went well, we should have a libpm.a file in the build folder
along with other test files in $PMLIB_TOP/build/tests

NOTE: If you want to debug the code, you should consider compiling it with
debugging symbols. This can be done by telling cmake that we want a Debug build
type like this.

    $ cmake -DCMAKE_BUILD_TYPE=Debug ../src

Testing
=======

The folder tests contains simple programs that test specific functionalities
of the library.

Benchmark
=========

The benchmark folder contains benchmarks use to tests the performance of the
library. As of now, the only comparison point we use is TPL.

Settings
========

The library provides both logging and stats. These could be very helpful when
debugging the code but it can also add a significant overhead. Both logging and
stats can be disabled by updating the settings.h file.

Environment Variables
=====================

The following environment variables could be set to change the default behavior
of the library.

    PMLIB_CLOSURE=0     Disables the closure computation. (Defaults 1)

    PMLIB_FIX_PTRS=0    No metadata about persistent pointers is store and the
                        pointers are not fixed after a container restore. This
                        only applies when we can guarantee that the container
                        was mmap at the same address on which is was created.
                        (Defaults 1)

    PMLIB_LOG_LEVEL=n   Sets the log level to n. Msg(s) with log-level bigger
                        than n will not be logged.

    PMLIB_LOG_FILE=/path/to/file
                        Define the log file

    PMLIB_INIT_SIZE=n   Initializes the container file to n bytes.

    PMLIB_CONT_FILE=/path/to/file
                        Create container at the given path

    PMLIB_USE_FMAPPER=1 Use Fixed Mapper page allocator (default)

    PMLIB_USE_NLMAPPER=1
                        Use Non-Linear Mapper page allocator


Running tests with large containers
===================================

The current design maps portions of the backend-file into the process address
space. When large containers are used, it's possible to exceed the default
maximum number of mmap regions per process. To solve this problem we change
this limit following these instructions.

    To check the current value

        $ sysctl  vm.max_map_count

    To change the value to n

        # sysctl -w vm.max_map_count=n

