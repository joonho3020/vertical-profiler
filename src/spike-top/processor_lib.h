#ifndef _PROCESSOR_LIB_H_
#define _PROCESSOR_LIB_H_

#include <inttypes.h>
#include <riscv/processor.h>
#include <google/protobuf/arena.h>
#include "arch-state.pb.h"

#define PC_SERIALIZE_BEFORE 3
#define PC_SERIALIZE_AFTER 5
#define invalid_pc(pc) ((pc) & 1)

struct trace_entry_t {
  reg_t pc;
  reg_t asid;
  reg_t cycle;
};

struct rtl_step_t {
  bool val;
  uint64_t time;
  uint64_t pc;
  uint64_t insn;
  bool except;
  bool intrpt;
  int cause;
  bool has_w;
  uint64_t wdata;
  int priv;

  rtl_step_t() {};

  rtl_step_t(bool val, uint64_t time, uint64_t pc, uint64_t insn,
      bool except, bool intrpt, int cause, bool has_w, uint64_t wdata,
      int priv)
    : val(val), time(time), pc(pc), insn(insn), except(except),
    intrpt(intrpt), cause(cause), has_w(has_w), wdata(wdata), priv(priv)
  {
  }

  void print() {
    printf("%d %" PRIu64 " 0x%" PRIx64 " %d %d %d %d 0x%" PRIx64 "\n",
        val, time, pc, except, intrpt, cause, has_w, wdata);
  }
};

typedef std::vector<trace_entry_t> trace_t;

class wait_for_interrupt_t {};

class processor_lib_t : public processor_t
{
public:
  processor_lib_t(const isa_parser_t *isa, const cfg_t* cfg,
              simif_t* sim, uint32_t id, bool halt_on_reset,
              FILE *log_file, std::ostream& sout_); // because of command line option --log and -s we need both
  ~processor_lib_t();

  reg_t get_asid();
  reg_t get_mcycle();
  trace_t& step_trace() { return trace; }
  virtual void step(size_t n) override;
  void step_from_trace(int rd, uint64_t wdata, reg_t npc);

private:
  trace_t trace;

public:
  google::protobuf::Arena* arena;

  template <class T>
  inline T* create_protobuf(google::protobuf::Arena* arena) {
    return google::protobuf::Arena::Create<T>(arena);
  }

  // Protobuf serialization
  BasicCSR*        gen_basic_csr_proto(reg_t init);
  MisaCSR*         gen_misa_csr_proto(misa_csr_t_p ptr);
  BaseStatusCSR*   gen_base_status_csr_proto(bool has_page, reg_t wm, reg_t rm);
  MstatusCSR*      gen_mstatus_csr_proto(mstatus_csr_t_p csr);

  SstatusProxyCSR* gen_sstatus_proxy_csr_proto(sstatus_proxy_csr_t_p csr);
  VsstatusCSR*     gen_vsstatus_csr_proto(vsstatus_csr_t_p csr);
  SstatusCSR*      gen_sstatus_csr_proto(sstatus_csr_t_p csr);
  MaskedCSR*       gen_masked_csr_proto(reg_t val, reg_t mask);

  SmcntrpmfCSR*    gen_smcntrpmf_csr_proto(smcntrpmf_csr_t_p csr);
  WideCntrCSR*     gen_wide_cntr_csr_proto(wide_counter_csr_t_p csr);
  MedelegCSR*      gen_medeleg_csr_proto(csr_t_p csr);

  template <class CSR_T>
  VirtBasicCSR*    gen_virt_basic_csr_proto(virtualized_csr_t_p csr);

  HidelegCSR*      gen_hideleg_csr_proto(csr_t_p csr);
  DCSR*            gen_dcsr_csr_proto(dcsr_csr_t_p csr);
  McontextCSR*     gen_mcontext_csr_proto(std::shared_ptr<proxy_csr_t> csr);
  HenvcfgCSR*      gen_henvcfg_csr_proto(std::shared_ptr<henvcfg_csr_t> csr);
  StimecmpCSR*     gen_stimecmp_csr_proto(std::shared_ptr<stimecmp_csr_t> csr);

  virtual void serialize_proto(void* aproto, void* arena) override;

  // Protobuf deserialization
  template <class T>
  void set_basic_csr_from_proto(T& csr, const BasicCSR& proto);

  void set_medeleg_csr_from_proto(medeleg_csr_t& csr, const MedelegCSR& proto);
  void set_misa_csr_from_proto(misa_csr_t& csr, const MisaCSR& proto);
  void set_basestatus_csr_from_proto(base_status_csr_t& csr, const BaseStatusCSR& proto);
  void set_mstatus_csr_from_proto(mstatus_csr_t& csr, const MstatusCSR& proto);

  void set_sstatus_proxy_csr_from_proto(sstatus_proxy_csr_t& csr, const SstatusProxyCSR& proto);
  void set_vsstatus_csr_from_proto(vsstatus_csr_t& csr, const VsstatusCSR& proto);
  void set_sstatus_csr_from_proto(sstatus_csr_t& csr, const SstatusCSR& proto);
  void set_mcause_csr_from_proto(cause_csr_t& csr, const BasicCSR& proto);

  void set_masked_csr_from_proto(masked_csr_t& csr, const MaskedCSR& proto);
  void set_smcntrpmf_csr_from_proto(smcntrpmf_csr_t& csr, const SmcntrpmfCSR& proto);
  void set_widecntr_csr_from_proto(wide_counter_csr_t& csr, const WideCntrCSR& proto);

  template <class T>
  void set_virt_basic_csr_from_proto(virtualized_csr_t& csr, T& vcsr, const VirtBasicCSR& proto);

  void set_hideleg_csr_from_proto(hideleg_csr_t& csr, const HidelegCSR& proto);
  void set_dcsr_csr_from_proto(dcsr_csr_t& csr, const DCSR& proto);
  void set_mcontext_csr_from_proto(proxy_csr_t& csr, const McontextCSR& proto);
  void set_pmpaddr_csr_from_proto(pmpaddr_csr_t& csr, const PmpCSR& proto);

  void set_henvcfg_csr_from_proto(henvcfg_csr_t& csr, const HenvcfgCSR& proto);
  void set_hstateen_csr_from_proto(hstateen_csr_t& csr, const HstateenCSR& proto);
  void set_time_counter_csr_from_proto(time_counter_csr_t& csr, const BasicCSR& proto);
  void set_stimecmp_csr_from_proto(stimecmp_csr_t& csr, const StimecmpCSR& proto);

  void deserialize_proto(void* aproto);
  void print_state();
};


#endif // _PROCESSOR_LIB_H_
