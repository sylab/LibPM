#TODO: look into the meaning of the project variable to avoid
# defining the same things multitple times

include_directories( .. ../utils )

set (SIMPLE_TESTS
    test_cont
    test_restore
    test_full_restore
    test_fmapper
    test_pptr
    test_crash_recovery
    test_cpoint_overhead
    test_closure
)

foreach( test_target ${SIMPLE_TESTS} )
    set(test_source_file "${test_target}.c")
    add_executable( ${test_target} ${test_source_file} )
    target_link_libraries( ${test_target} pm rt)
    add_test(${test_target}  ${test_target} )
endforeach(test_target)

add_executable(perf_rbtree perf_rbtree.c tpl.c)
target_link_libraries(perf_rbtree pm rt)
#add_test(perf_rbtree perf_rbtree)
