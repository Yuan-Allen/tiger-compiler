#include "tiger/frame/x64frame.h"

extern frame::RegManager *reg_manager;

namespace frame {
/* TODO: Put your lab5 code here */
class InFrameAccess : public Access {
public:
  int offset;

  explicit InFrameAccess(int offset) : offset(offset) {}
  /* TODO: Put your lab5 code here */
  tree::Exp *ToExp(tree::Exp *frame_ptr) const override;
};

class InRegAccess : public Access {
public:
  temp::Temp *reg;

  explicit InRegAccess(temp::Temp *reg) : reg(reg) {}
  /* TODO: Put your lab5 code here */
  tree::Exp *ToExp(tree::Exp *frame_ptr) const override {
    // fprintf(stderr, "reg toExp\n");
    return new tree::TempExp(reg);
  }
};

class X64Frame : public Frame {
  /* TODO: Put your lab5 code here */
public:
  X64Frame(temp::Label *name, std::list<bool> formals) : Frame(name, formals) {
    offset = 0;
    formals_ = new std::list<frame::Access *>();
    for (bool escape : formals) {
      formals_->push_back(AllocLocal(escape));
    }

    int i = 0;
    temp::TempList *temp_list = reg_manager->ArgRegs();
    view_shift = new tree::ExpStm(new tree::ConstExp(0));
    for (frame::Access *access : *formals_) {
      if (i < 6) {
        view_shift = new tree::SeqStm(
            view_shift,
            new tree::MoveStm(
                access->ToExp(new tree::TempExp(reg_manager->FramePointer())),
                new tree::TempExp(temp_list->NthTemp(i))));
      } else {
        view_shift = new tree::SeqStm(
            view_shift,
            new tree::MoveStm(
                access->ToExp(new tree::TempExp(reg_manager->FramePointer())),
                new tree::MemExp(new tree::BinopExp(
                    tree::BinOp::PLUS_OP,
                    new tree::TempExp(reg_manager->FramePointer()),
                    new tree::ConstExp((i + 1 - 6) *
                                       reg_manager->WordSize())))));
      }
      ++i;
    }
  }

  frame::Access *AllocLocal(bool escape) override {
    frame::Access *access = nullptr;
    if (escape) {
      offset -= reg_manager->WordSize();
      access = new InFrameAccess(offset);
    } else {
      access = new InRegAccess(temp::TempFactory::NewTemp());
    }
    return access;
  }

  std::list<frame::Access *> GetFormals() override { return *formals_; }
};
/* TODO: Put your lab5 code here */

Frame *NewFrame(temp::Label *name, std::list<bool> formals) {
  return new X64Frame(name, formals);
}

tree::Exp *externalCall(const std::string &name, tree::ExpList *args) {
  return new tree::CallExp(
      new tree::NameExp(temp::LabelFactory::NamedLabel(name)), args);
}

tree::Exp *InFrameAccess::ToExp(tree::Exp *frame_ptr) const {
  // fprintf(stderr, " frame toExp\n");
  return new tree::MemExp(new tree::BinopExp(tree::BinOp::PLUS_OP, frame_ptr,
                                             new tree::ConstExp(offset)));
}

tree::Stm *procEntryExit1(Frame *f, tree::Stm *stm) {
  return new tree::SeqStm(f->view_shift, stm);
}

assem::InstrList *procEntryExit2(assem::InstrList *body) {
  body->Append(new assem::OperInstr("", new temp::TempList(),
                                    reg_manager->ReturnSink(), nullptr));
  return body;
}

assem::Proc *ProcEntryExit3(frame::Frame *f, assem::InstrList *body) {
  // ".set xx_framesize, size"
  std::string prolog = ".set " + f->name_->Name() + "_framesize, " +
                       std::to_string(-(f->offset)) + "\n";
  // "name:"
  prolog += f->name_->Name() + ":\n";
  // "subq $size, %rsp"
  prolog += "subq $" + std::to_string(-(f->offset)) + ", %rsp\n";

  //"addq $size, %rsp"
  std::string epilog = "addq $" + std::to_string(-(f->offset)) + ", %rsp\n";
  epilog += "retq\n";
  return new assem::Proc(prolog, body, epilog);
}

X64RegManager::X64RegManager() {
  temp::Temp *t = nullptr;
  for (size_t i = 0; i < 16; i++) {
    t = temp::TempFactory::NewTemp();
    regs_.push_back(t);
  }
  temp_map_->Enter(GetRegister(0), new std::string("%rax"));
  temp_map_->Enter(GetRegister(1), new std::string("%rbx"));
  temp_map_->Enter(GetRegister(2), new std::string("%rcx"));
  temp_map_->Enter(GetRegister(3), new std::string("%rdx"));
  temp_map_->Enter(GetRegister(4), new std::string("%rsi"));
  temp_map_->Enter(GetRegister(5), new std::string("%rdi"));
  temp_map_->Enter(GetRegister(6), new std::string("%rbp"));
  temp_map_->Enter(GetRegister(7), new std::string("%rsp"));
  temp_map_->Enter(GetRegister(8), new std::string("%r8"));
  temp_map_->Enter(GetRegister(9), new std::string("%r9"));
  temp_map_->Enter(GetRegister(10), new std::string("%r10"));
  temp_map_->Enter(GetRegister(11), new std::string("%r11"));
  temp_map_->Enter(GetRegister(12), new std::string("%r12"));
  temp_map_->Enter(GetRegister(13), new std::string("%r13"));
  temp_map_->Enter(GetRegister(14), new std::string("%r14"));
  temp_map_->Enter(GetRegister(15), new std::string("%r15"));
}

temp::TempList *X64RegManager::Registers() {
  temp::TempList *temp_list = new temp::TempList();
  for (size_t i = 0; i < 16; i++) {
    if (i == 4)
      continue;
    temp_list->Append(GetRegister(i));
  }
  return temp_list;
}

temp::TempList *X64RegManager::ArgRegs() {
  temp::TempList *temp_list = new temp::TempList();
  temp_list->Append(GetRegister(5)); //%rdi
  temp_list->Append(GetRegister(4)); //%rsi
  temp_list->Append(GetRegister(3)); //%rdx
  temp_list->Append(GetRegister(2)); //%rcx
  temp_list->Append(GetRegister(8)); //%r8
  temp_list->Append(GetRegister(9)); //%r9
  return temp_list;
}

temp::TempList *X64RegManager::CallerSaves() {
  temp::TempList *temp_list = new temp::TempList();
  temp_list->Append(GetRegister(0));  //%rax
  temp_list->Append(GetRegister(2));  //%rcx
  temp_list->Append(GetRegister(3));  //%rdx
  temp_list->Append(GetRegister(4));  //%rsi
  temp_list->Append(GetRegister(5));  //%rdi
  temp_list->Append(GetRegister(8));  //%r8
  temp_list->Append(GetRegister(9));  //%r9
  temp_list->Append(GetRegister(10)); //%r10
  temp_list->Append(GetRegister(11)); //%r11
  return temp_list;
}

temp::TempList *X64RegManager::CalleeSaves() {
  temp::TempList *temp_list = new temp::TempList();
  temp_list->Append(GetRegister(1));  //%rbx
  temp_list->Append(GetRegister(6));  //%rbp
  temp_list->Append(GetRegister(12)); //%r12
  temp_list->Append(GetRegister(13)); //%r13
  temp_list->Append(GetRegister(14)); //%r14
  temp_list->Append(GetRegister(15)); //%r15
  return temp_list;
}

temp::TempList *X64RegManager::ReturnSink() {
  // return nullptr; // for lab5 part1
  temp::TempList *temp_list = CalleeSaves();
  temp_list->Append(StackPointer()); //%rsp
  temp_list->Append(ReturnValue());  //%rax
  return temp_list;
}

int X64RegManager::WordSize() { return 8; }

temp::Temp *X64RegManager::FramePointer() {
  return GetRegister(6); //%rbp
}

temp::Temp *X64RegManager::StackPointer() {
  return GetRegister(7); //%rsp
}

temp::Temp *X64RegManager::ReturnValue() {
  return GetRegister(0); //%rax
}

temp::TempList *X64RegManager::OpRegs() {
  temp::TempList *temp_list = new temp::TempList();
  temp_list->Append(GetRegister(3)); //%rdx
  temp_list->Append(GetRegister(0)); //%rax
  return temp_list;
}

} // namespace frame