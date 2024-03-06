#include <string>
#include <map>
#include <vector>
#include <assert.h>

#include "stack_unwinder.h"
#include "../tracerv/trace_tracker.h"
#include "../tracerv/tracerv_processing.h"

namespace profiler {

void process_traces(named_trace_t trace, stack_unwinder_t* su) {
  for (auto& x : trace) {
    su->process_instruction(x.pc, x.cycle, x.bin);
  }
}

stack_unwinder_t::stack_unwinder_t(
    std::vector<std::pair<std::string, std::string>> objdump_paths,
    FILE *stackfile)
{
  for (auto x : objdump_paths) {
    printf("Parsing ObjdumpedBinary %s %s\n", x.first.c_str(), x.second.c_str());
    bin_dumps_[x.first] = new ObjdumpedBinary(x.second);
  }
  this->stackfile_ = stackfile;
  this->workers_.start(1);
}

void stack_unwinder_t::stop() {
  flush_insns_to_threadpool();
  workers_.stop();
}

void stack_unwinder_t::submit_insn(reg_t pc, reg_t cycle, std::string bin) {
  named_traces_.push_back({pc, cycle, bin});
}

void stack_unwinder_t::flush_insns_to_threadpool() {
  stack_unwinder_t* su = this;
  workers_.queue_job(process_traces, named_traces_, su);
  named_traces_.clear();
}

void stack_unwinder_t::submit_insns_to_threadpool() {
  if ((uint32_t)named_traces_.size() >= INSN_TRACE_FLUSH_THRESHOLD ||
      !workers_.busy()) {
    flush_insns_to_threadpool();
  }
}

void stack_unwinder_t::process_instruction(uint64_t inst_addr,
                                           uint64_t cycle,
                                           std::string binary) {
  auto bit = bin_dumps_.find(binary);
  Instr *this_instr = nullptr;
  std::string label_pfx = "";
  if (bit != bin_dumps_.end()) {
    this_instr = bit->second->getInstrFromAddr(inst_addr);
    label_pfx = binary;
  }

  std::string userspace_misc = "USERSPACE_MISC";

  if (!this_instr) {
    if ((label_stack_.size() == 1)  &&
        (userspace_misc == label_stack_[label_stack_.size() - 1]->label)) {
      LabelMeta* last_label = label_stack_[label_stack_.size() - 1];
      last_label->end_cycle = cycle;
    } else {
      while (label_stack_.size() > 0) {
        LabelMeta *pop_label = label_stack_[label_stack_.size() - 1];
        label_stack_.pop_back();
        pop_label->post_print(this->stackfile_);
        delete pop_label;
        if (label_stack_.size() > 0) {
          LabelMeta *last_label = label_stack_[label_stack_.size() - 1];
          last_label->end_cycle = cycle;
        }
      }
      LabelMeta *new_label = new LabelMeta();
      new_label->label_pfx = label_pfx;
      new_label->label = userspace_misc;
      new_label->start_cycle = cycle;
      new_label->end_cycle = cycle;
      new_label->indent = label_stack_.size() + 1;
      new_label->asm_sequence = false;
      label_stack_.push_back(new_label);
      new_label->pre_print(this->stackfile_);
    }
  } else {
    std::string label = this_instr->function_name;

    // if there is a userspace_misc, pop
    if ((label_stack_.size() > 0) &&
        (userspace_misc == label_stack_[label_stack_.size() - 1]->label)) {
      LabelMeta* pop_label = label_stack_[label_stack_.size() - 1];
      label_stack_.pop_back();
      pop_label->post_print(this->stackfile_);
      delete pop_label;
    }

    // same label, increment cycle
    if ((label_stack_.size() > 0) &&
        (label == label_stack_[label_stack_.size() - 1]->label)) {
      LabelMeta* last_label = label_stack_[label_stack_.size() - 1];
      last_label->end_cycle = cycle;
    } else { // different label but not a new function. unwind
      if ((label_stack_.size() > 0) and this_instr->in_asm_sequence and
          label_stack_[label_stack_.size() - 1]->asm_sequence) {

        LabelMeta *pop_label = label_stack_[label_stack_.size() - 1];
        label_stack_.pop_back();
        pop_label->post_print(this->stackfile_);
        delete pop_label;

        LabelMeta *new_label = new LabelMeta();
        new_label->label_pfx = label_pfx;
        new_label->label = label;
        new_label->start_cycle = cycle;
        new_label->end_cycle = cycle;
        new_label->indent = label_stack_.size() + 1;
        new_label->asm_sequence = this_instr->in_asm_sequence;
        label_stack_.push_back(new_label);
        new_label->pre_print(this->stackfile_);
      } else if ((label_stack_.size() > 0) and
                 (this_instr->is_callsite or !(this_instr->is_fn_entry))) {
        uint64_t unwind_start_level = (uint64_t)(-1);
        while (
            (label_stack_.size() > 0) and
            (label_stack_[label_stack_.size() - 1]->label.compare(label) != 0)) {
          LabelMeta *pop_label = label_stack_[label_stack_.size() - 1];
          label_stack_.pop_back();
          pop_label->post_print(this->stackfile_);
          if (unwind_start_level == (uint64_t)(-1)) {
            unwind_start_level = pop_label->indent;
          }
          delete pop_label;
          if (label_stack_.size() > 0) {
            LabelMeta *last_label = label_stack_[label_stack_.size() - 1];
            last_label->end_cycle = cycle;
          }
        }
        if (label_stack_.size() == 0) {
          fprintf(this->stackfile_,
                  "WARN: STACK ZEROED WHEN WE WERE LOOKING FOR LABEL: %s, "
                  "iaddr 0x%" PRIx64 "\n",
                  label.c_str(),
                  inst_addr);
          fprintf(this->stackfile_,
                  "WARN: is_callsite was: %d, is_fn_entry was: %d\n",
                  this_instr->is_callsite,
                  this_instr->is_fn_entry);
          fprintf(this->stackfile_,
                  "WARN: Unwind started at level: dec %" PRIu64 "\n",
                  unwind_start_level);
          fprintf(this->stackfile_, "WARN: Last instr was\n");
          this->last_instr_->printMeFile(this->stackfile_, std::string("WARN: "));
        }
      } else { // new function, add to stack top
        LabelMeta *new_label = new LabelMeta();
        new_label->label_pfx = label_pfx;
        new_label->label = label;
        new_label->start_cycle = cycle;
        new_label->end_cycle = cycle;
        new_label->indent = label_stack_.size() + 1;
        new_label->asm_sequence = this_instr->in_asm_sequence;
        label_stack_.push_back(new_label);
        new_label->pre_print(this->stackfile_);
      }
    }
  }
}

} // namespace Profiler
