#!/bin/bash

PROF_TOP_DIR=$(pwd)/..
SPIKE_BUILDDIR=$PROF_TOP_DIR/prof/riscv-isa-sim-private/build

function run_ganged_spike_lib() {
  SIM_DIR=$1
  SLOTNO=$2
  BINARY=$3

  SIM_SLOT_DIR=$SIM_DIR/sim_slot_$SLOTNO
  BINARY_DIR=$SIM_SLOT_DIR/$BINARY

  cd $SPIKE_BUILDDIR
  ./spike_lib --log OUT --fsim-trace $SIM_SLOT_DIR/COSPIKE-TRACE.log $BINARY_DIR
}


SIM_DIR=$PROF_TOP_DIR/chipyard/sims/firesim/deploy/sim-dir/embench

run_ganged_spike_lib $SIM_DIR 0  embench-riscv-aha-mont64-aha-mont64
run_ganged_spike_lib $SIM_DIR 1  embench-riscv-crc32-crc32
run_ganged_spike_lib $SIM_DIR 2  embench-riscv-cubic-cubic
run_ganged_spike_lib $SIM_DIR 3  embench-riscv-edn-edn
run_ganged_spike_lib $SIM_DIR 4  embench-riscv-huffbench-huffbench
run_ganged_spike_lib $SIM_DIR 5  embench-riscv-matmult-int-matmult-int
run_ganged_spike_lib $SIM_DIR 6  embench-riscv-minver-minver
run_ganged_spike_lib $SIM_DIR 7  embench-riscv-nbody-nbody
run_ganged_spike_lib $SIM_DIR 8  embench-riscv-nettle-aes-nettle-aes
run_ganged_spike_lib $SIM_DIR 9  embench-riscv-nettle-sha256-nettle-sha256
run_ganged_spike_lib $SIM_DIR 10 embench-riscv-nsichneu-nsichneu
run_ganged_spike_lib $SIM_DIR 11 embench-riscv-picojpeg-picojpeg
run_ganged_spike_lib $SIM_DIR 12 embench-riscv-qrduino-qrduino
run_ganged_spike_lib $SIM_DIR 13 embench-riscv-sglib-combined-sglib-combined
run_ganged_spike_lib $SIM_DIR 14 embench-riscv-slre-slre
run_ganged_spike_lib $SIM_DIR 15 embench-riscv-st-st
# run_ganged_spike_lib $SIM_DIR 16 embench-riscv-statemate-statemate
run_ganged_spike_lib $SIM_DIR 17 embench-riscv-ud-ud
run_ganged_spike_lib $SIM_DIR 18 embench-riscv-wikisort-wikisort
