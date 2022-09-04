include_directories( .. ../utils )

add_executable(shell shell.c sqlite3.c)
target_link_libraries(shell pthread dl pm)

add_executable(exec exec.c sqlite3.c benchmarks.c int2words.c)
target_link_libraries(exec pthread dl rt pm)

add_executable(exec_vanilla exec.c sqlite3_vanilla.c benchmarks.c int2words.c)
target_link_libraries(exec_vanilla pthread dl rt pm)
