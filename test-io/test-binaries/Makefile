RISCVPFX=riscv64-unknown


BARE_PFX=$(RISCVPFX)-elf
BARE_CC=$(BARE_PFX)-gcc

LINUX_PFX=$(RISCVPFX)-linux-gnu
LINUX_CC=$(LINUX_PFX)-gcc

CFLAGS = -fno-common -fno-builtin-printf -std=c++11 -static

PROGRAMS = hello

%.bare.riscv: %.c
	$(BARE_CC) $(CFLAGS) -specs=htif_nano.specs  -o $@ $^

%.linux.riscv: %.c
	$(LINUX_CC) $(CFLAGS) -DLINUX -o $@ $^

.PHONY: default
default: bare linux

.PHONY: bare
bare: $(addsuffix .bare.riscv, $(PROGRAMS))

.PHONY: linux
linux: $(addsuffix .linux.riscv, $(PROGRAMS))

.PHONY: clean
clean:
	rm -f *.riscv
