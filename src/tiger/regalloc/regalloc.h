#ifndef TIGER_REGALLOC_REGALLOC_H_
#define TIGER_REGALLOC_REGALLOC_H_

#include "tiger/codegen/assem.h"
#include "tiger/codegen/codegen.h"
#include "tiger/frame/frame.h"
#include "tiger/frame/temp.h"
#include "tiger/liveness/liveness.h"
#include "tiger/regalloc/color.h"
#include "tiger/util/graph.h"

namespace ra {

class Result {
public:
  temp::Map *coloring_;
  assem::InstrList *il_;

  Result() : coloring_(nullptr), il_(nullptr) {}
  Result(temp::Map *coloring, assem::InstrList *il)
      : coloring_(coloring), il_(il) {}
  Result(const Result &result) = delete;
  Result(Result &&result) = delete;
  Result &operator=(const Result &result) = delete;
  Result &operator=(Result &&result) = delete;
  ~Result(){};
};

class RegAllocator {
  /* TODO: Put your lab6 code here */
public:
  RegAllocator(frame::Frame *frame, std::unique_ptr<cg::AssemInstr> traces);
  void RegAlloc();
  std::unique_ptr<ra::Result> TransferResult();

private:
  frame::Frame *frame_;
  assem::InstrList *instr_list_;
  std::unique_ptr<ra::Result> result_;
  live::LiveGraph liveness;

  live::LiveGraph GetLiveGraph();
  temp::TempList *RewriteProgram(live::INodeListPtr spilledNodes);
  assem::InstrList *removeMoveInstr(assem::InstrList *il, temp::Map *color);

  void ShowAssem(temp::Map *color);
};

} // namespace ra

#endif