#include "tiger/regalloc/regalloc.h"

#include "tiger/output/logger.h"

extern frame::RegManager *reg_manager;

namespace ra {
/* TODO: Put your lab6 code here */
RegAllocator::RegAllocator(frame::Frame *frame,
                           std::unique_ptr<cg::AssemInstr> assem_instr)
    : frame_(frame), assem_instr_(std::move(assem_instr)) {}

void RegAllocator::RegAlloc() {
  // 1. 把递归改成了循环
  // 2. 把每一次重复的主要流程放在了doColoring
  col::Result color_result;
  bool done = false;
  do {
    liveness = GetLiveGraph();
    col::Color *color = new col::Color(liveness);
    color_result = color->doColoring();
    if (color_result.spills == nullptr) {
      done = true;
    } else {
      RewriteProgram();
      done = false;
    }
  } while (!done);
  this->result_ = std::make_unique<ra::Result>(
      color_result.coloring, this->assem_instr_->GetInstrList());
}

std::unique_ptr<ra::Result> RegAllocator::TransferResult() {
  return std::move(result_);
}

live::LiveGraph RegAllocator::GetLiveGraph() {
  // fprintf(stderr, "GetLiveGraph\n");
  fg::FlowGraphFactory *fgFactory =
      new fg::FlowGraphFactory(assem_instr_.get()->GetInstrList());
  fgFactory->AssemFlowGraph();
  fg::FGraphPtr cfg = fgFactory->GetFlowGraph();
  live::LiveGraphFactory *liveFactory = new live::LiveGraphFactory(cfg);
  liveFactory->Liveness();
  return liveFactory->GetLiveGraph();
}

void RegAllocator::RewriteProgram() {}

} // namespace ra