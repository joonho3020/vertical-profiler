#!/bin/bash

PROF_TOP_DIR=$(pwd)/..
SPIKE_BUILDDIR=$PROF_TOP_DIR/src/builddir

cd $SPIKE_BUILDDIR
# ./spike_lib_main \
# --dtb=$PROF_TOP_DIR/test-io/traces/boom.dtb \
# --isa=rv64imafdczicsr_zifencei_zihpm_zicntr \
# --log=OUT \
# --extlib=$PROF_TOP_DIR/src/spike-devices/libspikedevices.so \
# --device="sifive_uart" \
# --device="iceblk,img=$PROF_TOP_DIR/test-io/traces/linux-poweroff0-linux-poweroff.img" \
# --rtl-trace=$PROF_TOP_DIR/test-io/traces/COSPIKE-TRACE-FSIM-BOOM-ZERO-DRAM.log \
# $PROF_TOP_DIR/test-io/traces/linux-poweroff0-linux-poweroff-bin

# TRACE_DIR=$PROF_TOP_DIR/test-io/traces/boom-markdown
# DISK_IMG=markdown0-markdown.img
# FSIM_TRACE_DIR=/scratch/joonho.whangbo/coding/FIRESIM_RUNS_DIR/boom-linux-multithread-sv39/sim_slot_0/COSPIKE-TRACES
# WORKLOAD=markdown0-markdown-bin
# gdb --args ./spike_lib_main \
#   --dtb=$TRACE_DIR/boom-sv39.dtb \
#   --isa=rv64imafdczicsr_zifencei_zihpm_zicntr \
#   --log=OUT \
#   --extlib=$PROF_TOP_DIR/src/spike-devices/libspikedevices.so \
#   --device="sifive_uart" \
#   --device="iceblk,img=$TRACE_DIR/$DISK_IMG" \
#   --rtl-trace=$FSIM_TRACE_DIR \
#   $TRACE_DIR/$WORKLOAD

TRACE_DIR=$PROF_TOP_DIR/test-io/traces/boom-markdown
DISK_IMG=markdown0-markdown.img
FSIM_TRACE_DIR=/scratch/joonho.whangbo/coding/FIRESIM_RUNS_DIR/boom-linux-multithread-sv48/sim_slot_0/COSPIKE-TRACES
WORKLOAD=markdown0-markdown-bin
./spike_lib_main \
  --dtb=$TRACE_DIR/boom.dtb \
  --isa=rv64imafdczicsr_zifencei_zihpm_zicntr \
  --log=OUT \
  --extlib=$PROF_TOP_DIR/src/spike-devices/libspikedevices.so \
  --device="sifive_uart" \
  --device="iceblk,img=$TRACE_DIR/$DISK_IMG" \
  --rtl-cfg="$FSIM_TRACE_DIR:12:100000:6291456" \
  $TRACE_DIR/$WORKLOAD
