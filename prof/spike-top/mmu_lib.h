#ifndef __MMU_LIB_H__
#define __MMU_LIB_H__



#include <riscv/mmu.h>
#include "sim_lib.h"
#include "processor_lib.h"

class mmu_lib_t : public mmu_t {
public:
  mmu_lib_t(simif_t* sim, endianness_t endianness, processor_lib_t* proc);
  ~mmu_lib_t();

  void take_checkpoint(char* host_offset);

  virtual void store_slow_path_intrapage(reg_t len,
      const uint8_t* bytes,
      mem_access_info_t access_info,
      bool actually_store) override;

  sim_lib_t* simlib;
};

#endif //__MMU_LIB_H__
