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
  }

  frame::Access *AllocLocal(bool escape) {
    frame::Access *access = nullptr;
    if (escape) {
      offset -= reg_manager->WordSize();
      access = new InFrameAccess(offset);
    } else {
      access = new InRegAccess(temp::TempFactory::NewTemp());
    }
    return access;
  }
};
/* TODO: Put your lab5 code here */

tree::Exp *InFrameAccess::ToExp(tree::Exp *frame_ptr) const {
  return new tree::MemExp(new tree::BinopExp(tree::BinOp::PLUS_OP, frame_ptr,
                                             new tree::ConstExp(offset)));
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
  temp_list->Append(GetRegister(1));  //%rax
  temp_list->Append(GetRegister(6));  //%rbp
  temp_list->Append(GetRegister(12)); //%r12
  temp_list->Append(GetRegister(13)); //%r13
  temp_list->Append(GetRegister(14)); //%r14
  temp_list->Append(GetRegister(15)); //%r15
}

temp::TempList *X64RegManager::ReturnSink() {
  return nullptr; // for lab5 part1
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

} // namespace frame