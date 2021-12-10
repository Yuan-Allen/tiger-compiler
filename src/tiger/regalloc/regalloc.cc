#include "tiger/regalloc/regalloc.h"

#include "tiger/output/logger.h"

extern frame::RegManager *reg_manager;

namespace ra {
/* TODO: Put your lab6 code here */
RegAllocator::RegAllocator(frame::Frame *frame,
                           std::unique_ptr<cg::AssemInstr> assem_instr)
    : frame_(frame), assem_instr_(std::move(assem_instr)) {}

void RegAllocator::RegAlloc() { liveness = GetLiveGraph(); }

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
} // namespace ra