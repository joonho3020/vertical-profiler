#ifndef __DISASM_H__
#define __DISASM_H__

#include <string>
#include <riscv/decode.h>

namespace profiler {

#define OP_CSR 0b1110011

#define CSRW   "csrw"



class disassembler_t {
public:
  disassembler_t() {}

public:
  uint64_t opcode(insn_t insn);
  uint64_t funct3(insn_t insn);
  bool is_csrw(insn_t insn);
  bool is_type(std::string type, insn_t insn);

private:
  uint64_t bits(insn_t insn, int lo, int len);
};

} // namespace profiler

#endif //__DISASM_H__
