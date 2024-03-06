#ifndef __TYPES_H__
#define __TYPES_H__

#include <inttypes.h>
#include <string>
#include <map>
#include <optional>

namespace profiler {

/////////////////////////////////////////////////////
// MACROS
/////////////////////////////////////////////////////
#define   likely(x) __builtin_expect(x, 1)
#define unlikely(x) __builtin_expect(x, 1)


#define pprintf(...)                                          \
  fprintf(stdout, "\033[0;32m[Prof]\033[0m " __VA_ARGS__ );   \
  fflush(stdout);

#define pexit(...)                                            \
  fprintf(stdout, "\033[0;31m[Prof]\033[0m " __VA_ARGS__ );   \
  fflush(stdout);                                             \
  assert(false);

#define passert(...)                                          \
  fprintf(stdout, "\033[0;31m[Prof]\033[0m " __VA_ARGS__ );   \
  fflush(stdout);                                             \
  assert(false);

/////////////////////////////////////////////////////
// typedefs
/////////////////////////////////////////////////////
typedef uint64_t addr_t;
typedef uint64_t reg_t;
typedef std::map<reg_t, std::string> reg2str_t;
typedef std::optional<reg_t> optreg_t;

/////////////////////////////////////////////////////
// consts
/////////////////////////////////////////////////////
#define k_do_execveat_common "do_execveat_common.isra.0"
#define k_do_execveat_common_filename_arg 1

#define k_set_mm_asid "set_mm_asid"

#define k_pick_next_task_fair "pick_next_task_fair"
#define offsetof_task_struct_pid 1072

#define k_kernel_clone "kernel_clone"

#define k_finish_task_switch "finish_task_switch.isra.0"
#define k_finish_task_switch_prev_arg 0

const std::string KERNEL = "k";
const std::string PROF_CSR_SATP = "satp";

#define XPR_CNT 32

static std::string iregs[XPR_CNT] = {
  "x0", "ra", "sp", "gp",  "tp",  "t0", "t1", "t2",
  "s0", "s1", "a0",  "a1",  "a2", "a3", "a4", "a5",
  "a6", "a7", "s2",  "s3",  "s4", "s5", "s6", "s7",
  "s8", "s9", "s10", "s11", "t3", "t4", "t5", "t6"
};

static std::map<std::string, unsigned int> riscv_abi_ireg {
  {"x0",  0}, {"ra",  1}, {"sp" ,  2}, {"gp" ,  3},
  {"tp",  4}, {"t0",  5}, {"t1" ,  6}, {"t2" ,  7},
  {"s0",  8}, {"s1",  9}, {"a0" , 10}, {"a1" , 11},
  {"a2", 12}, {"a3", 13}, {"a4" , 14}, {"a5" , 15},
  {"a6", 16}, {"a7", 17}, {"s2" , 18}, {"s3" , 19},
  {"s4", 20}, {"s5", 21}, {"s6" , 22}, {"s7" , 23},
  {"s8", 24}, {"s9", 25}, {"s10", 26}, {"s11", 27},
  {"t3", 28}, {"t4", 29}, {"t5" , 30}, {"t6" , 31}
};

} // namespace profiler

#endif //__TYPES_H__
