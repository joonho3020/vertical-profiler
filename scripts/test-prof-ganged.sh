./profiler_main \
  --log=OUT \
  --prof-out=./out  \
  --kernel-info=../../test-io/traces/KERNEL.riscv.dump,../../test-io/traces/linux-poweroff-bin-dwarf  \
  --dtb=../../test-io/traces/boom.dtb \
  --isa=rv64imafdczicsr_zifencei_zihpm_zicntr \
  --extlib=../spike-devices/libspikedevices.so \
  --device=sifive_uart \
  --device=iceblk,img=../../test-io/traces/linux-poweroff0-linux-poweroff.img \
  --rtl-trace=../../test-io/traces/COSPIKE-TRACE-FSIM-BOOM-ZERO-DRAM.log \
  ../../test-io/traces/linux-poweroff0-linux-poweroff-bin | tee PROFILER-LOGS
