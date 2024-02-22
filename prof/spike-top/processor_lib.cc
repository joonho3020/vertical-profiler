
#include <riscv/processor.h>
#include <riscv/mmu.h>
#include <riscv/disasm.h>
#include <google/protobuf/arena.h>
#include "processor_lib.h"
#include "mmu_lib.h"

static void commit_log_reset(processor_t* p)
{
  p->get_state()->log_reg_write.clear();
  p->get_state()->log_mem_read.clear();
  p->get_state()->log_mem_write.clear();
}

static void commit_log_stash_privilege(processor_t* p)
{
  state_t* state = p->get_state();
  state->last_inst_priv = state->prv;
  state->last_inst_xlen = p->get_xlen();
  state->last_inst_flen = p->get_flen();
}

static void commit_log_print_value(FILE *log_file, int width, const void *data)
{
  assert(log_file);

  switch (width) {
    case 8:
      fprintf(log_file, "0x%02" PRIx8, *(const uint8_t *)data);
      break;
    case 16:
      fprintf(log_file, "0x%04" PRIx16, *(const uint16_t *)data);
      break;
    case 32:
      fprintf(log_file, "0x%08" PRIx32, *(const uint32_t *)data);
      break;
    case 64:
      fprintf(log_file, "0x%016" PRIx64, *(const uint64_t *)data);
      break;
    default:
      // max lengh of vector
      if (((width - 1) & width) == 0) {
        const uint64_t *arr = (const uint64_t *)data;

        fprintf(log_file, "0x");
        for (int idx = width / 64 - 1; idx >= 0; --idx) {
          fprintf(log_file, "%016" PRIx64, arr[idx]);
        }
      } else {
        abort();
      }
      break;
  }
}

static void commit_log_print_value(FILE *log_file, int width, uint64_t val)
{
  commit_log_print_value(log_file, width, &val);
}

static void commit_log_print_insn(processor_t *p, reg_t pc, insn_t insn)
{
  FILE *log_file = p->get_log_file();

  auto& reg = p->get_state()->log_reg_write;
  auto& load = p->get_state()->log_mem_read;
  auto& store = p->get_state()->log_mem_write;
  int priv = p->get_state()->last_inst_priv;
  int xlen = p->get_state()->last_inst_xlen;
  int flen = p->get_state()->last_inst_flen;

  // print core id on all lines so it is easy to grep
  fprintf(log_file, "core%4" PRId32 ": ", p->get_id());

  fprintf(log_file, "%1d ", priv);
  commit_log_print_value(log_file, xlen, pc);
  fprintf(log_file, " (");
  commit_log_print_value(log_file, insn.length() * 8, insn.bits());
  fprintf(log_file, ")");
  bool show_vec = false;

  for (auto item : reg) {
    if (item.first == 0)
      continue;

    char prefix = ' ';
    int size;
    int rd = item.first >> 4;
    bool is_vec = false;
    bool is_vreg = false;
    switch (item.first & 0xf) {
    case 0:
      size = xlen;
      prefix = 'x';
      break;
    case 1:
      size = flen;
      prefix = 'f';
      break;
    case 2:
      size = p->VU.VLEN;
      prefix = 'v';
      is_vreg = true;
      break;
    case 3:
      is_vec = true;
      break;
    case 4:
      size = xlen;
      prefix = 'c';
      break;
    default:
      assert("can't been here" && 0);
      break;
    }

    if (!show_vec && (is_vreg || is_vec)) {
        fprintf(log_file, " e%ld %s%ld l%ld",
                (long)p->VU.vsew,
                p->VU.vflmul < 1 ? "mf" : "m",
                p->VU.vflmul < 1 ? (long)(1 / p->VU.vflmul) : (long)p->VU.vflmul,
                (long)p->VU.vl->read());
        show_vec = true;
    }

    if (!is_vec) {
      if (prefix == 'c')
        fprintf(log_file, " c%d_%s ", rd, csr_name(rd));
      else
        fprintf(log_file, " %c%-2d ", prefix, rd);
      if (is_vreg)
        commit_log_print_value(log_file, size, &p->VU.elt<uint8_t>(rd, 0));
      else
        commit_log_print_value(log_file, size, item.second.v);
    }
  }

  for (auto item : load) {
    fprintf(log_file, " mem ");
    commit_log_print_value(log_file, xlen, std::get<0>(item));
  }

  for (auto item : store) {
    fprintf(log_file, " mem ");
    commit_log_print_value(log_file, xlen, std::get<0>(item));
    fprintf(log_file, " ");
    commit_log_print_value(log_file, std::get<2>(item) << 3, std::get<1>(item));
  }
  fprintf(log_file, "\n");
}


// These two functions are expected to be inlined by the compiler separately in
// the processor_t::step() loop. The logged variant is used in the slow path
static inline reg_t execute_insn_fast(processor_t* p, reg_t pc, insn_fetch_t fetch) {
  return fetch.func(p, fetch.insn, pc);
}

static inline reg_t execute_insn_logged(processor_t* p, reg_t pc, insn_fetch_t fetch)
{
  if (p->get_log_commits_enabled()) {
    commit_log_reset(p);
    commit_log_stash_privilege(p);
  }

  reg_t npc;

  try {
    npc = fetch.func(p, fetch.insn, pc);
    if (npc != PC_SERIALIZE_BEFORE) {
      if (p->get_log_commits_enabled()) {
        commit_log_print_insn(p, pc, fetch.insn);
      }
     }
  } catch (wait_for_interrupt_t &t) {
      if (p->get_log_commits_enabled()) {
        commit_log_print_insn(p, pc, fetch.insn);
      }
      throw;
  } catch(mem_trap_t& t) {
      //handle segfault in midlle of vector load/store
      if (p->get_log_commits_enabled()) {
        for (auto item : p->get_state()->log_reg_write) {
          if ((item.first & 3) == 3) {
            commit_log_print_insn(p, pc, fetch.insn);
            break;
          }
        }
      }
      throw;
  } catch(...) {
    throw;
  }
  return npc;
}

processor_lib_t::processor_lib_t(const isa_parser_t *isa, const cfg_t* cfg,
              simif_t* sim, uint32_t id, bool halt_on_reset,
              FILE *log_file, std::ostream& sout_)
  : processor_t(isa, cfg, sim, id, halt_on_reset, log_file, sout_)
{
  delete mmu;
  mmu = new mmu_lib_t(sim, cfg->endianness, this);
}

processor_lib_t::~processor_lib_t() {
}

reg_t processor_lib_t::get_asid() {
  reg_t satp = this->get_state()->satp->read();
  reg_t asid = get_field(satp, this->get_xlen() == 32 ? SATP32_ASID : SATP64_ASID);
  return asid;
}

void processor_lib_t::step(size_t n) {
  trace.clear();

  if (!state.debug_mode) {
    if (halt_request == HR_REGULAR) {
      enter_debug_mode(DCSR_CAUSE_DEBUGINT);
    } else if (halt_request == HR_GROUP) {
      enter_debug_mode(DCSR_CAUSE_GROUP);
    } // !!!The halt bit in DCSR is deprecated.
    else if (state.dcsr->halt) {
      enter_debug_mode(DCSR_CAUSE_HALT);
    }
  }

  while (n > 0) {
    size_t instret = 0;
    reg_t pc = state.pc;
    mmu_t* _mmu = mmu;
    state.prv_changed = false;
    state.v_changed = false;

    #define advance_pc() \
      if (unlikely(invalid_pc(pc))) { \
        switch (pc) { \
          case PC_SERIALIZE_BEFORE: state.serialized = true; break; \
          case PC_SERIALIZE_AFTER: ++instret; break; \
          default: abort(); \
        } \
        pc = state.pc; \
        break; \
      } else { \
        state.pc = pc; \
        instret++; \
      }

    try
    {
      take_pending_interrupt();

      if (unlikely(slow_path()))
      {
        // Main simulation loop, slow path.
        while (instret < n)
        {
          if (unlikely(!state.serialized && state.single_step == state.STEP_STEPPED)) {
            state.single_step = state.STEP_NONE;
            if (!state.debug_mode) {
              enter_debug_mode(DCSR_CAUSE_STEP);
              // enter_debug_mode changed state.pc, so we can't just continue.
              break;
            }
          }

          if (unlikely(state.single_step == state.STEP_STEPPING)) {
            state.single_step = state.STEP_STEPPED;
          }

          if (!state.serialized && check_triggers_icount) {
            auto match = TM.detect_icount_match();
            if (match.has_value()) {
              assert(match->timing == triggers::TIMING_BEFORE);
              throw triggers::matched_t((triggers::operation_t)0, 0, match->action, state.v);
            }
          }

          // debug mode wfis must nop
          if (unlikely(in_wfi && !state.debug_mode)) {
            throw wait_for_interrupt_t();
          }

          in_wfi = false;
          insn_fetch_t fetch = mmu->load_insn(pc);
          if (debug && !state.serialized)
            disasm(fetch.insn);
          trace.push_back({pc, get_asid(), state.prv, state.prev_prv, fetch.insn});
          pc = execute_insn_logged(this, pc, fetch);
          advance_pc();
        }
      }
      else while (instret < n)
      {
        // Main simulation loop, fast path.
        for (auto ic_entry = _mmu->access_icache(pc); ; ) {
          auto fetch = ic_entry->data;
          trace.push_back({pc, get_asid(), state.prv, state.prev_prv, fetch.insn});
          pc = execute_insn_fast(this, pc, fetch);
          ic_entry = ic_entry->next;
          if (unlikely(ic_entry->tag != pc))
            break;
          if (unlikely(instret + 1 == n))
            break;
          instret++;
          state.pc = pc;
        }

        advance_pc();
      }
    }
    catch(trap_t& t)
    {
      take_trap(t, pc);
      n = instret;

      // Trigger action takes priority over single step
      auto match = TM.detect_trap_match(t);
      if (match.has_value())
        take_trigger_action(match->action, 0, state.pc, 0);
      else if (unlikely(state.single_step == state.STEP_STEPPED)) {
        state.single_step = state.STEP_NONE;
        enter_debug_mode(DCSR_CAUSE_STEP);
      }
    }
    catch (triggers::matched_t& t)
    {
      if (mmu->matched_trigger) {
        delete mmu->matched_trigger;
        mmu->matched_trigger = NULL;
      }
      take_trigger_action(t.action, t.address, pc, t.gva);
    }
    catch(trap_debug_mode&)
    {
      enter_debug_mode(DCSR_CAUSE_SWBP);
    }
    catch (wait_for_interrupt_t &t)
    {
      // Return to the outer simulation loop, which gives other devices/harts a
      // chance to generate interrupts.
      //
      // In the debug ROM this prevents us from wasting time looping, but also
      // allows us to switch to other threads only once per idle loop in case
      // there is activity.
      n = ++instret;
      in_wfi = true;
    }

    state.minstret->bump(instret);

    // Model a hart whose CPI is 1.
    state.mcycle->bump(instret);

    n -= instret;
  }
}

BasicCSR* processor_lib_t::gen_basic_csr_proto(reg_t init) {
  BasicCSR* proto = create_protobuf<BasicCSR>(arena);
  proto->set_msg_val(init);
  return proto;
}

MisaCSR* processor_lib_t::gen_misa_csr_proto(misa_csr_t_p ptr) {
  MisaCSR*  mproto = create_protobuf<MisaCSR>(arena);
  BasicCSR* bproto = gen_basic_csr_proto(ptr->val);

  mproto->set_allocated_msg_basic_csr(bproto);
  mproto->set_msg_max_isa(ptr->max_isa);
  mproto->set_msg_write_mask(ptr->write_mask);
  return mproto;
}

BaseStatusCSR* processor_lib_t::gen_base_status_csr_proto(bool has_page,
                                                      reg_t wm,
                                                      reg_t rm) {
  BaseStatusCSR* proto = create_protobuf<BaseStatusCSR>(arena);
  proto->set_msg_has_page(has_page);
  proto->set_msg_sstatus_write_mask(wm);
  proto->set_msg_sstatus_read_mask(rm);
  return proto;
}

MstatusCSR* processor_lib_t::gen_mstatus_csr_proto(mstatus_csr_t_p csr) {
  MstatusCSR* m = create_protobuf<MstatusCSR>(arena);
  BaseStatusCSR* base = gen_base_status_csr_proto(csr->has_page,
                                                  csr->sstatus_write_mask,
                                                  csr->sstatus_read_mask);
  m->set_allocated_msg_base_status_csr(base);
  m->set_msg_val(csr->val);
  return m;
}

SstatusProxyCSR* processor_lib_t::gen_sstatus_proxy_csr_proto(sstatus_proxy_csr_t_p csr) {
  SstatusProxyCSR* sp = create_protobuf<SstatusProxyCSR>(arena);
  MstatusCSR*    m = gen_mstatus_csr_proto(csr->mstatus);
  BaseStatusCSR* b = gen_base_status_csr_proto(csr->has_page,
                                               csr->sstatus_write_mask,
                                               csr->sstatus_read_mask);
  sp->set_allocated_msg_base_status_csr(b);
  sp->set_allocated_msg_mstatus_csr(m);
  return sp;
}

VsstatusCSR* processor_lib_t::gen_vsstatus_csr_proto(vsstatus_csr_t_p csr) {
  VsstatusCSR* v = create_protobuf<VsstatusCSR>(arena);
  BaseStatusCSR* b = gen_base_status_csr_proto(csr->has_page,
                                               csr->sstatus_write_mask,
                                               csr->sstatus_read_mask);
  v->set_allocated_msg_base_status_csr(b);
  v->set_msg_val(csr->val);
  return v;
}

SstatusCSR* processor_lib_t::gen_sstatus_csr_proto(sstatus_csr_t_p csr) {
  SstatusCSR*      s  = create_protobuf<SstatusCSR>(arena);
  SstatusProxyCSR* sp = gen_sstatus_proxy_csr_proto(csr->orig_sstatus);
  VsstatusCSR*     v  = gen_vsstatus_csr_proto(csr->virt_sstatus);

  s->set_allocated_msg_orig_sstatus(sp);
  s->set_allocated_msg_virt_sstatus(v);
  return s;
}

MaskedCSR* processor_lib_t::gen_masked_csr_proto(reg_t val, reg_t mask) {
  MaskedCSR* m = create_protobuf<MaskedCSR>(arena);
  BasicCSR*  b = gen_basic_csr_proto(val);
  m->set_allocated_msg_basic_csr(b);
  m->set_msg_mask(mask);
  return m;
}

SmcntrpmfCSR* processor_lib_t::gen_smcntrpmf_csr_proto(smcntrpmf_csr_t_p csr) {
  SmcntrpmfCSR* s = create_protobuf<SmcntrpmfCSR>(arena);
  MaskedCSR*    m = gen_masked_csr_proto(csr->val, csr->mask);

  s->set_allocated_msg_masked_csr(m);
  if (csr->prev_val.has_value()) {
    OptionalUInt64* o = create_protobuf<OptionalUInt64>(arena);
    o->set_msg_val(csr->prev_val.value());
    s->set_allocated_msg_prev_val(o);
  }
  return s;
}

WideCntrCSR* processor_lib_t::gen_wide_cntr_csr_proto(wide_counter_csr_t_p csr) {
  WideCntrCSR*  w = create_protobuf<WideCntrCSR>(arena);
  SmcntrpmfCSR* s = gen_smcntrpmf_csr_proto(csr->config_csr);

  w->set_msg_val(csr->val);
  w->set_allocated_msg_config_csr(s);
  return w;
}

MedelegCSR* processor_lib_t::gen_medeleg_csr_proto(csr_t_p csr) {
  MedelegCSR* m = create_protobuf<MedelegCSR>(arena);
  auto medeleg = std::dynamic_pointer_cast<medeleg_csr_t>(csr);
  BasicCSR* b = gen_basic_csr_proto(medeleg->val);

  m->set_allocated_msg_basic_csr(b);
  m->set_msg_hypervisor_exceptions(medeleg->hypervisor_exceptions);
  return m;
}

template <class CSR_T>
VirtBasicCSR* processor_lib_t::gen_virt_basic_csr_proto(virtualized_csr_t_p csr) {
  auto vcsr = std::dynamic_pointer_cast<CSR_T>(csr->virt_csr);
  auto ocsr = std::dynamic_pointer_cast<CSR_T>(csr->orig_csr);
  BasicCSR* vproto = gen_basic_csr_proto(vcsr->val);
  BasicCSR* oproto = gen_basic_csr_proto(ocsr->val);

  VirtBasicCSR* vb_proto = create_protobuf<VirtBasicCSR>(arena);
  vb_proto->set_allocated_msg_nonvirt_csr(oproto);
  vb_proto->set_allocated_msg_virt_csr(vproto);
  return vb_proto;
}

HidelegCSR* processor_lib_t::gen_hideleg_csr_proto(csr_t_p csr) {
  auto hideleg = std::dynamic_pointer_cast<hideleg_csr_t>(csr);
  auto mideleg = std::dynamic_pointer_cast<basic_csr_t>(hideleg->mideleg);

  MaskedCSR* hideleg_proto = gen_masked_csr_proto(hideleg->val, hideleg->mask);
  BasicCSR*  mideleg_proto = gen_basic_csr_proto(mideleg->val);

  HidelegCSR* proto = create_protobuf<HidelegCSR>(arena);
  proto->set_allocated_msg_hideleg_csr(hideleg_proto);
  proto->set_allocated_msg_mideleg_csr(mideleg_proto);
  return proto;
}

DCSR* processor_lib_t::gen_dcsr_csr_proto(dcsr_csr_t_p csr) {
  DCSR* d = create_protobuf<DCSR>(arena);
  d->set_msg_prv     (csr->prv);
  d->set_msg_step    (csr->step);
  d->set_msg_ebreakm (csr->ebreakm);
  d->set_msg_ebreaks (csr->ebreaks);
  d->set_msg_ebreaku (csr->ebreaku);
  d->set_msg_ebreakvs(csr->ebreakvs);
  d->set_msg_ebreakvu(csr->ebreakvu);
  d->set_msg_halt    (csr->halt);
  d->set_msg_v       (csr->v);
  d->set_msg_cause   (csr->cause);
  return d;
}

McontextCSR* processor_lib_t::gen_mcontext_csr_proto(std::shared_ptr<proxy_csr_t> csr) {
  McontextCSR* mcp = create_protobuf<McontextCSR>(arena);
  auto mc = std::dynamic_pointer_cast<masked_csr_t>(csr->delegate);
  MaskedCSR* mp = gen_masked_csr_proto(mc->val, mc->mask);

  mcp->set_allocated_msg_delegate(mp);
  return mcp;
}

HenvcfgCSR* processor_lib_t::gen_henvcfg_csr_proto(std::shared_ptr<henvcfg_csr_t> csr) {
  HenvcfgCSR* henvproto = create_protobuf<HenvcfgCSR>(arena);

  auto menvcfg = std::dynamic_pointer_cast<masked_csr_t>(csr->menvcfg);
  MaskedCSR* mproto = gen_masked_csr_proto(menvcfg->val, menvcfg->mask);
  MaskedCSR* hproto = gen_masked_csr_proto(csr->val,     csr->mask);

  henvproto->set_allocated_msg_henvcfg(hproto);
  henvproto->set_allocated_msg_menvcfg(mproto);
  return henvproto;
}

StimecmpCSR* processor_lib_t::gen_stimecmp_csr_proto(std::shared_ptr<stimecmp_csr_t> csr) {
  StimecmpCSR* sp = create_protobuf<StimecmpCSR>(arena);
  BasicCSR*    bp = gen_basic_csr_proto(csr->val);

  sp->set_allocated_msg_basic_csr(bp);
  sp->set_msg_intr_mask(csr->intr_mask);
  return sp;
}


void processor_lib_t::serialize_proto(void* msg, void* aptr) {
  assert(xlen == 64);

  ArchState* aproto = (ArchState*)msg;
  google::protobuf::Arena* arena = (google::protobuf::Arena*)aptr;
  this->arena = arena;
  auto csrmap = state.csrmap;

  aproto->set_msg_pc(state.pc);
  for (int i = 0; i < NXPR; i++) {
    aproto->add_msg_xpr(state.XPR[i]);
  }

  for (int i = 0; i < NFPR; i++) {
    Float128* fp = aproto->add_msg_fpr();
    fp->set_msg_0(state.FPR[i].v[0]);
    fp->set_msg_1(state.FPR[i].v[1]);
  }

  aproto->set_msg_prv(state.prv);
  aproto->set_msg_prev_prv(state.prev_prv);
  aproto->set_msg_prv_changed(state.prv_changed);
  aproto->set_msg_v_changed(state.v_changed);
  aproto->set_msg_v(state.v);
  aproto->set_msg_prev_v(state.prev_v);

  if (state.misa) {
    MisaCSR* misa_proto = gen_misa_csr_proto(state.misa);
    aproto->set_allocated_msg_misa(misa_proto);
  }

  if (state.mstatus) {
    MstatusCSR* mstatus_proto = gen_mstatus_csr_proto(state.mstatus);
    aproto->set_allocated_msg_mstatus(mstatus_proto);
  }

  if (state.mepc) {
    auto mepc = std::dynamic_pointer_cast<epc_csr_t>(state.mepc);
    BasicCSR* mepc_proto = gen_basic_csr_proto(mepc->val);
    aproto->set_allocated_msg_mepc(mepc_proto);
  }

  if (state.mtval) {
    auto mtval = std::dynamic_pointer_cast<basic_csr_t>(state.mtval);
    BasicCSR* mtval_proto = gen_basic_csr_proto(mtval->val);
    aproto->set_allocated_msg_mtval(mtval_proto);
  }

  auto it = csrmap.find(CSR_MSCRATCH);
  if (it != csrmap.end()) {
    auto csr = std::dynamic_pointer_cast<basic_csr_t>(it->second);
    BasicCSR* proto = gen_basic_csr_proto(csr->val);
    aproto->set_allocated_msg_mscratch(proto);
  }

  if (state.mtvec) {
    auto mtvec = std::dynamic_pointer_cast<tvec_csr_t>(state.mtvec);
    BasicCSR* mtvec_proto = gen_basic_csr_proto(mtvec->val);
    aproto->set_allocated_msg_mtvec(mtvec_proto);
  }

  if (state.mcause) {
    auto mcause = std::dynamic_pointer_cast<cause_csr_t>(state.mcause);
    BasicCSR* mcause_proto = gen_basic_csr_proto(mcause->val);
    aproto->set_allocated_msg_mcause(mcause_proto);
  }

  if (state.minstret) {
    WideCntrCSR* minstret_proto = gen_wide_cntr_csr_proto(state.minstret);
    aproto->set_allocated_msg_minstret(minstret_proto);
  }

  if (state.mcycle) {
    WideCntrCSR* mcycle_proto = gen_wide_cntr_csr_proto(state.mcycle);
    aproto->set_allocated_msg_mcycle(mcycle_proto);
  }

  if (state.time) {
    auto t = state.time;
    BasicCSR* b = gen_basic_csr_proto(t->shadow_val);
    aproto->set_allocated_msg_time(b);
  }

  for (int i = 0; i < N_HPMCOUNTERS; i++) {
    if (state.mevent[i]) {
      BasicCSR* b_proto = aproto->add_msg_mevent();
      auto mevent = std::dynamic_pointer_cast<basic_csr_t>(state.mevent[i]);
      b_proto->set_msg_val(mevent->val);
    }
  }

  if (state.mie) {
    BasicCSR* mie_proto = gen_basic_csr_proto(state.mie->val);
    aproto->set_allocated_msg_mie(mie_proto);
  }

  if (state.mip) {
    BasicCSR* mip_proto = gen_basic_csr_proto(state.mip->val);
    aproto->set_allocated_msg_mip(mip_proto);
  }

  if (state.medeleg) {
    MedelegCSR* medeleg_proto = gen_medeleg_csr_proto(state.medeleg);
    aproto->set_allocated_msg_medeleg(medeleg_proto);
  }

  if (state.mcounteren) {
    auto csr = std::dynamic_pointer_cast<masked_csr_t>(state.mcounteren);
    MaskedCSR* proto = gen_masked_csr_proto(csr->val, csr->mask);
    aproto->set_allocated_msg_mcounteren(proto);
  }

  if (state.scounteren) {
    auto sen = std::dynamic_pointer_cast<masked_csr_t>(state.scounteren);
    MaskedCSR* m_proto = gen_masked_csr_proto(sen->val, sen->mask);
    aproto->set_allocated_msg_scounteren(m_proto);
  }

  if (state.sepc) {
    auto sepc = std::dynamic_pointer_cast<virtualized_csr_t>(state.sepc);
    VirtBasicCSR* sepc_proto = gen_virt_basic_csr_proto<epc_csr_t>(sepc);
    aproto->set_allocated_msg_sepc(sepc_proto);
  }

  if (state.stval) {
    auto stval = std::dynamic_pointer_cast<virtualized_csr_t>(state.stval);
    VirtBasicCSR* stval_proto = gen_virt_basic_csr_proto<basic_csr_t>(stval);
    aproto->set_allocated_msg_stval(stval_proto);
  }

  it = csrmap.find(CSR_SSCRATCH);
  if (it != csrmap.end()) {
    auto csr = std::dynamic_pointer_cast<virtualized_csr_t>(it->second);
    VirtBasicCSR* proto = gen_virt_basic_csr_proto<basic_csr_t>(csr);
    aproto->set_allocated_msg_sscratch(proto);
  }

  if (state.stvec) {
    auto stvec = std::dynamic_pointer_cast<virtualized_csr_t>(state.stvec);
    VirtBasicCSR* stvec_proto = gen_virt_basic_csr_proto<tvec_csr_t>(stvec);
    aproto->set_allocated_msg_stvec(stvec_proto);
  }

  if (state.satp) {
    VirtBasicCSR* satp_proto = gen_virt_basic_csr_proto<basic_csr_t>(state.satp);
    aproto->set_allocated_msg_satp(satp_proto);
  }

  if (state.scause) {
    auto scause = std::dynamic_pointer_cast<virtualized_csr_t>(state.scause);
    VirtBasicCSR* scause_proto = gen_virt_basic_csr_proto<basic_csr_t>(scause);
    aproto->set_allocated_msg_scause(scause_proto);
  }

  if (state.mtval2) {
    auto mtval2 = std::dynamic_pointer_cast<basic_csr_t>(state.mtval2);
    BasicCSR* mtval2_proto = gen_basic_csr_proto(mtval2->val);
    aproto->set_allocated_msg_mtval2(mtval2_proto);
  }

  if (state.mtinst) {
    auto mtinst = std::dynamic_pointer_cast<basic_csr_t>(state.mtinst);
    BasicCSR* mtinst_proto = gen_basic_csr_proto(mtinst->val);;
    aproto->set_allocated_msg_mtinst(mtinst_proto);
  }

  if (state.hstatus) {
    auto csr = std::dynamic_pointer_cast<masked_csr_t>(state.hstatus);
    MaskedCSR* proto = gen_masked_csr_proto(csr->val, csr->mask);
    aproto->set_allocated_msg_hstatus(proto);
  }

  if (state.hideleg) {
    HidelegCSR* hideleg_proto = gen_hideleg_csr_proto(state.hideleg);
    aproto->set_allocated_msg_hideleg(hideleg_proto);
  }

  if (state.hedeleg) {
    auto csr = std::dynamic_pointer_cast<masked_csr_t>(state.hedeleg);
    MaskedCSR* proto = gen_masked_csr_proto(csr->val, csr->mask);
    aproto->set_allocated_msg_hedeleg(proto);
  }

  if (state.hcounteren) {
    auto csr = std::dynamic_pointer_cast<masked_csr_t>(state.hcounteren);
    MaskedCSR* proto = gen_masked_csr_proto(csr->val, csr->mask);
    aproto->set_allocated_msg_hcounteren(proto);
  }

  if (state.htimedelta) {
    auto ht = std::dynamic_pointer_cast<basic_csr_t>(state.htimedelta);
    BasicCSR* b = gen_basic_csr_proto(ht->val);
    aproto->set_allocated_msg_htimedelta(b);
  }

  if (state.htval) {
    auto htval = std::dynamic_pointer_cast<basic_csr_t>(state.htval);
    BasicCSR* b_proto = gen_basic_csr_proto(htval->val);
    aproto->set_allocated_msg_htval(b_proto);
  }

  if (state.htinst) {
    auto htinst = std::dynamic_pointer_cast<basic_csr_t>(state.htinst);
    BasicCSR* b_proto = gen_basic_csr_proto(htinst->val);
    aproto->set_allocated_msg_htinst(b_proto);
  }

  if (state.hgatp) {
    auto hgatp = std::dynamic_pointer_cast<basic_csr_t>(state.hgatp);
    BasicCSR* b_proto = gen_basic_csr_proto(hgatp->val);
    aproto->set_allocated_msg_hgatp(b_proto);
  }

  if (state.sstatus) {
    SstatusCSR* s_proto = gen_sstatus_csr_proto(state.sstatus);
    aproto->set_allocated_msg_sstatus(s_proto);
  }

  if (state.dpc) {
    auto dpc = std::dynamic_pointer_cast<epc_csr_t>(state.dpc);
    BasicCSR* b = gen_basic_csr_proto(dpc->val);
    aproto->set_allocated_msg_dpc(b);
  }

  it = csrmap.find(CSR_DSCRATCH0);
  if (it != csrmap.end()) {
    auto csr = std::dynamic_pointer_cast<basic_csr_t>(it->second);
    BasicCSR* b = gen_basic_csr_proto(csr->val);
    aproto->set_allocated_msg_dscratch0(b);
  }

  it = csrmap.find(CSR_DSCRATCH1);
  if (it != csrmap.end()) {
    auto csr = std::dynamic_pointer_cast<basic_csr_t>(it->second);
    BasicCSR* b = gen_basic_csr_proto(csr->val);
    aproto->set_allocated_msg_dscratch1(b);
  }

  if (state.dcsr) {
    auto dcsr = std::dynamic_pointer_cast<dcsr_csr_t>(state.dcsr);
    DCSR* d = gen_dcsr_csr_proto(dcsr);
    aproto->set_allocated_msg_dcsr(d);
  }

  if (state.tselect) {
    auto tsel = std::dynamic_pointer_cast<basic_csr_t>(state.tselect);
    BasicCSR* b = gen_basic_csr_proto(tsel->val);
    aproto->set_allocated_msg_tselect(b);
  }

  if (state.scontext) {
    auto sc = std::dynamic_pointer_cast<masked_csr_t>(state.scontext);
    MaskedCSR* c = gen_masked_csr_proto(sc->val, sc->mask);
    aproto->set_allocated_msg_scontext(c);
  }

  it = csrmap.find(CSR_HCONTEXT);
  if (it != csrmap.end()) {
    auto csr = std::dynamic_pointer_cast<masked_csr_t>(it->second);
    MaskedCSR* proto = gen_masked_csr_proto(csr->val, csr->mask);
    aproto->set_allocated_msg_hcontext(proto);
  }

  if (state.mseccfg) {
    auto mseccfg = std::dynamic_pointer_cast<basic_csr_t>(state.mseccfg);
    BasicCSR* b = gen_basic_csr_proto(mseccfg->val);
    aproto->set_allocated_msg_mseccfg(b);
  }

  for (int i = 0; i < state.max_pmp; i++) {
    if (state.pmpaddr[i]) {
      PmpCSR* p_proto = aproto->add_msg_pmpaddr();

      auto pmpaddr = state.pmpaddr[i];
      BasicCSR* c_proto = gen_basic_csr_proto(pmpaddr->val);
      p_proto->set_allocated_msg_basic_csr(c_proto);
      p_proto->set_msg_cfg(pmpaddr->cfg);
      p_proto->set_msg_pmpidx(pmpaddr->pmpidx);
    }
  }

  if (state.fflags) {
    auto fflags = state.fflags;
    MaskedCSR* m_proto = gen_masked_csr_proto(fflags->val, fflags->mask);
    aproto->set_allocated_msg_fflags(m_proto);
  }

  if (state.frm) {
    auto frm = state.frm;
    MaskedCSR* m_proto = gen_masked_csr_proto(frm->val, frm->mask);
    aproto->set_allocated_msg_frm(m_proto);
  }

  if (state.senvcfg) {
    auto senv = std::dynamic_pointer_cast<masked_csr_t>(state.senvcfg);
    MaskedCSR* m_proto = gen_masked_csr_proto(senv->val, senv->mask);
    aproto->set_allocated_msg_senvcfg(m_proto);
  }

  if (state.henvcfg) {
    auto henv = std::dynamic_pointer_cast<henvcfg_csr_t>(state.henvcfg);
    HenvcfgCSR* h_proto = gen_henvcfg_csr_proto(henv);
    aproto->set_allocated_msg_henvcfg(h_proto);
  }

  for (int i = 0; i < 4; i++) {
    if (state.mstateen[i]) {
      auto csr = std::dynamic_pointer_cast<masked_csr_t>(state.mstateen[i]);
      MaskedCSR* m_proto = aproto->add_msg_mstateen();
      BasicCSR*  b_proto = gen_basic_csr_proto(csr->val);
      m_proto->set_allocated_msg_basic_csr(b_proto);
      m_proto->set_msg_mask(csr->mask);
    }
  }

  for (int i = 0; i < 4; i++) {
    if (state.sstateen[i]) {
      auto csr = std::dynamic_pointer_cast<hstateen_csr_t>(state.sstateen[i]);
      HstateenCSR* h_proto = aproto->add_msg_sstateen();
      MaskedCSR*   m_proto = gen_masked_csr_proto(csr->val, csr->mask);
      h_proto->set_allocated_msg_masked_csr(m_proto);
      h_proto->set_msg_index(csr->index);
    }
  }

  for (int i = 0; i < 4; i++) {
    if (state.hstateen[i]) {
      auto csr = std::dynamic_pointer_cast<hstateen_csr_t>(state.hstateen[i]);
      HstateenCSR* h_proto = aproto->add_msg_hstateen();
      MaskedCSR*   m_proto = gen_masked_csr_proto(csr->val, csr->mask);
      h_proto->set_msg_index(csr->index);
    }
  }

  it = csrmap.find(CSR_MNSCRATCH);
  if (it != csrmap.end()) {
    auto csr = std::dynamic_pointer_cast<basic_csr_t>(it->second);
    BasicCSR* proto = gen_basic_csr_proto(csr->val);
    aproto->set_allocated_msg_mnscratch(proto);
  }

  if (state.mnepc) {
    auto csr = std::dynamic_pointer_cast<basic_csr_t>(state.mnepc);
    BasicCSR* proto = gen_basic_csr_proto(csr->val);
    aproto->set_allocated_msg_mnepc(proto);
  }

  if (state.mnstatus) {
    auto csr = std::dynamic_pointer_cast<basic_csr_t>(state.mnstatus);
    BasicCSR* proto = gen_basic_csr_proto(csr->val);
    aproto->set_allocated_msg_mnstatus(proto);
  }

  if (state.stimecmp) {
    auto st = std::dynamic_pointer_cast<stimecmp_csr_t>(state.stimecmp);
    StimecmpCSR* sp = gen_stimecmp_csr_proto(st);
    aproto->set_allocated_msg_stimecmp(sp);
  }

  if (state.vstimecmp) {
    auto st = std::dynamic_pointer_cast<stimecmp_csr_t>(state.vstimecmp);
    StimecmpCSR* sp = gen_stimecmp_csr_proto(st);
    aproto->set_allocated_msg_vstimecmp(sp);
  }

  if (state.jvt) {
    auto jvt = std::dynamic_pointer_cast<basic_csr_t>(state.jvt);
    BasicCSR* b = gen_basic_csr_proto(jvt->val);
    aproto->set_allocated_msg_jvt(b);
  }

  it = csrmap.find(CSR_MISELECT);
  if (it != csrmap.end()) {
    auto csr = std::dynamic_pointer_cast<basic_csr_t>(it->second);
    BasicCSR* proto = gen_basic_csr_proto(csr->val);
    aproto->set_allocated_msg_miselect(proto);
  }

  it = csrmap.find(CSR_SISELECT);
  if (it != csrmap.end()) {
    auto csr = std::dynamic_pointer_cast<virtualized_csr_t>(it->second);
    VirtBasicCSR* proto = gen_virt_basic_csr_proto<basic_csr_t>(csr);
    aproto->set_allocated_msg_siselect(proto);
  }

  if (state.srmcfg) {
    auto csr = std::dynamic_pointer_cast<masked_csr_t>(state.srmcfg);
    MaskedCSR* proto = gen_masked_csr_proto(csr->val, csr->mask);
    aproto->set_allocated_msg_srmcfg(proto);
  }

  aproto->set_msg_debug_mode(state.debug_mode);
  aproto->set_msg_serialized(state.serialized);
  aproto->set_msg_single_step(state.single_step);
  aproto->set_msg_last_inst_priv(state.last_inst_priv);
  aproto->set_msg_last_inst_xlen(state.last_inst_xlen);
  aproto->set_msg_last_inst_flen(state.last_inst_flen);
}


template <class T>
void processor_lib_t::set_basic_csr_from_proto(T& csr, const BasicCSR& proto) {
  csr.val = proto.msg_val();
}

void processor_lib_t::set_medeleg_csr_from_proto(medeleg_csr_t& csr,
                                             const MedelegCSR& proto) {
  set_basic_csr_from_proto<basic_csr_t>(csr, proto.msg_basic_csr());
  csr.hypervisor_exceptions = proto.msg_hypervisor_exceptions();
}

void processor_lib_t::set_misa_csr_from_proto(misa_csr_t& csr,
                                          const MisaCSR& proto) {
  set_basic_csr_from_proto<basic_csr_t>(csr, proto.msg_basic_csr());
  csr.max_isa    = proto.msg_max_isa();
  csr.write_mask = proto.msg_write_mask();
}

void processor_lib_t::set_basestatus_csr_from_proto(base_status_csr_t& csr,
                                                const BaseStatusCSR& proto) {
  csr.has_page = proto.msg_has_page();
  csr.sstatus_write_mask = proto.msg_sstatus_write_mask();
  csr.sstatus_read_mask  = proto.msg_sstatus_read_mask();
}

void processor_lib_t::set_mstatus_csr_from_proto(mstatus_csr_t& csr,
                                             const MstatusCSR& proto) {
  set_basestatus_csr_from_proto(csr, proto.msg_base_status_csr());
  csr.val = proto.msg_val();
}

void processor_lib_t::set_sstatus_proxy_csr_from_proto(sstatus_proxy_csr_t& csr,
                                                   const SstatusProxyCSR& proto) {
  set_mstatus_csr_from_proto(*(csr.mstatus), proto.msg_mstatus_csr());
  set_basestatus_csr_from_proto(csr, proto.msg_base_status_csr());
}

void processor_lib_t::set_vsstatus_csr_from_proto(vsstatus_csr_t& csr,
                                              const VsstatusCSR& proto) {
  set_basestatus_csr_from_proto(csr, proto.msg_base_status_csr());
  csr.val = proto.msg_val();
}

void processor_lib_t::set_sstatus_csr_from_proto(sstatus_csr_t& csr,
                                             const SstatusCSR& proto) {
  set_sstatus_proxy_csr_from_proto(*(csr.orig_sstatus), proto.msg_orig_sstatus());
  set_vsstatus_csr_from_proto     (*(csr.virt_sstatus), proto.msg_virt_sstatus());
}

void processor_lib_t::set_mcause_csr_from_proto(cause_csr_t& csr,
                                            const BasicCSR& proto) {
  set_basic_csr_from_proto<basic_csr_t>(csr, proto);
}

void processor_lib_t::set_masked_csr_from_proto(masked_csr_t& csr,
                                            const MaskedCSR& proto) {
  set_basic_csr_from_proto<basic_csr_t>(csr, proto.msg_basic_csr());
  csr.mask = proto.msg_mask();
}

void processor_lib_t::set_smcntrpmf_csr_from_proto(smcntrpmf_csr_t& csr,
                                               const SmcntrpmfCSR& proto) {
  set_masked_csr_from_proto(csr, proto.msg_masked_csr());
  if (proto.has_msg_prev_val()) {
    auto opt = proto.msg_prev_val();
    csr.prev_val = opt.msg_val();
  }
}

void processor_lib_t::set_widecntr_csr_from_proto(wide_counter_csr_t& csr,
                                              const WideCntrCSR& proto) {
  csr.val = proto.msg_val();
  set_smcntrpmf_csr_from_proto(*(csr.config_csr), proto.msg_config_csr());
}

template <class T>
void processor_lib_t::set_virt_basic_csr_from_proto(virtualized_csr_t& csr,
                                                T& vcsr,
                                                const VirtBasicCSR& proto) {
  set_basic_csr_from_proto<T>(*std::dynamic_pointer_cast<T>(csr.orig_csr), proto.msg_nonvirt_csr());
  set_basic_csr_from_proto<T>(*std::dynamic_pointer_cast<T>(csr.virt_csr), proto.msg_virt_csr());
  set_basic_csr_from_proto<T>(vcsr, proto.msg_virt_csr());
}

void processor_lib_t::set_hideleg_csr_from_proto(hideleg_csr_t& csr,
                                             const HidelegCSR& proto) {
  set_masked_csr_from_proto(csr, proto.msg_hideleg_csr());

  auto mideleg = std::dynamic_pointer_cast<basic_csr_t>(csr.mideleg);
  set_basic_csr_from_proto(*mideleg, proto.msg_mideleg_csr());
}

void processor_lib_t::set_dcsr_csr_from_proto(dcsr_csr_t& csr, const DCSR& proto) {
  csr.prv      = proto.msg_prv();
  csr.step     = proto.msg_step();
  csr.ebreakm  = proto.msg_ebreakm();
  csr.ebreaks  = proto.msg_ebreaks();
  csr.ebreaku  = proto.msg_ebreaku();
  csr.ebreakvs = proto.msg_ebreakvs();
  csr.ebreakvu = proto.msg_ebreakvu();
  csr.halt     = proto.msg_halt();
  csr.v        = proto.msg_v();
  csr.cause    = proto.msg_cause();
}

void processor_lib_t::set_mcontext_csr_from_proto(proxy_csr_t& csr,
                                              const McontextCSR& proto) {
  auto delegate = std::dynamic_pointer_cast<masked_csr_t>(csr.delegate);
  set_masked_csr_from_proto(*delegate, proto.msg_delegate());
}

void processor_lib_t::set_pmpaddr_csr_from_proto(pmpaddr_csr_t& csr,
                                             const PmpCSR& proto) {
  csr.val = proto.msg_basic_csr().msg_val();
  csr.cfg = proto.msg_cfg();
  csr.pmpidx = proto.msg_pmpidx();
}

void processor_lib_t::set_henvcfg_csr_from_proto(henvcfg_csr_t& csr,
                                             const HenvcfgCSR& proto) {
  set_masked_csr_from_proto(csr, proto.msg_henvcfg());
  auto menv = std::dynamic_pointer_cast<masked_csr_t>(csr.menvcfg);
  set_masked_csr_from_proto(*menv, proto.msg_menvcfg());
}

void processor_lib_t::set_hstateen_csr_from_proto(hstateen_csr_t& csr,
                                              const HstateenCSR& proto) {
  set_masked_csr_from_proto(csr, proto.msg_masked_csr());
  csr.index = proto.msg_index();
}

void processor_lib_t::set_time_counter_csr_from_proto(time_counter_csr_t& csr,
                                                  const BasicCSR& proto) {
  csr.shadow_val = proto.msg_val();
}

void processor_lib_t::set_stimecmp_csr_from_proto(stimecmp_csr_t& csr,
                                              const StimecmpCSR& proto) {
  set_basic_csr_from_proto<basic_csr_t>(csr, proto.msg_basic_csr());
  csr.intr_mask = proto.msg_intr_mask();
}

void processor_lib_t::deserialize_proto(void* msg) {
  assert(xlen == 64);
  ArchState* aproto = (ArchState*)msg;
  auto csrmap = state.csrmap;

  state.pc = aproto->msg_pc();

  for (int i = 0, cnt = aproto->msg_xpr_size(); i < cnt; i++) {
    state.XPR.write(i, aproto->msg_xpr(i));
  }

  for (int i = 0, cnt = aproto->msg_fpr_size(); i < cnt; i++) {
    auto fpr_msg = aproto->msg_fpr(i);

    float128_t fp;
    fp.v[0] = fpr_msg.msg_0();
    fp.v[1] = fpr_msg.msg_1();
    state.FPR.write(i, fp);
  }

  state.prv         = aproto->msg_prv();
  state.prev_prv    = aproto->msg_prev_prv();
  state.prv_changed = aproto->msg_prv_changed();
  state.v_changed   = aproto->msg_v_changed();
  state.v           = aproto->msg_v();
  state.prev_v      = aproto->msg_prev_v();

/* std::cout << " pc: " << state.pc */
/* << " prv: " << state.prv */
/* << " prev_prv: " << state.prev_prv */
/* << " prv_changed: " << state.prv_changed */
/* << " v_changed: " << state.v_changed */
/* << " v: " << state.v */
/* << " prev_v: " << state.prev_v << std::endl; */

  if (aproto->has_msg_misa()) {
    set_misa_csr_from_proto(*(state.misa), aproto->msg_misa());
  }

  if (aproto->has_msg_mstatus()) {
    set_mstatus_csr_from_proto(*(state.mstatus), aproto->msg_mstatus());
  }

  if (aproto->has_msg_mepc()) {
    auto mepc = std::dynamic_pointer_cast<epc_csr_t>(state.mepc);
    set_basic_csr_from_proto<epc_csr_t>(*mepc, aproto->msg_mepc());
  }

  if (aproto->has_msg_mtval()) {
    auto mtval = std::dynamic_pointer_cast<basic_csr_t>(state.mtval);
    set_basic_csr_from_proto<basic_csr_t>(*mtval, aproto->msg_mtval());
  }

  if (aproto->has_msg_mscratch()) {
    auto it = csrmap.find(CSR_MSCRATCH);
    assert(it != csrmap.end());
    auto csr = std::dynamic_pointer_cast<basic_csr_t>(it->second);
    set_basic_csr_from_proto<basic_csr_t>(*csr, aproto->msg_mscratch());
  }

  if (aproto->has_msg_mtvec()) {
    auto mtvec = std::dynamic_pointer_cast<tvec_csr_t>(state.mtvec);
    set_basic_csr_from_proto<tvec_csr_t>(*mtvec, aproto->msg_mtvec());
  }

  if (aproto->has_msg_mcause()) {
    auto mcause = std::dynamic_pointer_cast<cause_csr_t>(state.mcause);
    set_basic_csr_from_proto<basic_csr_t>(*mcause, aproto->msg_mcause());
  }

  if (aproto->has_msg_minstret()) {
    auto minstret = state.minstret;
    set_widecntr_csr_from_proto(*minstret, aproto->msg_minstret());
  }

  if (aproto->has_msg_mcycle()) {
    auto mcycle = state.mcycle;
    set_widecntr_csr_from_proto(*mcycle, aproto->msg_mcycle());
  }

  if (aproto->has_msg_time()) {
    set_time_counter_csr_from_proto(*(state.time), aproto->msg_time());
  }


  if (aproto->msg_mevent_size() > 0) {
    int cnt = aproto->msg_mevent_size();
    assert(cnt <= N_HPMCOUNTERS);
    for (int i = 0; i < cnt; i++) {
      auto mevent = std::dynamic_pointer_cast<basic_csr_t>(state.mevent[i]);
      set_basic_csr_from_proto<basic_csr_t>(*mevent, aproto->msg_mevent(i));
    }
  }

  if (aproto->has_msg_mie()) {
    auto mie = state.mie;
    set_basic_csr_from_proto<mip_or_mie_csr_t>(*mie, aproto->msg_mie());
  }

  if (aproto->has_msg_mip()) {
    auto mip = state.mip;
    set_basic_csr_from_proto<mip_or_mie_csr_t>(*mip, aproto->msg_mip());
  }

  if (aproto->has_msg_medeleg()) {
    auto csr  = std::dynamic_pointer_cast<medeleg_csr_t>(state.medeleg);
    set_medeleg_csr_from_proto(*csr, aproto->msg_medeleg());
  }

  if (aproto->has_msg_mcounteren()) {
    auto mcounteren = std::dynamic_pointer_cast<masked_csr_t>(state.mcounteren);
    set_masked_csr_from_proto(*mcounteren, aproto->msg_mcounteren());
  }

  if (aproto->has_msg_scounteren()) {
    auto sen = std::dynamic_pointer_cast<masked_csr_t>(state.scounteren);
    set_masked_csr_from_proto(*sen, aproto->msg_scounteren());
  }

  if (aproto->has_msg_sepc()) {
    auto vsepc = std::dynamic_pointer_cast<epc_csr_t>(state.vsepc);
    auto sepc  = std::dynamic_pointer_cast<virtualized_csr_t>(state.sepc);
    set_virt_basic_csr_from_proto<epc_csr_t>(*sepc, *vsepc, aproto->msg_sepc());
  }

  if (aproto->has_msg_stval()) {
    auto vstval = std::dynamic_pointer_cast<basic_csr_t>(state.vstval);
    auto stval  = std::dynamic_pointer_cast<virtualized_csr_t>(state.stval);
    set_virt_basic_csr_from_proto<basic_csr_t>(*stval, *vstval, aproto->msg_stval());
  }

  if (aproto->has_msg_sscratch()) {
    auto it = csrmap.find(CSR_SSCRATCH);
    assert(it != csrmap.end());
    auto ss = std::dynamic_pointer_cast<virtualized_csr_t>(it->second);

    it = csrmap.find(CSR_VSSCRATCH);
    assert(it != csrmap.end());
    auto vs = std::dynamic_pointer_cast<basic_csr_t>(it->second);

    set_virt_basic_csr_from_proto<basic_csr_t>(*ss, *vs, aproto->msg_sscratch());
  }

  if (aproto->has_msg_stvec()) {
    auto vstvec = std::dynamic_pointer_cast<tvec_csr_t>(state.vstvec);
    auto stvec  = std::dynamic_pointer_cast<virtualized_csr_t>(state.stvec);
    set_virt_basic_csr_from_proto<tvec_csr_t>(*stvec, *vstvec, aproto->msg_stvec());
  }

  if (aproto->has_msg_satp()) {
    auto vsatp = std::dynamic_pointer_cast<basic_csr_t>(state.vsatp);
    auto satp  = state.satp;
    set_virt_basic_csr_from_proto<basic_csr_t>(*satp, *vsatp, aproto->msg_satp());
  }

  if (aproto->has_msg_scause()) {
    auto vscause = std::dynamic_pointer_cast<basic_csr_t>(state.vscause);
    auto scause = std::dynamic_pointer_cast<virtualized_csr_t>(state.scause);
    set_virt_basic_csr_from_proto<basic_csr_t>(*scause, *vscause, aproto->msg_scause());
  }

  if (aproto->has_msg_mtval2()) {
    auto mtval2 = std::dynamic_pointer_cast<basic_csr_t>(state.mtval2);
    set_basic_csr_from_proto<basic_csr_t>(*mtval2, aproto->msg_mtval2());
  }

  if (aproto->has_msg_mtinst()) {
    auto mtinst = std::dynamic_pointer_cast<basic_csr_t>(state.mtinst);
    set_basic_csr_from_proto<basic_csr_t>(*mtinst, aproto->msg_mtinst());
  }

  if (aproto->has_msg_hstatus()) {
    auto hstatus = std::dynamic_pointer_cast<masked_csr_t>(state.hstatus);
    set_masked_csr_from_proto(*hstatus, aproto->msg_hstatus());
  }

  if (aproto->has_msg_hideleg()) {
    auto hideleg = std::dynamic_pointer_cast<hideleg_csr_t>(state.hideleg);
    auto mideleg = std::dynamic_pointer_cast<mideleg_csr_t>(hideleg->mideleg);
    set_hideleg_csr_from_proto(*hideleg, aproto->msg_hideleg());
  }

  if (aproto->has_msg_hedeleg()) {
    auto hedeleg = std::dynamic_pointer_cast<masked_csr_t>(state.hedeleg);
    set_masked_csr_from_proto(*hedeleg, aproto->msg_hedeleg());
  }

  if (aproto->has_msg_hcounteren()) {
    auto hcounteren = std::dynamic_pointer_cast<masked_csr_t>(state.hcounteren);
    set_masked_csr_from_proto(*hcounteren, aproto->msg_hcounteren());
  }

  if (aproto->has_msg_htimedelta()) {
    auto ht = std::dynamic_pointer_cast<basic_csr_t>(state.htimedelta);
    set_basic_csr_from_proto<basic_csr_t>(*ht, aproto->msg_htimedelta());
  }

  if (aproto->has_msg_htval()) {
    auto htval = std::dynamic_pointer_cast<basic_csr_t>(state.htval);
    set_basic_csr_from_proto<basic_csr_t>(*htval, aproto->msg_htval());
  }

  if (aproto->has_msg_htinst()) {
    auto htinst = std::dynamic_pointer_cast<basic_csr_t>(state.htinst);
    set_basic_csr_from_proto<basic_csr_t>(*htinst, aproto->msg_htinst());
  }

  if (aproto->has_msg_hgatp()) {
    auto hgatp = std::dynamic_pointer_cast<basic_csr_t>(state.hgatp);
    set_basic_csr_from_proto<basic_csr_t>(*hgatp, aproto->msg_hgatp());
  }

  if (aproto->has_msg_sstatus()) {
    set_sstatus_csr_from_proto(*(state.sstatus), aproto->msg_sstatus());
  }

  if (aproto->has_msg_dpc()) {
    auto dpc = std::dynamic_pointer_cast<epc_csr_t>(state.dpc);
    set_basic_csr_from_proto<epc_csr_t>(*dpc, aproto->msg_dpc());
  }

  if (aproto->has_msg_dscratch0()) {
    auto it = csrmap.find(CSR_DSCRATCH0);
    assert(it != csrmap.end());
    auto csr = std::dynamic_pointer_cast<basic_csr_t>(it->second);
    set_basic_csr_from_proto(*csr, aproto->msg_dscratch0());
  }

  if (aproto->has_msg_dscratch1()) {
    auto it = csrmap.find(CSR_DSCRATCH1);
    assert(it != csrmap.end());
    auto csr = std::dynamic_pointer_cast<basic_csr_t>(it->second);
    set_basic_csr_from_proto(*csr, aproto->msg_dscratch1());
  }

  if (aproto->has_msg_dcsr()) {
    auto dcsr = std::dynamic_pointer_cast<dcsr_csr_t>(state.dcsr);
    set_dcsr_csr_from_proto(*dcsr, aproto->msg_dcsr());
  }

  if (aproto->has_msg_tselect()) {
    auto tsel = std::dynamic_pointer_cast<tselect_csr_t>(state.tselect);
    set_basic_csr_from_proto(*tsel, aproto->msg_tselect());
  }

  if (aproto->has_msg_scontext()) {
    auto sc = std::dynamic_pointer_cast<masked_csr_t>(state.scontext);
    set_masked_csr_from_proto(*sc, aproto->msg_scontext());
  }

  if (aproto->has_msg_hcontext()) {
    auto it = csrmap.find(CSR_HCONTEXT);
    assert(it != csrmap.end());
    auto csr = std::dynamic_pointer_cast<masked_csr_t>(it->second);
    set_masked_csr_from_proto(*csr, aproto->msg_hcontext());
  }

  if (aproto->has_msg_mseccfg()) {
    auto mseccfg = std::dynamic_pointer_cast<basic_csr_t>(state.mseccfg);
    set_basic_csr_from_proto<basic_csr_t>(*mseccfg, aproto->msg_mseccfg());
  }

  if (aproto->msg_pmpaddr_size() > 0) {
    int cnt = aproto->msg_pmpaddr_size();
    assert(cnt <= state.max_pmp);
    for (int i = 0; i < cnt; i++) {
      auto pmpaddr = state.pmpaddr[i];
      set_pmpaddr_csr_from_proto(*pmpaddr, aproto->msg_pmpaddr(i));
    }
  }

  if (aproto->has_msg_fflags()) {
    set_masked_csr_from_proto(*(state.fflags), aproto->msg_fflags());
  }

  if (aproto->has_msg_frm()) {
    set_masked_csr_from_proto(*(state.frm), aproto->msg_frm());
  }

  if (aproto->has_msg_senvcfg()) {
    auto senv = std::dynamic_pointer_cast<masked_csr_t>(state.senvcfg);
    set_masked_csr_from_proto(*senv, aproto->msg_senvcfg());
  }

  if (aproto->has_msg_henvcfg()) {
    auto henv = std::dynamic_pointer_cast<henvcfg_csr_t>(state.henvcfg);
    set_henvcfg_csr_from_proto(*henv, aproto->msg_henvcfg());
  }

  if (aproto->msg_mstateen_size() > 0) {
    int cnt = aproto->msg_mstateen_size();
    assert(cnt <= 4);
    for (int i = 0; i < cnt; i++) {
      auto mstateen = std::dynamic_pointer_cast<masked_csr_t>(state.mstateen[i]);
      set_masked_csr_from_proto(*mstateen, aproto->msg_mstateen(i));
    }
  }

  if (aproto->msg_sstateen_size() > 0) {
    int cnt = aproto->msg_sstateen_size();
    assert(cnt <= 4);
    for (int i = 0; i < cnt; i++) {
      auto sstateen = std::dynamic_pointer_cast<hstateen_csr_t>(state.sstateen[i]);
      set_hstateen_csr_from_proto(*sstateen, aproto->msg_sstateen(i));
    }
  }

  if (aproto->msg_hstateen_size() > 0) {
    int cnt = aproto->msg_hstateen_size();
    assert(cnt <= 4);
    for (int i = 0; i < cnt; i++) {
      auto hstateen = std::dynamic_pointer_cast<hstateen_csr_t>(state.hstateen[i]);
      set_hstateen_csr_from_proto(*hstateen, aproto->msg_hstateen(i));
    }
  }

  if (aproto->has_msg_mnscratch()) {
    auto it = csrmap.find(CSR_MNSCRATCH);
    assert(it != csrmap.end());
    auto csr = std::dynamic_pointer_cast<basic_csr_t>(it->second);
    set_basic_csr_from_proto(*csr, aproto->msg_mnscratch());
  }

  if (aproto->has_msg_mnepc()) {
    auto mnepc = std::dynamic_pointer_cast<epc_csr_t>(state.mnepc);
    set_basic_csr_from_proto<epc_csr_t>(*mnepc, aproto->msg_mnepc());
  }

  if (aproto->has_msg_mnstatus()) {
    auto mnstatus = std::dynamic_pointer_cast<basic_csr_t>(state.mnstatus);
    set_basic_csr_from_proto<basic_csr_t>(*mnstatus, aproto->msg_mnstatus());
  }

  if (aproto->has_msg_stimecmp()) {
    auto st = std::dynamic_pointer_cast<stimecmp_csr_t>(state.stimecmp);
    set_stimecmp_csr_from_proto(*st, aproto->msg_stimecmp());
  }

  if (aproto->has_msg_vstimecmp()) {
    auto st = std::dynamic_pointer_cast<stimecmp_csr_t>(state.vstimecmp);
    set_stimecmp_csr_from_proto(*st, aproto->msg_vstimecmp());
  }

  if (aproto->has_msg_jvt()) {
    auto jvt = std::dynamic_pointer_cast<basic_csr_t>(state.jvt);
    set_basic_csr_from_proto<basic_csr_t>(*jvt, aproto->msg_jvt());
  }

  if (aproto->has_msg_miselect()) {
    auto it = csrmap.find(CSR_MISELECT);
    assert(it != csrmap.end());
    auto csr = std::dynamic_pointer_cast<basic_csr_t>(it->second);
    set_basic_csr_from_proto(*csr, aproto->msg_miselect());
  }

  if (aproto->has_msg_siselect()) {
    auto it = csrmap.find(CSR_SISELECT);
    assert(it != csrmap.end());
    auto ss = std::dynamic_pointer_cast<virtualized_csr_t>(it->second);

    it = csrmap.find(CSR_VSISELECT);
    assert(it != csrmap.end());
    auto vs = std::dynamic_pointer_cast<basic_csr_t>(it->second);

    set_virt_basic_csr_from_proto<basic_csr_t>(*ss, *vs, aproto->msg_siselect());
  }

  if (aproto->has_msg_srmcfg()) {
    auto csr = std::dynamic_pointer_cast<masked_csr_t>(state.srmcfg);
    set_masked_csr_from_proto(*csr, aproto->msg_srmcfg());
  }

  state.debug_mode = aproto->msg_debug_mode();

  state.serialized     = aproto->msg_serialized();

  auto ss = aproto->msg_single_step();
  if (ss == 0) {
    state.single_step = state_t::STEP_NONE;
  } else if (ss == 1) {
    state.single_step = state_t::STEP_STEPPING;
  } else if (ss == 2) {
    state.single_step = state_t::STEP_STEPPED;
  } else {
    assert(false);
  }
  state.last_inst_priv = aproto->msg_last_inst_priv();
  state.last_inst_xlen = aproto->msg_last_inst_xlen();
  state.last_inst_flen = aproto->msg_last_inst_flen();

  google::protobuf::ShutdownProtobufLibrary();
}
