#include <cstdint>
#include <riscv/decode.h>
#include "disam.h"


namespace profiler {

uint64_t disassembler_t::opcode(insn_t insn) {
  return bits(insn, 0, 7);
}

uint64_t disassembler_t::funct3(insn_t insn) {
  return bits(insn, 12, 3);
}

bool disassembler_t::is_csrw(insn_t insn) {
  return (insn.length() == 4) &&
         opcode(insn) == OP_CSR &&
         funct3(insn) == 0x001;
}

bool disassembler_t::is_type(std::string type, insn_t insn) {
  if (type == CSRW) {
    return is_csrw(insn);
  } else {
    return false;
  }
}

uint64_t disassembler_t::bits(insn_t insn, int lo, int len) {
  uint64_t bits = insn.bits();
  return (bits >> lo) & ((uint64_t(1) << len) - 1);
}

} // namespace profiler
