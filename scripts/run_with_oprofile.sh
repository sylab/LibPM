#!/bin/bash

set -u
set -e
set -x

container=/mnt/pmfs/container0
container_backup=${container}_backup

tpl_file=/mnt/pmfs/slist.tpl
tpl_file_backup=${tpl_file}_backup

n_load=100000
n_exec=100000
app=rbtree_exec
#app=rbtree_load

echo 6553000 > /proc/sys/vm/max_map_count

rm -f $container $container_backup

./rbtree_load  -p -n $n_load

opcontrol --start --no-vmlinux
    ./$app -p -n $n_exec -w a 
opreport -l $app
opcontrol --reset
opcontrol --stop
