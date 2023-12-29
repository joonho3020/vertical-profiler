#!/bin/bash


set -ex

CURDIR=$(pwd)

TESTDIR=$CURDIR/../test-io/
TRACEDIR=$CURDIR/../prof/builddir/out

TRACEFILE=$TRACEDIR/SPIKETRACE-0010430
DWARFFILE=$TESTDIR/test-binaries/linux-workloads/linux-workloads-bin-dwarf

DATE=$(date '+%Y-%m-%d-%H-%M')
OUTPUT_DIR=$TESTDIR/$DATE-out
TRACE_DIR=$OUTPUT_DIR/traces
mkdir -p $TRACE_DIR

cd $CURDIR/../prof/builddir
./main $TRACEFILE $DWARFFILE > $TRACE_DIR/TRACEFILE-SPIKE
cd ..
fireperf/gen-all-flamegraphs-fireperf.sh  $OUTPUT_DIR
