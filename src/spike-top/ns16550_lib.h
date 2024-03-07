#ifndef __NS16550_LIB_H__
#define __NS16550_LIB_H__

#include <queue>
#include <sstream>
#include <riscv/sim.h>
#include <riscv/dts.h>
#include <riscv/devices.h>
#include <riscv/abstract_device.h>
#include <riscv/abstract_interrupt_controller.h>

class ns16550_lib_t : public ns16550_t {
public:
  ns16550_lib_t(abstract_interrupt_controller_t *intctrl,
            uint32_t interrupt_id, uint32_t reg_shift, uint32_t reg_io_width);

private:
  virtual void serialize_proto(void* msg, void* arena) override;
  virtual void deserialize_proto(void* msg) override;

  std::queue<uint8_t> ckpt_rx_queue;
  uint8_t ckpt_dll;
  uint8_t ckpt_dlm;
  uint8_t ckpt_iir;
  uint8_t ckpt_ier;
  uint8_t ckpt_fcr;
  uint8_t ckpt_lcr;
  uint8_t ckpt_mcr;
  uint8_t ckpt_lsr;
  uint8_t ckpt_msr;
  uint8_t ckpt_scr;
  int     ckpt_backoff_counter;
};

std::string ns16550_lib_generate_dts(const sim_t* sim)
{
  std::stringstream s;
  s << std::hex
    << "    SERIAL0: ns16550@" << NS16550_BASE << " {\n"
       "      compatible = \"ns16550a\";\n"
       "      clock-frequency = <" << std::dec << (sim->CPU_HZ/sim->INSNS_PER_RTC_TICK) << ">;\n"
       "      interrupt-parent = <&PLIC>;\n"
       "      interrupts = <" << std::dec << NS16550_INTERRUPT_ID;
  reg_t ns16550bs = NS16550_BASE;
  reg_t ns16550sz = NS16550_SIZE;
  s << std::hex << ">;\n"
       "      reg = <0x" << (ns16550bs >> 32) << " 0x" << (ns16550bs & (uint32_t)-1) <<
                   " 0x" << (ns16550sz >> 32) << " 0x" << (ns16550sz & (uint32_t)-1) << ">;\n"
       "      reg-shift = <0x" << NS16550_REG_SHIFT << ">;\n"
       "      reg-io-width = <0x" << NS16550_REG_IO_WIDTH << ">;\n"
       "    };\n";
  return s.str();
}

ns16550_lib_t* ns16550_lib_parse_from_fdt(const void* fdt, const sim_t* sim, reg_t* base, const std::vector<std::string>& sargs)
{
  uint32_t ns16550_shift, ns16550_io_width, ns16550_int_id;
  if (fdt_parse_ns16550(fdt, base,
                        &ns16550_shift, &ns16550_io_width, &ns16550_int_id,
                        "ns16550a") == 0) {
    abstract_interrupt_controller_t* intctrl = sim->get_intctrl();
    return new ns16550_lib_t(intctrl, ns16550_int_id, ns16550_shift, ns16550_io_width);
  } else {
    return nullptr;
  }
}

REGISTER_DEVICE(ns16550_lib, ns16550_lib_parse_from_fdt, ns16550_lib_generate_dts)

#endif // __NS16550_LIB_H__
