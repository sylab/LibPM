#!/bin/bash

set -e
set -u

CONTAINER_LOCATION=/mnt/pmfs/container0
CONTAINER_LOCATION=/tmp/container0
TERM=xterm
export LD_LIBRARY_PATH=.

# extending the max number of memory maps allowed by the kernel 4 GB
sysctl -w vm.max_map_count=1048576
#disable virtual address randomization
echo 0 | tee /proc/sys/kernel/randomize_va_space

modifier=""

if [ $# == 1 ]; then
    if [ $1 == "gdb" ]; then
        #modifier="gdbtui --args"
        modifier="gdb --args"
    elif [ $1 == "cgdb" ]; then
        modifier="cgdb --args"
    elif [ $1 == "ddd" ]; then
        modifier="ddd --args"
    elif [ $1 == "valgrind" ]; then
        modifier="valgrind --leak-check=full --show-reachable=yes"
    fi
fi

#CPOINT_OVERHEAD_NUMOPE=262144
#CPOINT_OVERHEAD_NUMOPE=100000
#CPOINT_OVERHEAD_NUMOPE=15000
CPOINT_OVERHEAD_NUMOPE=65536

function drop_caches()
{
    sudo bash -c "echo 3 > /proc/sys/vm/drop_caches"
}

function remove_container()
{
    if [ -e $CONTAINER_LOCATION ]; then
        rm -rf $CONTAINER_LOCATION
    fi
}

function test_full_restore_overhead()
{
    #NODES=65536 #256 mb of data
    #NODES=131072 #512 mb of data
    NODES=262144 # 1gb of data

    for i in `seq 10`; do
        #running the baseline
        remove_container
        drop_caches
        $modifier ./tests/test_full_restore -n $NODES -b

        #running with softpm with one cpoint at the end
        remove_container
        drop_caches
        $modifier ./tests/test_full_restore -n $NODES -c

        #running with softpm calling cpoint after every update
        remove_container
        drop_caches
        $modifier ./tests/test_full_restore -n $NODES -c -p
    done
}

function test_full_restore_time()
{
    NODES=262144 # 1gb of data

    # vary the size from 32mb to 1gb in powers of 2
    #for node_count in 8192 16384 32768 65536 131072 262144; do
    #for node_count in 262144; do
    for node_count in $NODES; do

        for i in `seq 1`; do
            #running with softpm with one cpoint at the end
            remove_container
            drop_caches
            ./tests/test_full_restore -n $node_count -c
            #$modifier ./tests/test_full_restore -n $node_count -c


            ##running with softpm calling cpoint after every update
            drop_caches
            $modifier ./tests/test_full_restore -r
        done

        ls -l $CONTAINER_LOCATION
    done
}

function test_cpoint_overhead()
{
    NODE_SIZE=1024  # 1KB
    NODES=512000    # a total of 500 MB

    for i in `seq 1`; do
        echo Using malloc as a comparison point
        echo =============================================================
        $modifier ./tests/test_cpoint_overhead -n $NODES -s $NODE_SIZE -b

        echo Just one commit at the end
        echo =============================================================
        remove_container
        $modifier ./tests/test_cpoint_overhead -n $NODES -s $NODE_SIZE -p

        echo Commit after every modification
        echo =============================================================
        remove_container
        $modifier ./tests/test_cpoint_overhead -n $NODES -s $NODE_SIZE -c
    done
}

function test_pptr()
{
    NODES=600
    CPOINT_FREQ=1 #after every operation

    remove_container

    echo Creating a list with $NODES nodes
    $modifier ./tests/test_pptr -f $CPOINT_FREQ -c $NODES

    echo Restoring a list with $NODES nodes
    $modifier ./tests/test_pptr -r $NODES

    echo Restoring a list with $NODES nodes
    $modifier ./tests/test_pptr -r $NODES
}

function virtualized_systems_demo()
{
    for i in `seq 1`; do
        remove_container
        $modifier ./tests/test_crash_recovery -c
        $modifier ./tests/test_crash_recovery -r
    done
}

function perf_rbtree()
{
    NODES=262144 #1GB
    #NODES=131072 # 512 MB
    #NODES=1024

    remove_container

    drop_caches
    $modifier ./tests/perf_rbtree -n $NODES -t -w 50
    drop_caches
    $modifier ./tests/perf_rbtree -t -r

    drop_caches
    $modifier ./tests/perf_rbtree -n $NODES -s -w 50
    drop_caches
    $modifier ./tests/perf_rbtree -s -r

}

#virtualized_systems_demo
#test_full_restore_time
#test_cpoint_overhead
#test_pptr
perf_rbtree
