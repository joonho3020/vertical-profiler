#!/bin/bash

PROF_TOP_DIR=$(pwd)/..
SPIKE_BUILDDIR=$PROF_TOP_DIR/src/builddir

cd $SPIKE_BUILDDIR
./spike_lib_main \
  --dtb=$PROF_TOP_DIR/test-io/traces/boom.dtb \
  --isa=rv64imafdczicsr_zifencei_zihpm_zicntr \
  --log=OUT \
  --extlib=$PROF_TOP_DIR/src/spike-devices/libspikedevices.so \
  --device="sifive_uart" \
  --device="iceblk,img=$PROF_TOP_DIR/test-io/traces/linux-poweroff0-linux-poweroff.img" \
  --rtl-trace=$PROF_TOP_DIR/test-io/traces/COSPIKE-TRACE-FSIM-BOOM-ZERO-DRAM.log \
  $PROF_TOP_DIR/test-io/traces/linux-poweroff0-linux-poweroff-bin
