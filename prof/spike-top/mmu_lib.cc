
#include <riscv/mmu.h>
#include "mmu_lib.h"

mmu_lib_t::mmu_lib_t(simif_t* sim, endianness_t, endianness, processor_lib_t* proc)
  : mmu_t(sim, endianness, proc) {
}

mmu_lib_t::~mmu_lib_t() {
}

void mmu_lib_t::take_checkpoint(char* host_offset) {
}

/* virtual void mmu_lib_t::store_slow_path_intrapage(reg_t len, */
/* const uint8_t* bytes, */
/* mem_access_info_t access_info, */
/* bool actually_store) { */
/* } */
