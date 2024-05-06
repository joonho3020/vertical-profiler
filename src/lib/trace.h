#ifndef __TRACE_H__
#define __TRACE_H__

#include <inttypes.h>
#include <vector>
#include <stdio.h>

struct trace_entry_t {
  uint64_t pc;
  uint64_t asid;
  uint64_t cycle;
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
    printf("- t: %" PRIu64 " pc: %" PRIx64 " v,e,i,h,c,w %d %d %d %d %d %" PRIx64 "\n",
        time, pc, val, except, intrpt, has_w, cause, wdata);
  }
};

typedef std::vector<trace_entry_t> trace_t;

#endif //__TRACE_H__
