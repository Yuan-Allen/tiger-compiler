#include "tiger/codegen/codegen.h"

#include <cassert>
#include <sstream>

extern frame::RegManager *reg_manager;

namespace {

constexpr int maxlen = 1024;

} // namespace

namespace cg {

void CodeGen::Codegen() {
  /* TODO: Put your lab5 code here */
  fs_ = frame_->name_->Name() + "_framesize";
  assem::InstrList *instrList = new assem::InstrList();
  temp::TempList *calleeSaved = reg_manager->CalleeSaves();
  temp::TempList *storeCalleeSaved = new temp::TempList();

  // store callee save regs
  for (temp::Temp *reg : calleeSaved->GetList()) {
    temp::Temp *storeReg = temp::TempFactory::NewTemp();
    instrList->Append(new assem::MoveInstr("movq `s0, `d0",
                                           new temp::TempList({storeReg}),
                                           new temp::TempList({reg})));
    storeCalleeSaved->Append(storeReg);
  }

  // munch
  for (tree::Stm *stm : traces_->GetStmList()->GetList()) {
    stm->Munch(*instrList, fs_);
  }

  // restore callee save regs
  for (int i = 0; i < static_cast<int>(calleeSaved->GetList().size()); ++i) {
    instrList->Append(new assem::MoveInstr(
        "movq `s0, `d0", new temp::TempList({calleeSaved->NthTemp(i)}),
        new temp::TempList({storeCalleeSaved->NthTemp(i)})));
  }

  assem_instr_ = std::make_unique<AssemInstr>(frame::procEntryExit2(instrList));
}

void AssemInstr::Print(FILE *out, temp::Map *map) const {
  for (auto instr : instr_list_->GetList())
    instr->Print(out, map);
  fprintf(out, "\n");
}
} // namespace cg

namespace tree {
/* TODO: Put your lab5 code here */

void SeqStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  this->left_->Munch(instr_list, fs);
  this->right_->Munch(instr_list, fs);
}

void LabelStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  instr_list.Append(new assem::LabelInstr(
      temp::LabelFactory::LabelString(this->label_), this->label_));
}

void JumpStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  instr_list.Append(new assem::OperInstr("jmp `j0", nullptr, nullptr,
                                         new assem::Targets(this->jumps_)));
}

void CjumpStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  temp::Temp *left_temp = left_->Munch(instr_list, fs);
  temp::Temp *right_temp = right_->Munch(instr_list, fs);
  std::string op;
  switch (this->op_) {
  case tree::RelOp::EQ_OP:
    op = "je";
    break;
  case tree::RelOp::NE_OP:
    op = "jne";
    break;
  case tree::RelOp::LT_OP:
    op = "jl";
    break;
  case tree::RelOp::LE_OP:
    op = "jle";
    break;
  case tree::RelOp::GE_OP:
    op = "jge";
    break;
  case tree::RelOp::GT_OP:
    op = "jg";
    break;
  case tree::RelOp::ULT_OP:
    op = "jb";
    break;
  case tree::RelOp::ULE_OP:
    op = "jbe";
    break;
  case tree::RelOp::UGE_OP:
    op = "jae";
    break;
  case tree::RelOp::UGT_OP:
    op = "ja";
    break;
  default:
    assert(false);
    break;
  }
  instr_list.Append(new assem::OperInstr(
      "cmpq `s1, `s0", nullptr, new temp::TempList({left_temp, right_temp}),
      nullptr));
  instr_list.Append(
      new assem::OperInstr(op + " `j0", nullptr, nullptr,
                           new assem::Targets(new std::vector<temp::Label *>(
                               {this->true_label_, this->false_label_}))));
}

void MoveStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  if (typeid(*dst_) == typeid(tree::TempExp)) {
    temp::Temp *s = src_->Munch(instr_list, fs);
    instr_list.Append(new assem::MoveInstr(
        "movq `s0, `d0",
        new temp::TempList({static_cast<tree::TempExp *>(dst_)->temp_}),
        new temp::TempList({s})));
  } else if (typeid(*dst_) == typeid(tree::MemExp)) {
    temp::Temp *s = src_->Munch(instr_list, fs);
    temp::Temp *d =
        static_cast<tree::MemExp *>(dst_)->exp_->Munch(instr_list, fs);
    instr_list.Append(new assem::OperInstr(
        "movq `s0, (`s1)", nullptr, new temp::TempList({s, d}), nullptr));
  } else {
    // should not go here
    assert(false);
  }
}

void ExpStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  this->exp_->Munch(instr_list, fs);
}

temp::Temp *BinopExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  temp::Temp *left = left_->Munch(instr_list, fs);
  temp::Temp *right = right_->Munch(instr_list, fs);
  temp::Temp *reg = temp::TempFactory::NewTemp();
  instr_list.Append(new assem::MoveInstr(
      "movq `s0, `d0", new temp::TempList({reg}), new temp::TempList({left})));
  switch (this->op_) {
  case tree::BinOp::PLUS_OP:
    // reg是隐藏的src
    instr_list.Append(
        new assem::OperInstr("addq `s0, `d0", new temp::TempList({reg}),
                             new temp::TempList({right, reg}), nullptr));
    break;
  case tree::BinOp::MINUS_OP:
    instr_list.Append(
        new assem::OperInstr("subq `s0, `d0", new temp::TempList({reg}),
                             new temp::TempList({right, reg}), nullptr));
    break;
  case tree::BinOp::MUL_OP:
    // instr_list.Append(
    //     new assem::OperInstr("imulq `s0, `d0", new temp::TempList({reg}),
    //                          new temp::TempList({right, reg}), nullptr));
    instr_list.Append(new assem::MoveInstr(
        "movq `s0, `d0", new temp::TempList({reg_manager->ReturnValue()}),
        new temp::TempList({left})));
    instr_list.Append(new assem::OperInstr("imulq `s0", reg_manager->OpRegs(),
                                           new temp::TempList({right}),
                                           nullptr));
    instr_list.Append(
        new assem::MoveInstr("movq `s0, `d0", new temp::TempList({reg}),
                             new temp::TempList({reg_manager->ReturnValue()})));
    break;
  case tree::BinOp::DIV_OP:
    instr_list.Append(new assem::MoveInstr(
        "movq `s0, `d0", new temp::TempList({reg_manager->ReturnValue()}),
        new temp::TempList({left})));
    instr_list.Append(new assem::OperInstr(
        "cqto", reg_manager->OpRegs(),
        new temp::TempList({reg_manager->ReturnValue()}), nullptr));
    instr_list.Append(new assem::OperInstr("idivq `s0", reg_manager->OpRegs(),
                                           new temp::TempList({right}),
                                           nullptr));
    instr_list.Append(
        new assem::MoveInstr("movq `s0, `d0", new temp::TempList({reg}),
                             new temp::TempList({reg_manager->ReturnValue()})));
    break;
  default:
    assert(false);
    break;
  }
  return reg;
}

temp::Temp *MemExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  temp::Temp *ret = temp::TempFactory::NewTemp();
  temp::Temp *r = exp_->Munch(instr_list, fs);
  instr_list.Append(new assem::OperInstr("movq (`s0), `d0",
                                         new temp::TempList({ret}),
                                         new temp::TempList(r), nullptr));
  return ret;
}

temp::Temp *TempExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  temp::Temp *ret = temp::TempFactory::NewTemp();
  if (temp_ == reg_manager->FramePointer()) {
    // "leaq xx_framesize(`s0), `d0"
    std::string str = "leaq " + std::string(fs.data()) + "(`s0), `d0";
    instr_list.Append(new assem::OperInstr(
        str, new temp::TempList({ret}),
        new temp::TempList({reg_manager->StackPointer()}), nullptr));
    return ret;
  } else {
    return this->temp_;
  }
}

temp::Temp *EseqExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  assert(this->stm_ != nullptr);
  assert(this->exp_ != nullptr);
  stm_->Munch(instr_list, fs);
  return exp_->Munch(instr_list, fs);
}

temp::Temp *NameExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  temp::Temp *ret = temp::TempFactory::NewTemp();
  // "leaq name(%rip), `d0"
  std::string str =
      "leaq " + temp::LabelFactory::LabelString(this->name_) + "(%rip), `d0";
  instr_list.Append(
      new assem::OperInstr(str, new temp::TempList({ret}), nullptr, nullptr));
  return ret;
}

temp::Temp *ConstExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  temp::Temp *ret = temp::TempFactory::NewTemp();
  // "movq $consti, `d0"
  std::string str = "movq $" + std::to_string(this->consti_) + ", `d0";
  instr_list.Append(
      new assem::OperInstr(str, new temp::TempList({ret}), nullptr, nullptr));
  return ret;
}

temp::Temp *CallExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  temp::Temp *ret = temp::TempFactory::NewTemp();
  temp::TempList *args = this->args_->MunchArgs(instr_list, fs);
  // "callq fun"
  std::string str =
      "callq " + temp::LabelFactory::LabelString(
                     static_cast<tree::NameExp *>(this->fun_)->name_);
  instr_list.Append(
      new assem::OperInstr(str, reg_manager->CallerSaves(), args, nullptr));
  instr_list.Append(
      new assem::MoveInstr("movq `s0, `d0", new temp::TempList({ret}),
                           new temp::TempList({reg_manager->ReturnValue()})));
  return ret;
}

temp::TempList *ExpList::MunchArgs(assem::InstrList &instr_list,
                                   std::string_view fs) {
  /* TODO: Put your lab5 code here */
  int i = 0;
  temp::TempList *arg_regs = reg_manager->ArgRegs();
  int reg_size = arg_regs->GetList().size(); // 6
  temp::TempList *ret = new temp::TempList();
  for (tree::Exp *exp : this->GetList()) {
    temp::Temp *arg = exp->Munch(instr_list, fs);
    if (i < reg_size) {
      instr_list.Append(new assem::MoveInstr(
          "movq `s0, `d0", new temp::TempList({arg_regs->NthTemp(i)}),
          new temp::TempList({arg})));
      ret->Append(arg_regs->NthTemp(i));
    } else {
      // instr_list.Append(new assem::OperInstr(
      //     "pushq `s0", nullptr, new temp::TempList({arg}), nullptr));
      // "movq `s0, offset(`s1)"
      std::string str =
          "movq `s0, " +
          std::to_string((i - reg_size) * reg_manager->WordSize()) + "(`s1)";
      instr_list.Append(new assem::OperInstr(
          str, nullptr, new temp::TempList({arg, reg_manager->StackPointer()}),
          nullptr));
    }
    ++i;
  }
  return ret;
}

} // namespace tree
