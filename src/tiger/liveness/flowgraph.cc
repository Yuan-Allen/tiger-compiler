#include "tiger/liveness/flowgraph.h"

namespace fg {

void FlowGraphFactory::AssemFlowGraph() {
  /* TODO: Put your lab6 code here */
  // fprintf(stderr, "AssemFlowGraph\n");
  FNodePtr prev = nullptr;
  FNodePtr cur = nullptr;
  for (assem::Instr *instr : instr_list_->GetList()) {
    cur = flowgraph_->NewNode(instr);
    if (prev != nullptr) {
      flowgraph_->AddEdge(prev, cur);
    }
    if (typeid(*instr) == typeid(assem::LabelInstr)) {
      label_map_->Enter(static_cast<assem::LabelInstr *>(instr)->label_, cur);
    }
    // 如果是jmp, 那么第一次分析（按指令出现顺序）不应该由这条指令走到下条
    if (typeid(*instr) == typeid(assem::OperInstr) &&
        static_cast<assem::OperInstr *>(instr)->assem_.find("jmp") !=
            std::string::npos) {
      prev = nullptr;
    } else {
      prev = cur;
    }
  }
  for (FNodePtr node : flowgraph_->Nodes()->GetList()) {
    assem::Instr *instr = node->NodeInfo();
    if (typeid(*instr) == typeid(assem::OperInstr) &&
        static_cast<assem::OperInstr *>(instr)->jumps_ != nullptr) {
      std::vector<temp::Label *> *labels =
          static_cast<assem::OperInstr *>(instr)->jumps_->labels_;
      for (temp::Label *label : *labels) {
        flowgraph_->AddEdge(node, label_map_->Look(label));
      }
    }
  }
}

} // namespace fg

namespace assem {

temp::TempList *LabelInstr::Def() const {
  /* TODO: Put your lab6 code here */
  return new temp::TempList();
}

temp::TempList *MoveInstr::Def() const {
  /* TODO: Put your lab6 code here */
  if (this->dst_ == nullptr) {
    return new temp::TempList();
  } else
    return this->dst_;
}

temp::TempList *OperInstr::Def() const {
  /* TODO: Put your lab6 code here */
  if (this->dst_ == nullptr) {
    return new temp::TempList();
  } else
    return this->dst_;
}

temp::TempList *LabelInstr::Use() const {
  /* TODO: Put your lab6 code here */
  return new temp::TempList();
}

temp::TempList *MoveInstr::Use() const {
  /* TODO: Put your lab6 code here */
  if (this->src_ == nullptr) {
    return new temp::TempList();
  } else
    return this->src_;
}

temp::TempList *OperInstr::Use() const {
  /* TODO: Put your lab6 code here */
  if (this->src_ == nullptr) {
    return new temp::TempList();
  } else
    return this->src_;
}
} // namespace assem
