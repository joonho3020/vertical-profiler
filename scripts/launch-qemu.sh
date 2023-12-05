#!/bin/bash



WORKLOAD_DIR=/scratch/joonho.whangbo/coding/the-one-profiler/test-io/test-binaries/linux-workloads

qemu-system-riscv64 -s -S -nographic -bios none -smp 4 \
	-machine virt -m 16384 \
	-object rng-random,filename=/dev/urandom,id=rng0 \
	-device virtio-rng-device,rng=rng0 \
	-device virtio-net-device,netdev=usernet \
	-netdev user,id=usernet,hostfwd=tcp::52819-:22 \
	-device virtio-blk-device,drive=hd0 \
	-kernel $WORKLOAD_DIR/linux-workloads-bin \
	-drive file=$WORKLOAD_DIR/linux-workloads.img,format=raw,id=hd0
