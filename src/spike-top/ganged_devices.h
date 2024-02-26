#ifndef __READ_OVERRIDE_DEVICES_H___
#define __READ_OVERRIDE_DEVICES_H___

#include <riscv/devices.h>
#include <riscv/abstract_device.h>
#include <riscv/abstract_interrupt_controller.h>

#include <memory>

/* #define DEBUG_GANGED_DEVICE */

class ganged_device_t : public abstract_device_t {
public:
  ganged_device_t(std::shared_ptr<abstract_device_t> base)
    : base(base), was_read(false) {}

public:
  virtual bool load(reg_t addr, size_t len, uint8_t* bytes) override {
#ifdef DEBUG_GANGED_DEVICE
    printf("ganged dev load: 0x%" PRIx64 "\n", addr);
#endif
    bool ret = base->load(addr, len, bytes);
    if (ret) was_read = true;
    return ret;
  }

  virtual bool store(reg_t addr, size_t len, const uint8_t* bytes) override {
#ifdef DEBUG_GANGED_DEVICE
    printf("ganged dev store: 0x%" PRIx64 "\n", addr);
#endif
    return base->store(addr, len, bytes);
  }

  virtual void tick(reg_t rtc_ticks) override {
    base->tick(rtc_ticks);
  }

public:
  bool was_read;

private:
  std::shared_ptr<abstract_device_t> base;
};


struct external_interrupts_t {
  external_interrupts_t(uint32_t id, int level)
    : id(id), level(level) { }

  uint32_t id;
  int level;
};

class plic_ganged_t : public plic_t {
public:
  plic_ganged_t(const simif_t* sim, uint32_t ndev);

public:
  bool load(reg_t addr, size_t len, uint8_t* bytes) override;
  bool store(reg_t addr, size_t len, const uint8_t* bytes) override;

  // How interrupts are ganged between the RTL simulation and functional sim.
  // Device models raise the interrupt line as soon as they finish operation
  // which will be written to the "interrupt_vector" in the PLIC
  // (submit_external_interrupt).
  // When the trace tells us that the core received an SEIP, we call the
  // "alert_core_external_interrupt" and set the MIP CSR of the corresponding
  // core. This will make the core to jump to the interrupt handler routine.
  // Then, the core will send a load request to the PLIC.
  // The PLIC responds to the load by returning the interrupt id, and the
  // core can jump to the correct interrupt routine. In this interrupt
  // routine, the core will send out MMIO load/store requests to the IO device
  // which will make it lower the interrupt. At this point, the functional
  // IO device model has to call the "lower_external_interrupt" which will
  // remove the interrupt signal from the PLIC's interrupt_vector.
  // The "alert_core_external_interrupt" simply alerts the core about the
  // interrupt (at this point, we don't know what the interrupt source is, as
  // the RISC-V spec does not specify how interrupt signals are arbitrated
  // within the PLIC). The following MMIO accesses to the device will 
  // tell the corresponding device that the core is servicing its interrupt
  // and thus need to lower the interrupt line.
  void set_interrupt_vector(uint32_t interrupt_id, int level);
  virtual void submit_external_interrupt(uint32_t interrupt_id) override;
  virtual bool alert_core_external_interrupt(int hartid) override;
  virtual bool lower_external_interrupt(uint32_t interrupt_id) override;

  uint64_t interrupt_vector;
};

#define MSIP_BASE 0x0
#define MTIMECMP_BASE 0x4000
#define MTIME_BASE 0xbff8

class clint_ganged_t : public clint_t {
public:
  clint_ganged_t(const simif_t* sim, uint64_t freq_hz, bool real_time);

public:
  bool load(reg_t addr, size_t len, uint8_t* bytes) override;
  bool store(reg_t addr, size_t len, const uint8_t* bytes) override;
  void tick(reg_t rtc_ticks) override;
};

#endif //__READ_OVERRIDE_DEVICES_H___
