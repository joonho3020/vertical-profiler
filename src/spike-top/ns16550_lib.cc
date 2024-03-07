
#include <queue>
#include "ns16550_lib.h"

ns16550_lib_t::ns16550_lib_t(
    abstract_interrupt_controller_t* intctrl,
    uint32_t intid, uint32_t reg_shift, uint32_t reg_io_width)
  : ns16550_t(intctrl, intid, reg_shift, reg_io_width)
{
}

void ns16550_lib_t::serialize_proto(void* msg, void* arena) {
  ckpt_dll = dll;
  ckpt_dlm = dlm;
  ckpt_iir = iir;
  ckpt_ier = ier;
  ckpt_fcr = fcr;
  ckpt_lcr = lcr;
  ckpt_mcr = mcr;
  ckpt_lsr = lsr;
  ckpt_msr = msr;
  ckpt_scr = scr;
  ckpt_backoff_counter = backoff_counter;

  ckpt_rx_queue = rx_queue;
}

void ns16550_lib_t::deserialize_proto(void *msg) {
  dll = ckpt_dll;
  dlm = ckpt_dlm;
  iir = ckpt_iir;
  ier = ckpt_ier;
  fcr = ckpt_fcr;
  lcr = ckpt_lcr;
  mcr = ckpt_mcr;
  lsr = ckpt_lsr;
  msr = ckpt_msr;
  scr = ckpt_scr;
  backoff_counter = ckpt_backoff_counter;

  rx_queue = ckpt_rx_queue;
}
