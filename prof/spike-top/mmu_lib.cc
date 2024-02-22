
#include <iostream>
#include <riscv/mmu.h>
#include "mmu_lib.h"
#include "sim_lib.h"
#include "processor_lib.h"

mmu_lib_t::mmu_lib_t(simif_t* sim, endianness_t endianness, processor_lib_t* proc)
  : mmu_t(sim, endianness, proc) {
  simlib = dynamic_cast<sim_lib_t*>(sim);
}

mmu_lib_t::~mmu_lib_t() {
}

void mmu_lib_t::take_checkpoint(char* host_offset) {
  auto& mm_ckpt = simlib->mm_ckpt;
  auto& mempool = simlib->ckpt_mempool;
  if (mm_ckpt.find(host_offset) == mm_ckpt.end()) {
    if (mempool.size() == 0) {
      char* buf = (char*)calloc(PGSIZE, 1);
      if (!buf)
        throw std::bad_alloc();
      mm_ckpt[host_offset] = buf;
    } else {
      mm_ckpt[host_offset] = mempool.back();
      mempool.pop_back();
    }
    memcpy(mm_ckpt[host_offset], (void*)host_offset, PGSIZE);
  }
}

void mmu_lib_t::store_slow_path_intrapage(reg_t len,
    const uint8_t* bytes,
    mem_access_info_t access_info,
    bool actually_store) {
  bool inplace_ckpt = !simlib->serialize_mem && simlib->serialize_called;

  reg_t addr = access_info.vaddr;
  reg_t vpn = addr >> PGSHIFT;
  if (!access_info.flags.is_special_access() && vpn == (tlb_store_tag[vpn % TLB_ENTRIES] & ~TLB_CHECK_TRIGGERS)) {
    if (actually_store) {
      char* host_offset = tlb_data[vpn % TLB_ENTRIES].host_offset;
      auto host_addr = host_offset + addr;
      if (inplace_ckpt) take_checkpoint(host_offset);
      memcpy(host_addr, bytes, len);
    }
    return;
  }

  reg_t paddr = translate(access_info, len);
  if (actually_store) {
    if (auto host_addr = sim->addr_to_mem(paddr)) {
      if (inplace_ckpt) {
        reg_t pgoffset = paddr % PGSIZE;
        char* host_offset = host_addr - pgoffset;
        take_checkpoint(host_offset);
      }
      memcpy(host_addr, bytes, len);
      if (tracer.interested_in_range(paddr, paddr + PGSIZE, STORE))
        tracer.trace(paddr, len, STORE);
      else if (!access_info.flags.is_special_access())
        refill_tlb(addr, paddr, host_addr, STORE);
    } else if (!mmio_store(paddr, len, bytes)) {
      throw trap_store_access_fault(access_info.effective_virt, addr, 0, 0);
    }
  }
}
