#!/bin/bash


set -ex

CURDIR=$(pwd)



if ! [ -f main ]; then
  make
fi

SMALLTRACE_DIR=$CURDIR/traces/linux-poweroff-trace
TRACEFILE=$SMALLTRACE_DIR/TRACEFILE-C0
DWARFFILE=$SMALLTRACE_DIR/linux-uniform0-br-base-bin-dwarf

DATE=$(date '+%Y-%m-%d-%h')
OUTPUT_DIR=$CURDIR/output-dir/$DATE-out
TRACE_DIR=$OUTPUT_DIR/traces
mkdir -p $TRACE_DIR

./main $TRACEFILE $DWARFFILE > $TRACE_DIR/TRACEFILE-C0
fireperf/gen-all-flamegraphs-fireperf.sh  $OUTPUT_DIR
