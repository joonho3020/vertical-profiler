// See LICENSE for license details.

#include <riscv/abstract_device.h>
#include <riscv/decode.h>
#include <riscv/sim.h>
#include <riscv/mmu.h>
#include <riscv/dts.h>
#include <riscv/platform.h>
#include <riscv/platform.h>
#include <fdt/libfdt.h>

#include <cstdint>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <cstdlib>
#include <cassert>
#include <inttypes.h>
#include <string>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>

#include "ganged_devices.h"
#include "sim_lib.h"
#include "mmu_lib.h"
#include "../lib/string_parser.h"

extern device_factory_t* clint_factory;
extern device_factory_t* plic_factory;
extern device_factory_t* ns16550_lib_factory;

sim_lib_t::sim_lib_t(const cfg_t *cfg, bool halted,
        std::vector<std::pair<reg_t, abstract_mem_t*>> mems,
        std::vector<device_factory_t*> plugin_device_factories,
        const std::vector<std::string>& args,
        const debug_module_config_t &dm_config, const char *log_path,
        bool dtb_enabled, const char *dtb_file,
        bool socket_enabled,
        FILE *cmd_file,
        const char* rtl_tracefile_name,
        bool serialize_mem)
  : sim_t(cfg, halted, mems, plugin_device_factories, args, dm_config,
          log_path, dtb_enabled, dtb_file, socket_enabled, true, cmd_file),
    rtl_tracefile_name(rtl_tracefile_name),
    serialize_mem(serialize_mem)
{
  arena = new google::protobuf::Arena();

  auto enq_func = [](std::queue<reg_t>* q, uint64_t x) { q->push(x); };
  fromhost_callback = std::bind(enq_func, &fromhost_queue, std::placeholders::_1);

  sout_.rdbuf(std::cerr.rdbuf()); // debug output goes to stderr by default

  for (auto& x : mems)
    bus.add_device(x.first, x.second);

  bus.add_device(DEBUG_START, &debug_module);

  socketif = NULL;
#ifdef HAVE_BOOST_ASIO
  if (socket_enabled) {
    socketif = new socketif_t();
  }
#else
  if (socket_enabled) {
    fputs("Socket support requires compilation with boost asio; "
          "please rebuild the riscv-isa-sim project using "
          "\"configure --with-boost-asio\".\n",
          stderr);
    abort();
  }
#endif

#ifndef RISCV_ENABLE_DUAL_ENDIAN
  if (cfg->endianness != endianness_little) {
    fputs("Big-endian support has not been prroperly enabled; "
          "please rebuild the riscv-isa-sim project using "
          "\"configure --enable-dual-endian\".\n",
          stderr);
    abort();
  }
#endif

  debug_mmu = new mmu_lib_t(this, cfg->endianness, NULL);

  for (size_t i = 0; i < cfg->nprocs(); i++) {
    procs[i] = new processor_lib_t(&isa, cfg, this, cfg->hartids[i], halted,
                               log_file.get(), sout_);
    harts[cfg->hartids[i]] = procs[i];
  }

  // When running without using a dtb, skip the fdt-based configuration steps
  if (!dtb_enabled) return;

  bool from_rtl_trace = (rtl_tracefile_name != nullptr);

  // Only make a CLINT (Core-Local INTerrupt controller) and PLIC (Platform-
  // Level-Interrupt-Controller) if they are specified in the device tree
  // configuration.
  //
  // This isn't *quite* as general as we could get (because you might have one
  // that's not bus-accessible), but it should handle the normal use cases. In
  // particular, the default device tree configuration that you get without
  // setting the dtb_file argument has one.
  std::vector<const device_factory_t*> device_factories = {};
  device_factories.push_back(clint_factory); // clint must be element 0
  device_factories.push_back(plic_factory);  // plic must be element 1
  if (!from_rtl_trace) {
    device_factories.push_back(ns16550_lib_factory);
  }

  device_factories.insert(device_factories.end(),
                          plugin_device_factories.begin(),
                          plugin_device_factories.end());

  // Load dtb_file if provided, otherwise self-generate a dts/dtb
  if (dtb_file) {
    std::ifstream fin(dtb_file, std::ios::binary);
    if (!fin.good()) {
      std::cerr << "can't find dtb file: " << dtb_file << std::endl;
      exit(-1);
    }
    std::stringstream strstream;
    strstream << fin.rdbuf();
    dtb = strstream.str();
  } else {
    std::pair<reg_t, reg_t> initrd_bounds = cfg->initrd_bounds;
    std::string device_nodes;
    for (const device_factory_t *factory : device_factories)
      device_nodes.append(factory->generate_dts(this));
    dts = make_dts(INSNS_PER_RTC_TICK, CPU_HZ,
                   initrd_bounds.first, initrd_bounds.second,
                   cfg->bootargs, cfg->pmpregions, cfg->pmpgranularity,
                   procs, mems, device_nodes);
    dtb = dts_compile(dts);
  }

  int fdt_code = fdt_check_header(dtb.c_str());
  if (fdt_code) {
    std::cerr << "Failed to read DTB from ";
    if (!dtb_file) {
      std::cerr << "auto-generated DTS string";
    } else {
      std::cerr << "`" << dtb_file << "'";
    }
    std::cerr << ": " << fdt_strerror(fdt_code) << ".\n";
    exit(-1);
  }

  void *fdt = (void *)dtb.c_str();

  for (size_t i = 0; i < device_factories.size(); i++) {
    const device_factory_t *factory = device_factories[i];
    reg_t device_base = 0;
    abstract_device_t* device = factory->parse_from_fdt(fdt, this, &device_base);
    if (device) {
      assert(device_base);

      std::shared_ptr<abstract_device_t> dev_ptr(device);
      if (from_rtl_trace) {
        if (i == 0) {
          std::shared_ptr<clint_t> base_clint = std::static_pointer_cast<clint_t>(dev_ptr);
          abstract_device_t* cg = new clint_ganged_t(this, base_clint->freq_hz, base_clint->real_time);
          std::shared_ptr<abstract_device_t> cg_ptr(cg);
          clint = std::static_pointer_cast<clint_t>(cg_ptr);

          std::shared_ptr<ganged_device_t> gdev = std::make_shared<ganged_device_t>(cg_ptr);
          ganged_devs.push_back(gdev);
          add_device(device_base, gdev);
        } else if (i == 1) {
          std::shared_ptr<plic_t> base_plic = std::static_pointer_cast<plic_t>(dev_ptr);
          abstract_device_t* pg = new plic_ganged_t(this, base_plic->num_ids - 1);
          std::shared_ptr<abstract_device_t> pg_ptr(pg);
          plic = std::static_pointer_cast<plic_t>(pg_ptr);

          std::shared_ptr<ganged_device_t> gdev = std::make_shared<ganged_device_t>(pg_ptr);
          ganged_devs.push_back(gdev);
          add_device(device_base, gdev);
        } else {
          std::shared_ptr<ganged_device_t> gdev = std::make_shared<ganged_device_t>(dev_ptr);
          ganged_devs.push_back(gdev);
          add_device(device_base, gdev);
        }
      } else {
        add_device(device_base, dev_ptr);
        if (i == 0) // clint_factory
          clint = std::static_pointer_cast<clint_t>(dev_ptr);
        else if (i == 1) // plic_factory
          plic = std::static_pointer_cast<plic_t>(dev_ptr);
      }
    }
  }

  if (from_rtl_trace) {
    firesim_bootrom.resize(ROCKETCHIP_BOOTROM_SIZE);
    std::shared_ptr<rom_device_t> bootrom_ptr = std::make_shared<rom_device_t>(firesim_bootrom);
    std::shared_ptr<ganged_device_t> rom_ptr = std::make_shared<ganged_device_t>(bootrom_ptr);
    ganged_devs.push_back(rom_ptr);
    add_device(ROCKETCHIP_BOOTROM_BASE, rom_ptr);
  }

  std::shared_ptr<mem_t> boot_addr_reg = std::make_shared<mem_t>(ROCKETCHIP_BOOTADDR_BASE);
  boot_addr_reg.get()->store(0, 8, (const uint8_t*)(&ROCKETCHIP_BOOTROM_BASE));
  add_device(ROCKETCHIP_BOOTADDR_BASE, boot_addr_reg);

  //per core attribute
  int cpu_offset = 0, cpu_map_offset, rc;
  size_t cpu_idx = 0;
  cpu_offset = fdt_get_offset(fdt, "/cpus");
  cpu_map_offset = fdt_get_offset(fdt, "/cpus/cpu-map");
  if (cpu_offset < 0)
    return;

  for (cpu_offset = fdt_get_first_subnode(fdt, cpu_offset); cpu_offset >= 0;
       cpu_offset = fdt_get_next_subnode(fdt, cpu_offset)) {

    if (!(cpu_map_offset < 0) && cpu_offset == cpu_map_offset)
      continue;

    if (cpu_idx >= nprocs())
      break;

    //handle pmp
    reg_t pmp_num, pmp_granularity;
    if (fdt_parse_pmp_num(fdt, cpu_offset, &pmp_num) != 0)
      pmp_num = 0;
    procs[cpu_idx]->set_pmp_num(pmp_num);

    if (fdt_parse_pmp_alignment(fdt, cpu_offset, &pmp_granularity) == 0) {
      procs[cpu_idx]->set_pmp_granularity(pmp_granularity);
    }

    //handle mmu-type
    const char *mmu_type;
    rc = fdt_parse_mmu_type(fdt, cpu_offset, &mmu_type);
    if (rc == 0) {
      procs[cpu_idx]->set_mmu_capability(IMPL_MMU_SBARE);
      if (strncmp(mmu_type, "riscv,sv32", strlen("riscv,sv32")) == 0) {
        procs[cpu_idx]->set_mmu_capability(IMPL_MMU_SV32);
      } else if (strncmp(mmu_type, "riscv,sv39", strlen("riscv,sv39")) == 0) {
        procs[cpu_idx]->set_mmu_capability(IMPL_MMU_SV39);
      } else if (strncmp(mmu_type, "riscv,sv48", strlen("riscv,sv48")) == 0) {
        procs[cpu_idx]->set_mmu_capability(IMPL_MMU_SV48);
      } else if (strncmp(mmu_type, "riscv,sv57", strlen("riscv,sv57")) == 0) {
        procs[cpu_idx]->set_mmu_capability(IMPL_MMU_SV57);
      } else if (strncmp(mmu_type, "riscv,sbare", strlen("riscv,sbare")) == 0) {
        //has been set in the beginning
      } else {
        std::cerr << "core ("
                  << cpu_idx
                  << ") has an invalid 'mmu-type': "
                  << mmu_type << ").\n";
        exit(1);
      }
    } else {
      procs[cpu_idx]->set_mmu_capability(IMPL_MMU_SBARE);
    }

    cpu_idx++;
  }

  if (cpu_idx != nprocs()) {
      std::cerr << "core number in dts ("
                <<  cpu_idx
                << ") doesn't match it in command line ("
                << nprocs() << ").\n";
      exit(1);
  }
}

sim_lib_t::~sim_lib_t() {
}

int sim_lib_t::run() {
  while (target_running()) {
    uint64_t tohost_req = check_tohost_req();
    if (tohost_req) {
      handle_tohost_req(tohost_req);
    } else {
      if (debug || ctrlc_pressed)
        interactive();
      else
        step_target(INTERLEAVE, INTERLEAVE / INSNS_PER_RTC_TICK);
    }
    send_fromhost_req();
  }
  return stop_sim();
}

void sim_lib_t::init() {
  if (!debug && log)
    set_procs_debug(true);

  htif_t::set_expected_xlen(isa.get_max_xlen());

  // load the binary
  start();
}

void sim_lib_t::run_for(uint64_t steps) {
  uint64_t tot_step = 0;
  bool stalled = false;

  while (target_running() && tot_step < steps && !stalled) {
    uint64_t tohost_req = check_tohost_req();
    if (tohost_req) {
      handle_tohost_req(tohost_req);
    } else {
      uint64_t cur_step = std::min(steps - tot_step, INTERLEAVE);
      uint64_t dev_step = std::max((uint64_t)1, cur_step / INSNS_PER_RTC_TICK);
      step_target(cur_step, dev_step);
      for (int i = 0, nprocs = (int)procs.size(); i < nprocs; i++) {
        auto plib = dynamic_cast<processor_lib_t*>(procs[i]);
        auto pst = plib->step_trace();
        target_trace.insert(target_trace.end(), pst.begin(), pst.end());
        tot_step += pst.size();
        if (pst.size() == 0) {
          stalled = true;
          break;
        }
      }
    }
    send_fromhost_req();
  }

  if (!target_running()) {
    fprintf(stderr, "target finished before %" PRIu64 " steps\n", steps);
  }
}

void sim_lib_t::step_proc(size_t n, unsigned int idx) {
  procs[idx]->step(n);
}

void sim_lib_t::yield_load_rsrv(unsigned int idx) {
  procs[idx]->get_mmu()->yield_load_reservation();
}

void sim_lib_t::step_devs(size_t n) {
  for (auto &dev : devices)
    dev->tick(n);
}

void sim_lib_t::step_target(size_t proc_step, size_t dev_step) {
  unsigned int nprocs = (unsigned int)procs.size();

  // TODO : yield_load_rsrv in multicore. currently, spike may loop infinitely
  // when the single processor yields every time it executes a single step
  assert(nprocs == 1);
  for (unsigned int pidx = 0; pidx < nprocs; pidx++) {
    step_proc(proc_step, pidx);
/* yield_load_rsrv(pidx); */
  }
  step_devs(dev_step);
}


bool sim_lib_t::target_running() {
  return (!signal_exit && exitcode) == 0;
}

int sim_lib_t::stop_sim() {
  stop();
  return exit_code();
}

// htif stuff
uint64_t sim_lib_t::check_tohost_req() {
  uint64_t tohost;
  try {
    if ((tohost = from_target(mem.read_uint64(tohost_addr))) != 0)
      mem.write_uint64(tohost_addr, target_endian<uint64_t>::zero);
  } catch (mem_trap_t& t) {
    bad_address("accessing tohost", t.get_tval());
  }
  return tohost;
}

void sim_lib_t::handle_tohost_req(uint64_t req) {
  try {
    command_t cmd(mem, req, fromhost_callback);
    device_list.handle_command(cmd);
    device_list.tick();
  } catch (mem_trap_t& t) {
    std::stringstream tohost_hex;
    tohost_hex << std::hex << req;
    bad_address("host was accessing memory on behalf of target (tohost = 0x" + tohost_hex.str() + ")", t.get_tval());
  }
}

void sim_lib_t::send_fromhost_req() {
  try {
    if (!fromhost_queue.empty() && !mem.read_uint64(fromhost_addr)) {
      mem.write_uint64(fromhost_addr, to_target(fromhost_queue.front()));
      fromhost_queue.pop();
    }
  } catch (mem_trap_t& t) {
    bad_address("accessing fromhost", t.get_tval());
  }
}

void sim_lib_t::serialize_proto(std::string& msg) {
#ifdef DEBUG_PROTOBUF
  printf("serializing\n");
  this->print_state();
#endif

  serialize_called = true;

  SimState* sim_proto = google::protobuf::Arena::Create<SimState>(arena);
  for (int i = 0, cnt = (int)procs.size(); i < cnt; i++) {
    ArchState* arch_proto = sim_proto->add_msg_arch_state();
    procs[i]->serialize_proto((void*)arch_proto, (void*)arena);
  }

  // CLINT
  CLINT* clint_proto = google::protobuf::Arena::Create<CLINT>(arena);
  clint_proto->set_msg_mtime(clint->get_mtime());
  for (uint64_t i = 0, cnt = (uint64_t)procs.size(); i < cnt; i++) {
    UInt64Map* kv = clint_proto->add_msg_mtimecmp();
    kv->set_msg_k(i);
    kv->set_msg_v(clint->get_mtimecmp(i));
  }
  sim_proto->set_allocated_msg_clint(clint_proto);

  // PLIC
  PLIC* plic_proto = google::protobuf::Arena::Create<PLIC>(arena);
  auto& plic_contexts = plic->get_contexts();
  for (auto& pc : plic_contexts) {
    PLICContext* ctx_proto = plic_proto->add_msg_contexts();
    ctx_proto->set_msg_priority_threshold(pc.priority_threshold);

    for (int i = 0; i < PLIC_MAX_DEVICES/32; i++) {
      ctx_proto->add_msg_enable (pc.enable[i]);
      ctx_proto->add_msg_pending(pc.pending[i]);
      ctx_proto->add_msg_claimed(pc.claimed[i]);
    }

    for (int i = 0; i < PLIC_MAX_DEVICES; i++) {
      ctx_proto->add_msg_pending_priority(pc.pending_priority[i]);
    }
  }

  for (int i = 0; i < PLIC_MAX_DEVICES; i++) {
    plic_proto->add_msg_priority(plic->get_priority(i));
  }

  for (int i = 0; i < PLIC_MAX_DEVICES/32; i++) {
    plic_proto->add_msg_level(plic->get_level(i));
  }
  sim_proto->set_allocated_msg_plic(plic_proto);

  // only one dram device for now
  assert((int)mems.size() == 1);

  for (int i = 0, nprocs = procs.size(); i < nprocs; i++) {
    procs[i]->get_mmu()->flush_tlb();
  }
  debug_mmu->flush_tlb();

  if (!serialize_mem) {
#ifndef DEBUG_MEM
    ckpt_ppn.clear();
    for (auto& addr_mem : mems) {
      auto mem = (mem_t*)addr_mem.second;
      std::map<reg_t, char*>& spm = mem->get_sparse_memory_map();
      for (auto& page : spm) {
        ckpt_ppn.insert(page.first);
      }
    }
    for (auto &page : mm_ckpt) {
      ckpt_mempool.push_back(page.second);
    }
    mm_ckpt.clear();
#else
    for (auto& page : all_mm_ckpt) {
      free(page.second);
    }
    all_mm_ckpt.clear();

    for (auto& addr_mem: mems) {
      auto mem = (mem_t*)addr_mem.second;
      std::map<reg_t, char*>& spm = mem->get_sparse_memory_map();
      for (auto& page : spm) {
        char* buf = (char*)malloc(PGSIZE);
        memcpy(buf, page.second, PGSIZE);
        all_mm_ckpt[page.first] = buf;
      }
    }
#endif
  } else {
    for (auto& addr_mem : mems) {
      auto mem = (mem_t*)addr_mem.second;
      std::map<reg_t, char*>& spm = mem->get_sparse_memory_map();
      for (auto& page: spm) {
        Page* page_proto = sim_proto->add_msg_sparse_mm();
        page_proto->set_msg_ppn(page.first);
        page_proto->set_msg_bytes((const void*)page.second, PGSIZE);
      }
    }
  }

  for (auto& dev : devices) {
    dev->serialize_proto(nullptr, nullptr);
  }

  sim_proto->SerializeToString(&msg);
  google::protobuf::ShutdownProtobufLibrary();
}

void sim_lib_t::deserialize_proto(std::string& msg) {
  serialize_called = false;
  google::protobuf::Arena* arena = new google::protobuf::Arena();
  SimState* sim_proto = google::protobuf::Arena::Create<SimState>(arena);
  sim_proto->ParseFromString(msg);

  for (int i = 0, cnt = sim_proto->msg_arch_state_size(); i < cnt; i++) {
    auto arch_proto = sim_proto->msg_arch_state(i);
    procs[i]->deserialize_proto(&arch_proto);
  }

  // CLINT
  const CLINT& clint_proto = sim_proto->msg_clint();
  clint->set_mtime(clint_proto.msg_mtime());
  assert(clint->mtimecmp.size() == clint_proto.msg_mtimecmp_size());
  clint->clear_mtimecmp();

  for (int i = 0, cnt = clint_proto.msg_mtimecmp_size(); i < cnt; i++) {
    const UInt64Map& kv = clint_proto.msg_mtimecmp(i);
    clint->set_mtimecmp(kv.msg_k(), kv.msg_v());
  }

  // PLIC
  const PLIC& plic_proto = sim_proto->msg_plic();
  assert(plic->get_contexts().size() == plic_proto.msg_contexts_size());
  auto& plic_contexts = plic->get_contexts();
  for (int i = 0, cnt = plic_proto.msg_contexts_size(); i < cnt; i++) {
    const PLICContext& pc = plic_proto.msg_contexts(i);
    plic_contexts[i].priority_threshold = pc.msg_priority_threshold();

    for (int j = 0; j < pc.msg_enable_size(); j++)
      plic_contexts[i].enable[j] = pc.msg_enable(j);

    for (int j = 0; j < pc.msg_pending_size(); j++)
      plic_contexts[i].pending[j] = pc.msg_pending(j);

    for (int j = 0; j < pc.msg_pending_priority_size(); j++)
      plic_contexts[i].pending_priority[j] = pc.msg_pending_priority(j);

    for (int j = 0; j < pc.msg_claimed_size(); j++)
      plic_contexts[i].claimed[j] = pc.msg_claimed(j);
  }

  assert(plic_proto.msg_priority_size() == PLIC_MAX_DEVICES);
  for (int i = 0, cnt = plic_proto.msg_priority_size(); i < cnt; i++) {
    plic->set_priority(i, plic_proto.msg_priority(i));
  }

  assert(plic_proto.msg_level_size() == PLIC_MAX_DEVICES / 32);
  for (int i = 0, cnt = plic_proto.msg_level_size(); i < cnt; i++) {
    plic->set_level(i, plic_proto.msg_level(i));
  }

  for (int i = 0, nprocs = procs.size(); i < nprocs; i++) {
    procs[i]->get_mmu()->flush_tlb();
  }
  debug_mmu->flush_tlb();

  for (auto& addr_mem : mems) {
    auto mem = (mem_t*)addr_mem.second;
    std::map<reg_t, char*>& spm = mem->get_sparse_memory_map();

    if (!serialize_mem) {
#ifndef DEBUG_MEM
      std::vector<reg_t> tofree;
      for (auto& page : spm) {
        auto ppn   = page.first;
        auto haddr = page.second;
        if (ckpt_ppn.find(ppn) == ckpt_ppn.end()) {
          tofree.push_back(ppn);
        } else {
          auto it = mm_ckpt.find(haddr);
          if (it != mm_ckpt.end()) {
            memcpy(haddr, it->second, PGSIZE);
          }
        }
      }
      for (auto x: tofree) {
        free(spm[x]);
        spm.erase(x);
      }
#else
      for (auto& addr_mem : mems) {
        auto mem = (mem_t*)addr_mem.second;
        std::map<reg_t, char*>& spm = mem->get_sparse_memory_map();
        std::vector<reg_t> tofree;
        for (auto& page : spm) {
          if (all_mm_ckpt.find(page.first) == all_mm_ckpt.end()) {
            tofree.push_back(page.first);
          } else {
            memcpy(page.second, all_mm_ckpt[page.first], PGSIZE);
          }
        }
        for (auto& x: tofree) {
          free(spm[x]);
          spm.erase(x);
        }
      }
#endif
    } else {
      for (auto& page: spm) {
        free(page.second);
      }
      spm.clear();
      for (int i = 0, cnt = sim_proto->msg_sparse_mm_size(); i < cnt; i++) {
        const Page& page_proto = sim_proto->msg_sparse_mm(i);
        reg_t ppn = page_proto.msg_ppn();
        const std::string& bytes = page_proto.msg_bytes();
        assert(bytes.size() == PGSIZE);

        char* res = (char*)malloc(PGSIZE);
        if (res == nullptr)
          throw std::bad_alloc();
        memcpy((void*)res, bytes.data(), bytes.size());
        spm[ppn] = res;
      }
    }
  }

  for (auto& dev : devices) {
    dev->deserialize_proto(nullptr);
  }
  google::protobuf::ShutdownProtobufLibrary();

#ifdef DEBUG_PROTOBUF
  printf("deserialize done\n");
  this->print_state();
#endif

}

bool sim_lib_t::ganged_step(rtl_step_t step, reg_t npc, int hartid) {
  bool val       = step.val;
  uint64_t time  = step.time;
  uint64_t pc    = step.pc;
  uint64_t insn  = step.insn;
  bool except    = step.except;
  bool intrpt    = step.intrpt;
  int cause      = step.cause;
  bool has_w     = step.has_w;
  uint64_t wdata = step.wdata;
  int priv       = step.priv;

  processor_lib_t* proc = get_core(hartid);
  state_t* s = proc->get_state();
  uint64_t s_pc = s->pc;

  uint64_t interrupt_cause = cause & 0x7FFFFFFFFFFFFFFF;
  bool ssip_interrupt = interrupt_cause == 0x1;
  bool msip_interrupt = interrupt_cause == 0x3;
  bool stip_interrupt = interrupt_cause == 0x5;
  bool mtip_interrupt = interrupt_cause == 0x7;
  bool seip_interrupt = interrupt_cause == 0x9;
  bool debug_interrupt = interrupt_cause == 0xe;

  // If the interrupt is set in RTL sim...
  if (intrpt) {
    if (ssip_interrupt || stip_interrupt) {
      // do nothing
    } else if (msip_interrupt) {
      s->mip->backdoor_write_with_mask(MIP_MSIP, MIP_MSIP);
    } else if (mtip_interrupt) {
      s->mip->backdoor_write_with_mask(MIP_MTIP, MIP_MTIP);
    } else if (debug_interrupt) {
      // don't execute instructions
    } else if (seip_interrupt) {
      bool has_pending_interrupt = this->plic->alert_core_external_interrupt(hartid);
      if (!has_pending_interrupt) {
        for (int i = 1, cnt = devices.size(); i < cnt; i++) {
          auto& d = devices[i];
          d->tick(1);
        }
        bool retry_plic_interrupt = this->plic->alert_core_external_interrupt(hartid);
        if (!retry_plic_interrupt) {
          printf("Spike does not have any pending interrupts\n");
          printf("TRACE time: %" PRIu64 " v: %d pc: 0x%" PRIx64 " insn: 0x%" PRIx64 " e: %d i: %d c: %d hw: %d wd: %" PRIu64 " prv: %d\n",
              time, val, pc, insn, except, intrpt, cause, has_w, wdata, priv);
          return false;
        }
      }
    } else {
      printf("Unknown interrupt\n");
      printf("TRACE time: %" PRIu64 " v: %d pc: 0x%" PRIx64 " insn: 0x%" PRIx64 " e: %d i: %d c: %d hw: %d wd: %" PRIu64 " prv: %d\n",
          time, val, pc, insn, except, intrpt, cause, has_w, wdata, priv);
      return false;
    }
  }

  // WFI instructions are noops in RTL.
  // To avoid executing wfi instruction multiple times in functional sim,
  // we want to clear the wfi signal.
  if (val || except || intrpt) {
    if (processor_step_cnt++ % DEVICE_TICK_PERIOD == 0) {
      // ignore clint
      for (int i = 1, cnt = devices.size(); i < cnt; i++) {
        auto& d = devices[i];
        d->tick(1);
      }
    }

    proc->clear_waiting_for_interrupt();
    proc->step(1);

/* uint64_t opcode = insn & 0x7f; */
/* uint64_t ilen  = insn_length(insn); */
/* int rd = (insn >> 7) & 0x1f; */
/* if (!except && !intrpt && */
/* ilen == 4 && */
/* (opcode == 0b0110011 || /1* R-type *1/ */
/* opcode == 0b0010011 || /1* I-type (compute) *1/ */
/* opcode == 0b0110111 || /1* lui *1/ */
/* opcode == 0b0010111 || /1* auipc *1/ */
/* opcode == 0b1100111)   /1* jalr *1/ */
/* ) { */
/* #ifdef DEBUG_SHORTCUT */
/* if (opcode == 0b0110011) printf("R-type "); */
/* if (opcode == 0b0010011) printf("I-type "); */
/* if (opcode == 0b0110111) printf("lui    "); */
/* if (opcode == 0b0010111) printf("auipc  "); */
/* if (opcode == 0b1100011) printf("B-type "); */
/* if (opcode == 0b1100111) printf("jalr   "); */
/* printf("time: %" PRIu64 " shortcut pc: 0x%" PRIx64 " npc: 0x%" PRIx64 " has_w: %d wdata: 0x%" PRIx64 " rd: %d\n", */
/* step.time, pc, npc, step.has_w, step.wdata, rd); */
/* #endif */
/* s->XPR.write(rd, wdata); */
/* s->pc = npc; */
/* if (proc->get_log_commits_enabled()) { */
/* proc->reset_commit_log(); */
/* } */
/* } else { */
/* proc->step(1); */
/* } */
  }

  if (val && !except) {
    if (s_pc != pc) {
      printf("!!!!!!!!!!!! %lld PC mismatch spike %lx != DUT %llx\n", time, s_pc, pc);
      printf("spike mstatus is %lx\n", s->mstatus->read());
      printf("spike mcause is %lx\n", s->mcause->read());
      printf("spike mtval is %lx\n" , s->mtval->read());
      printf("spike mtinst is %lx\n", s->mtinst->read());
      printf("spike mepc is %lx\n", s->mepc->read());
      printf("spike mtvec is %lx\n", s->mtvec->read());
      for (int i = 0; i < NXPR; i++) {
        printf("r %2d = 0x%" PRIx64 "\n", i, s->XPR[i]);
      }
      return false;
    }

    auto& mem_write = s->log_mem_write;
    auto& mem_read = s->log_mem_read;
    auto& log = s->log_reg_write;

    for (auto memwrite : mem_write) {
      reg_t waddr = std::get<0>(memwrite);
      uint64_t w_data = std::get<1>(memwrite);

      // If the store address matches the CLINT, lower the interrupt signal
      if ((waddr == CLINT_BASE + 4*hartid) && w_data == 0) {
        s->mip->backdoor_write_with_mask(MIP_MSIP, 0);
      }
      if ((waddr == CLINT_BASE + 0x4000 + 4*hartid)) {
        s->mip->backdoor_write_with_mask(MIP_MTIP, 0);
      }
    }

    bool scalar_wb = false;
    bool vector_wb = false;
    uint32_t vector_cnt = 0;
    std::vector<reg_t> vector_rds;
    for (auto &regwrite : log) {

      //TODO: scaling to multi issue reads?
      reg_t mem_read_addr = mem_read.empty() ? 0 : std::get<0>(mem_read[0]);
      reg_t mem_read_size = mem_read.empty() ? 0 : std::get<2>(mem_read[0]);

      int rd = regwrite.first >> 4;
      int type = regwrite.first & 0xf;

      // 0 => int
      // 1 => fp
      // 2 => vec
      // 3 => vec hint
      // 4 => csr
      bool device_read = false;
      for (auto& d : ganged_devs) {
        if (d.get()->was_read) {
          device_read = true;
          d.get()->was_read = false;
        }
      }

      bool lr_read = ((insn & MASK_LR_D) == MATCH_LR_D) || ((insn & MASK_LR_W) == MATCH_LR_W);
      bool sc_read = ((insn & MASK_SC_D) == MATCH_SC_D) || ((insn & MASK_SC_W) == MATCH_SC_W);

      bool ignore_read = device_read || sc_read || (!mem_read.empty() &&
          (lr_read ||
           (tohost_addr && mem_read_addr == tohost_addr) ||
           (fromhost_addr && mem_read_addr == fromhost_addr)));

      // check the type is compliant with writeback first
      if ((type == 0 || type == 1))
        scalar_wb = true;
      if (type == 2) {
        vector_rds.push_back(rd);
        vector_wb = true;
      }
      if (type == 3) continue;

      if ((rd != 0 && type == 0) || type == 1) {
        // Override reads from some CSRs
        uint64_t csr_addr = (insn >> 20) & 0xfff;
        bool csr_read = (insn & 0x7f) == 0x73;
        if (csr_read && (
              (csr_addr == CSR_MISA)       ||
              (csr_addr == CSR_MCOUNTEREN) ||
              (csr_addr == CSR_MCAUSE)     ||
              (csr_addr == CSR_MTVAL)     ||
              (csr_addr == CSR_MIMPID)     ||
              (csr_addr == CSR_MARCHID)    ||
              (csr_addr == CSR_MVENDORID)  ||
              (csr_addr == CSR_MCYCLE)     ||
              (csr_addr == CSR_MINSTRET)   ||
              (csr_addr == CSR_CYCLE)      ||
              (csr_addr == CSR_TIME)       ||
              (csr_addr == CSR_INSTRET)    ||
              (csr_addr == CSR_SATP)    ||
              (csr_addr >= CSR_TSELECT && csr_addr <= CSR_MCONTEXT) ||
              (csr_addr >= CSR_PMPADDR0 && csr_addr <= CSR_PMPADDR63)
              )) {
/* printf("csr override: rd[%d] wdata: 0x%" PRIx64 "\n", rd, wdata); */
          s->XPR.write(rd, wdata);
        } else if (ignore_read) {
/* printf("ign override: rd[%d] 0x%" PRIx64 " -> wdata: 0x%" PRIx64 "\n", rd, s->XPR[rd], wdata); */
/* printf("dr: %d sc: %d lr: %d mr: %d th: %d fh: %d\n", */
/* device_read, sc_read, lr_read, !mem_read.empty(), */
/* (tohost_addr && mem_read_addr == tohost_addr), */
/* (fromhost_addr && mem_read_addr == fromhost_addr)); */

          s->XPR.write(rd, wdata);
        } else if ((type == 0) && (wdata != regwrite.second.v[0])) {
          s->XPR.write(rd, wdata);
        } else if ((type == 1) && (wdata != regwrite.second.v[0])) {
          printf("%" PRIu64 " FP wdata mismatch: spike %" PRIu64 " fsim %" PRIu64 "\n",
              time, regwrite.second.v[0], wdata);
          return false;
        }
      }
    }
  }

  return true;
}

rtl_step_t sim_lib_t::parse_line_into_rtltrace(std::string line) {
  std::vector<std::string> words = fast_split(line, ' ', 10);

  bool     val    = strtobool_fast(words[1].c_str());
  uint64_t time   = strtoull_fast_dec(words[0].c_str());
  uint64_t pc     = strtoull_fast_hex(words[2].substr(2).c_str());
  uint64_t insn   = strtoull_fast_hex(words[3].substr(2).c_str());
  bool     except = strtobool_fast(words[4].c_str());
  bool     intrpt = strtobool_fast(words[5].c_str());
  int      cause  = strtoull_fast_dec(words[6].c_str());
  bool     has_w  = strtobool_fast(words[7].c_str());
  uint64_t wdata  = strtoull_fast_hex(words[8].substr(2).c_str());
  int      priv   = strtoull_fast_dec(words[9].c_str());

  return rtl_step_t(val, time, pc, insn,
                     except, intrpt, cause, has_w, wdata, priv);
}

int sim_lib_t::run_from_trace() {
  assert(rtl_tracefile_name);
  std::string line;
  std::string tracefile_string = std::string(rtl_tracefile_name);
  std::ifstream rtl_trace = std::ifstream(tracefile_string, std::ios::binary);

  // TODO : multicore support
  int hartid = 0;
  this->configure_log(true, true);
  this->get_core(hartid)->get_state()->pc = ROCKETCHIP_RESET_VECTOR;

  while (std::getline(rtl_trace, line)) {
    uint64_t tohost_req = check_tohost_req();
    if (tohost_req)
      handle_tohost_req(tohost_req);

    rtl_step_t step = parse_line_into_rtltrace(line);
    bool success = ganged_step(step, 0, hartid);
    if (!success) {
      printf("ganged simulation failed\n");
      assert(false);
    }
  }
  return 0;
}

void sim_lib_t::print_state() {
  for (int i = 0, cnt = (int)procs.size(); i < cnt; i++) {
    printf("proc[%d] state\n", i);
    this->get_core(i)->print_state();
  }

  printf("clint mtime 0x%" PRIx64 "\n", clint->get_mtime());
  for (uint64_t i = 0, cnt = (uint64_t)procs.size(); i < cnt; i++) {
    printf("mtimecmp[%" PRIu64 "]: 0x%" PRIx64 "\n", i, clint->get_mtimecmp(i));
  }

  printf("plic state\n");
  auto plic_contexts = plic->get_contexts();
  for (auto& pc : plic_contexts) {
    printf("%" PRIu8 "\n", pc.priority_threshold);
    for (int i = 0; i < PLIC_MAX_DEVICES/32; i++) {
      printf("e: %" PRIu32 " p: %" PRIu32 " c: %" PRIu32 "\n",
          pc.enable[i], pc.pending[i], pc.claimed[i]);
    }
    for (int i = 0; i < PLIC_MAX_DEVICES; i++) {
      printf("%" PRIu8 "\n", pc.pending_priority[i]);
    }
  }
  for (int i = 0; i < PLIC_MAX_DEVICES; i++) {
    printf("%" PRIu8 "\n", plic->get_priority(i));
  }
  for (int i = 0; i < PLIC_MAX_DEVICES/32; i++) {
    printf("%" PRIu8 "\n", plic->get_level(i));
  }
}
