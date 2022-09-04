include_directories( .. ../utils )

add_executable(rbtree_load rbtree_load.c rbtree.c tpl.c)
target_link_libraries(rbtree_load pm rt)

add_executable(rbtree_exec rbtree_exec.c rbtree.c distro.c tpl.c)
target_link_libraries(rbtree_exec pm rt)

add_executable(rbtree_print rbtree_print.c rbtree.c)
target_link_libraries(rbtree_print pm)

add_executable(slist_load slist_load.c tpl.c)
target_link_libraries(slist_load pm rt)

add_executable(slist_exec slist_exec.c distro.c tpl.c)
target_link_libraries(slist_exec pm rt)

add_executable(slist_print slist_print.c)
target_link_libraries(slist_print pm rt)
