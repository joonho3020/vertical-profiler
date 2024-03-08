#!/bin/bash



WORKLOAD_BIN=br-go-benchmarks-noperf0-br-go-benchmarks-noperf-bin
WORKLOAD_IMG=br-go-benchmarks-noperf.img

WORKLOAD_DIR=/scratch/joonho.whangbo/coding/vertical-profiler/test-io/traces/boom-go-bm-markdown/

# WORKLOAD_NAME=br-base
# WORKLOAD_DIR=/scratch/joonho.whangbo/coding/the-one-profiler/chipyard/software/firemarshal/images/firechip/$WORKLOAD_NAME

KERNEL_BIN=$WORKLOAD_DIR/$WORKLOAD_BIN
KERNEL_IMG=$WORKLOAD_DIR/$WORKLOAD_IMG

qemu-system-riscv64 -s -S -nographic -bios none -smp 4 \
	-machine virt -m 16384 \
	-object rng-random,filename=/dev/urandom,id=rng0 \
	-device virtio-rng-device,rng=rng0 \
	-device virtio-net-device,netdev=usernet \
	-netdev user,id=usernet,hostfwd=tcp::52819-:22 \
	-device virtio-blk-device,drive=hd0 \
	-kernel $KERNEL_BIN \
	-drive file=$KERNEL_IMG,format=raw,id=hd0
