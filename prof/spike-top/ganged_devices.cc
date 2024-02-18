#include <cstdint>
#include <sys/time.h>
#include <inttypes.h>

#include <riscv/devices.h>
#include <riscv/encoding.h>
#include <riscv/processor.h>
#include <riscv/simif.h>

#include "ganged_devices.h"

plic_ganged_t::plic_ganged_t(const simif_t* sim, uint32_t ndev)
  : plic_t(sim, ndev)
{
  interrupt_vector = 0;
}

bool plic_ganged_t::load(reg_t addr, size_t len, uint8_t* bytes) {
/* printf("- plic ganged load addr: 0x%" PRIx64 " len: %lu\n", addr, len); */
  return true;
}

bool plic_ganged_t::store(reg_t addr, size_t len, const uint8_t* bytes) {
/* printf("- plic ganged store addr: 0x%" PRIx64 " len: %lu\n", addr, len); */
  contexts[0].proc->get_state()->mip->backdoor_write_with_mask(MIP_SEIP, 0);
  return true;
}

void plic_ganged_t::set_interrupt_vector(uint32_t interrupt_id, int level) {
  assert(interrupt_id < 64);
  if (level) {
    interrupt_vector = interrupt_vector | (1UL << interrupt_id);
  } else {
    interrupt_vector = interrupt_vector & ~(1UL << interrupt_id);
  }
}

void plic_ganged_t::submit_external_interrupt(uint32_t interrupt_id) {
  set_interrupt_vector(interrupt_id, 1);
}

bool plic_ganged_t::alert_core_external_interrupt(int hartid) {
  if (interrupt_vector == 0) return false;
  contexts[hartid].proc->get_state()->mip->backdoor_write_with_mask(MIP_SEIP, MIP_SEIP);
  return true;
}

bool plic_ganged_t::lower_external_interrupt(uint32_t interrupt_id) {
  if (!(interrupt_vector & (1 << interrupt_id))) return false;
  set_interrupt_vector(interrupt_id, 0);
  return true;
}

clint_ganged_t::clint_ganged_t(const simif_t* sim, uint64_t freq_hz, bool real_time)
  : clint_t(sim, freq_hz, real_time)
{
}

void clint_ganged_t::tick(reg_t rtc_ticks) {
/* printf("- clint ganged tick\n"); */

  if (real_time) {
   struct timeval now;
   uint64_t diff_usecs;

   gettimeofday(&now, NULL);
   diff_usecs = ((now.tv_sec - real_time_ref_secs) * 1000000) + (now.tv_usec - real_time_ref_usecs);
   mtime = diff_usecs * freq_hz / 1000000;
  } else {
    mtime += rtc_ticks;
  }
}

bool clint_ganged_t::load(reg_t addr, size_t len, uint8_t* bytes) {
  if (len > 8)
    return false;

/* printf("- clint ganged load addr: 0x%" PRIx64 " len: %lu\n", addr, len); */

  tick(0);

  if (addr >= MSIP_BASE && addr < MTIMECMP_BASE) {
    if (len == 8) {
      // Implement double-word loads as a pair of word loads
      return load(addr, 4, bytes) && load(addr + 4, 4, bytes + 4);
    }

    const auto hart_id = (addr - MSIP_BASE) / sizeof(msip_t);
    const msip_t res = sim->get_harts().count(hart_id) &&
      (sim->get_harts().at(hart_id)->get_state()->mip->read() & MIP_MSIP);
    read_little_endian_reg(res, addr, len, bytes);
    return true;
  } else if (addr >= MTIMECMP_BASE && addr < MTIME_BASE) {
    const auto hart_id = (addr - MTIMECMP_BASE) / sizeof(mtimecmp_t);
    const mtime_t res = sim->get_harts().count(hart_id) ? mtimecmp[hart_id] : 0;
    read_little_endian_reg(res, addr, len, bytes);
  } else if (addr >= MTIME_BASE && addr < MTIME_BASE + sizeof(mtime_t)) {
    read_little_endian_reg(mtime, addr, len, bytes);
  } else if (addr + len <= CLINT_SIZE) {
    memset(bytes, 0, len);
  } else {
    return false;
  }
  return true;
}

bool clint_ganged_t::store(reg_t addr, size_t len, const uint8_t* bytes) {
  if (len > 8)
    return false;

/* printf("- clint gagned store addr: 0x%" PRIx64 " len: %lu\n", addr, len); */

  if (addr >= MSIP_BASE && addr < MTIMECMP_BASE) {
    if (len == 8) {
      // Implement double-word stores as a pair of word stores
      return store(addr, 4, bytes) && store(addr + 4, 4, bytes + 4);
    }

    if (addr % sizeof(msip_t) == 0) {  // ignore in-between bytes
      msip_t msip = 0;
      write_little_endian_reg(&msip, addr, len, bytes);

      const auto hart_id = (addr - MSIP_BASE) / sizeof(msip_t);
      if (sim->get_harts().count(hart_id)) {
        // When running with RTL lockstep, don't raise interrupts at random
        // points in time. Rather we want to raise interrupts only when
        // the RTL tells us to. However, we want to lower the interrupt by
        // keeping state in the clint and lowering the signal when the core wants
        // the clint to.
        reg_t ival = msip & 1 ? MIP_MSIP : 0;
      }
    }
  } else if (addr >= MTIMECMP_BASE && addr < MTIME_BASE) {
    const auto hart_id = (addr - MTIMECMP_BASE) / sizeof(mtimecmp_t);
    if (sim->get_harts().count(hart_id))
      write_little_endian_reg(&mtimecmp[hart_id], addr, len, bytes);
  } else if (addr >= MTIME_BASE && addr < MTIME_BASE + sizeof(mtime_t)) {
    write_little_endian_reg(&mtime, addr, len, bytes);
  } else if (addr + len <= CLINT_SIZE) {
    // Do nothing
  } else {
    return false;
  }
  tick(0);
  return true;
}
