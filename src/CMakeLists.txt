cmake_minimum_required(VERSION 2.8)

project(pm)
enable_testing()

set(CMAKE_C_FLAGS "-std=gnu99")

include_directories(. utils tests)

add_subdirectory(tests)
add_subdirectory(benchmarks)
add_subdirectory(sqlite)

set(SOURCE_FILES
	cont.c
    nlmapper.c
	slab.c
    checkpoint.c
    snapshot.c
    restore.c
    alloc.c
	page_alloc.c
	sfhandler.c
	fixptr.c
    fixmapper.c
    atomics.c
    closure.c
    hash.c
    htable.c
    debug.c
    out.c
    stats.c
)

add_library(pm STATIC ${SOURCE_FILES})

