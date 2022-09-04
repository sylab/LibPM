#!/bin/bash

set -u
set -e
#set -x

#container=/mnt/pmfs/container0
container=/tmp/container0
container_backup=${container}_backup

#tpl_file=/mnt/pmfs/rbtree.tpl
tpl_file=/tmp/rbtree.tpl
tpl_file_backup=${tpl_file}_backup

n_load=100000
n_exec=100000

n_load=10000
n_exec=10000

BUILD_FOLDER=../build

rm -f $container $container_backup

echo TPL Loading rbtree with consistent checkpoints ...
$BUILD_FOLDER/benchmarks/rbtree_load -t -n $n_load -c

for w in a b c; do
    echo TPL workload $w ===========================================
    rm -f $container $container_backup
    $BUILD_FOLDER/benchmarks/rbtree_load -t -n $n_load
    cp $tpl_file $tpl_file_backup
    $BUILD_FOLDER/benchmarks/rbtree_exec -t -n $n_exec -w $w
    echo "-----------------------------------------------"
    mv $tpl_file_backup $tpl_file
    $BUILD_FOLDER/benchmarks/rbtree_exec -t -n $n_exec -w $w -c
done

echo "#########################################################################"

echo PMLib Loading rbtree with volatile allocations...
rm -f $container $container_backup
$BUILD_FOLDER/benchmarks/rbtree_load -p -m -n $n_load

echo PMLib Loading rbtree with volatile allocations and consistent checkpoints...
rm -f $container $container_backup
$BUILD_FOLDER/benchmarks/rbtree_load -p -m -n $n_load -c

echo PMLib Loading rbtree with consistent checkpoints ...
rm -f $container $container_backup
$BUILD_FOLDER/benchmarks/rbtree_load -p -n $n_load -c

for w in a b c; do
    echo PMLib workload $w ===========================================
    rm -f $container $container_backup
    $BUILD_FOLDER/benchmarks/rbtree_load -p -n $n_load
    cp $container $container_backup
    $BUILD_FOLDER/benchmarks/rbtree_exec -p -n $n_exec -w $w
    echo "-----------------------------------------------"
    mv $container_backup $container
    $BUILD_FOLDER/benchmarks/rbtree_exec -p -n $n_exec -w $w -c
done

