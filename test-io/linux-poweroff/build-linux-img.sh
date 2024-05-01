#!/bin/bash



set -ex


make
mkdir -p linux-workloads/overlay/root
cp *.linux.riscv linux-workloads/overlay/root
WORKLOAD_DIR=$ONE_PROF_BASE/test-io/test-binaries/

MARSHAL_DIR=$ONE_PROF_BASE/chipyard/software/firemarshal
cd $MARSHAL_DIR
./marshal -vvv build $WORKLOAD_DIR/linux-workloads.yaml

# Delete all the symlinks under directory
find $WORKLOAD_DIR/linux-workloads -maxdepth 1 -type l -delete

ln -s $MARSHAL_DIR/images/firechip/linux-workloads/linux-workloads-bin       $WORKLOAD_DIR/linux-workloads
ln -s $MARSHAL_DIR/images/firechip/linux-workloads/linux-workloads-bin-dwarf $WORKLOAD_DIR/linux-workloads
ln -s $MARSHAL_DIR/images/firechip/linux-workloads/linux-workloads.img       $WORKLOAD_DIR/linux-workloads

riscv64-unknown-linux-gnu-objdump -D \
  $MARSHAL_DIR/images/firechip/linux-workloads/linux-workloads-bin-dwarf > \
  $WORKLOAD_DIR/linux-workloads/KERNEL.riscv.dump
