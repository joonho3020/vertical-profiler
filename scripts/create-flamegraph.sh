#!/bin/bash


set -ex

CURDIR=$(pwd)



if ! [ -f main ]; then
  make
fi

source env.sh

SMALLTRACE_DIR=$CURDIR/traces/spike-linux-trace
TRACEFILE=$SMALLTRACE_DIR/TRACEFILE-SPIKE
DWARFFILE=$SMALLTRACE_DIR/linux-poweroff-bin-dwarf

DATE=$(date '+%Y-%m-%d-%H-%M')
OUTPUT_DIR=$CURDIR/output-dir/$DATE-out
TRACE_DIR=$OUTPUT_DIR/traces
mkdir -p $TRACE_DIR

./main $TRACEFILE $DWARFFILE > $TRACE_DIR/TRACEFILE-SPIKE
fireperf/gen-all-flamegraphs-fireperf.sh  $OUTPUT_DIR
