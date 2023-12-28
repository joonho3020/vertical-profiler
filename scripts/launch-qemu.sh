#!/bin/bash



WORKLOAD_NAME=linux-workloads
WORKLOAD_DIR=/scratch/joonho.whangbo/coding/the-one-profiler/test-io/test-binaries/$WORKLOAD_NAME

# WORKLOAD_NAME=br-base
# WORKLOAD_DIR=/scratch/joonho.whangbo/coding/the-one-profiler/chipyard/software/firemarshal/images/firechip/$WORKLOAD_NAME

KERNEL_BIN=$WORKLOAD_DIR/$WORKLOAD_NAME-bin
KERNEL_IMG=$WORKLOAD_DIR/$WORKLOAD_NAME.img

qemu-system-riscv64 -s -S -nographic -bios none -smp 4 \
	-machine virt -m 16384 \
	-object rng-random,filename=/dev/urandom,id=rng0 \
	-device virtio-rng-device,rng=rng0 \
	-device virtio-net-device,netdev=usernet \
	-netdev user,id=usernet,hostfwd=tcp::52819-:22 \
	-device virtio-blk-device,drive=hd0 \
	-kernel $KERNEL_BIN \
	-drive file=$KERNEL_IMG,format=raw,id=hd0
