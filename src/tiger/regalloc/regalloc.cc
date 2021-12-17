#include "tiger/regalloc/regalloc.h"

#include "tiger/output/logger.h"

extern frame::RegManager *reg_manager;

namespace ra {
/* TODO: Put your lab6 code here */
RegAllocator::RegAllocator(frame::Frame *frame,
                           std::unique_ptr<cg::AssemInstr> assem_instr)
    : frame_(frame), instr_list_(assem_instr.get()->GetInstrList()) {}

void RegAllocator::RegAlloc() {
  // 1. 把递归改成了循环
  // 2. 把每一次重复的主要流程放在了doColoring
  col::Result color_result;
  temp::TempList *noSpillTemps = nullptr;
  bool done = false;
  do {
    liveness = GetLiveGraph();
    col::Color *color = new col::Color(liveness, noSpillTemps);
    color_result = color->doColoring();
    if (color_result.spills == nullptr ||
        color_result.spills->GetList().empty()) {
      done = true;
    } else {
      noSpillTemps = RewriteProgram(color_result.spills);
      done = false;
    }
    // ShowAssem(color_result.coloring);
  } while (!done);

  assem::InstrList *new_instrlist =
      removeMoveInstr(instr_list_, color_result.coloring);
  this->result_ =
      std::make_unique<ra::Result>(color_result.coloring, new_instrlist);
}

std::unique_ptr<ra::Result> RegAllocator::TransferResult() {
  return std::move(result_);
}

live::LiveGraph RegAllocator::GetLiveGraph() {
  // fprintf(stderr, "GetLiveGraph\n");
  fg::FlowGraphFactory *fgFactory = new fg::FlowGraphFactory(instr_list_);
  fgFactory->AssemFlowGraph();
  fg::FGraphPtr cfg = fgFactory->GetFlowGraph();
  live::LiveGraphFactory *liveFactory = new live::LiveGraphFactory(cfg);
  liveFactory->Liveness();
  return liveFactory->GetLiveGraph();
}

temp::TempList *RegAllocator::RewriteProgram(live::INodeListPtr spilledNodes) {
  // fprintf(stderr, "spilled nodes:\n");
  // for (live::INodePtr node : spilledNodes->GetList()) {
  //   fprintf(stderr, "%d, ", node->NodeInfo()->Int());
  // }
  // fprintf(stderr, "\n");
  temp::TempList *noSpillTemps = new temp::TempList();
  assem::InstrList *new_instrlist = nullptr;
  for (live::INodePtr node : spilledNodes->GetList()) {
    temp::Temp *spilledTemp = node->NodeInfo();
    frame_->offset -= reg_manager->WordSize();
    new_instrlist = new assem::InstrList();
    for (assem::Instr *instr : instr_list_->GetList()) {
      temp::TempList *templist_src = instr->Use();
      temp::TempList *templist_dst = instr->Def();
      temp::Temp *newtemp = nullptr;

      if (live::LiveGraphFactory::Contain(templist_src, spilledTemp)) {
        newtemp = temp::TempFactory::NewTemp();
        // selectSpill要避免选择这种由读取前面已溢出的寄存器产生的结点
        noSpillTemps->Append(newtemp);
        templist_src->replaceTemp(spilledTemp, newtemp);
        // movq (xx_framesize-size)(`s0), `d0
        std::string str = "movq (" + frame_->GetLabel() + "_framesize-" +
                          std::to_string(-frame_->offset) + ")(`s0), `d0";
        assem::Instr *new_instr = new assem::OperInstr(
            str, new temp::TempList({newtemp}),
            new temp::TempList({reg_manager->StackPointer()}), nullptr);
        new_instrlist->Append(new_instr);
      }

      new_instrlist->Append(instr);

      if (live::LiveGraphFactory::Contain(templist_dst, spilledTemp)) {
        newtemp = temp::TempFactory::NewTemp();
        noSpillTemps->Append(newtemp);
        templist_dst->replaceTemp(spilledTemp, newtemp);
        std::string str = "movq `s0, (" + frame_->GetLabel() + "_framesize-" +
                          std::to_string(-frame_->offset) + ")(`d0)";
        assem::Instr *new_instr = new assem::OperInstr(
            str, new temp::TempList({reg_manager->StackPointer()}),
            new temp::TempList({newtemp}), nullptr);
        new_instrlist->Append(new_instr);
      }
    }
    instr_list_ = new_instrlist;
  }
  return noSpillTemps;
}

assem::InstrList *RegAllocator::removeMoveInstr(assem::InstrList *il,
                                                temp::Map *color) {
  assem::InstrList *ret = new assem::InstrList();
  for (assem::Instr *instr : il->GetList()) {
    if (typeid(*instr) == typeid(assem::MoveInstr)) {
      temp::Temp *src = instr->Use()->NthTemp(0);
      temp::Temp *dst = instr->Def()->NthTemp(0);
      if (*(color->Look(src)) == *(color->Look(dst))) {
        continue;
      }
    }
    ret->Append(instr);
  }
  return ret;
}

void RegAllocator::ShowAssem(temp::Map *color) {
  fprintf(stderr, "---------------------show assem------------------\n");
  cg::AssemInstr *assem_instr = new cg::AssemInstr(instr_list_);
  Logger(stderr).Log(
      assem_instr,
      temp::Map::LayerMap(reg_manager->temp_map_,
                          temp::Map::LayerMap(color, temp::Map::Name())));
}

} // namespace ra