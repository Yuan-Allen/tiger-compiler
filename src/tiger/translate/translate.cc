#include "tiger/translate/translate.h"

#include <tiger/absyn/absyn.h>

#include "tiger/env/env.h"
#include "tiger/errormsg/errormsg.h"
#include "tiger/frame/frame.h"
#include "tiger/frame/temp.h"
#include "tiger/frame/x64frame.h"

extern frame::Frags *frags;
extern frame::RegManager *reg_manager;

namespace tr {

Access *Access::AllocLocal(Level *level, bool escape) {
  /* TODO: Put your lab5 code here */
  return new tr::Access(level, level->frame_->AllocLocal(escape));
}

class Cx {
public:
  temp::Label **trues_;
  temp::Label **falses_;
  tree::Stm *stm_;

  Cx(temp::Label **trues, temp::Label **falses, tree::Stm *stm)
      : trues_(trues), falses_(falses), stm_(stm) {}
};

class Exp {
public:
  [[nodiscard]] virtual tree::Exp *UnEx() const = 0;
  [[nodiscard]] virtual tree::Stm *UnNx() const = 0;
  [[nodiscard]] virtual Cx UnCx(err::ErrorMsg *errormsg) const = 0;
};

class ExpAndTy {
public:
  tr::Exp *exp_;
  type::Ty *ty_;

  ExpAndTy(tr::Exp *exp, type::Ty *ty) : exp_(exp), ty_(ty) {}
};

class ExExp : public Exp {
public:
  tree::Exp *exp_;

  explicit ExExp(tree::Exp *exp) : exp_(exp) {}

  [[nodiscard]] tree::Exp *UnEx() const override {
    /* TODO: Put your lab5 code here */
    return exp_;
  }
  [[nodiscard]] tree::Stm *UnNx() const override {
    /* TODO: Put your lab5 code here */
    return new tree::ExpStm(exp_);
  }
  [[nodiscard]] Cx UnCx(err::ErrorMsg *errormsg) const override {
    /* TODO: Put your lab5 code here */
    temp::Label *t = temp::LabelFactory::NewLabel();
    temp::Label *f = temp::LabelFactory::NewLabel();
    tree::CjumpStm *stm =
        new tree::CjumpStm(tree::NE_OP, exp_, new tree::ConstExp(0), t, f);
    return tr::Cx(&(stm->true_label_), &(stm->false_label_), stm);
  }
};

class NxExp : public Exp {
public:
  tree::Stm *stm_;

  explicit NxExp(tree::Stm *stm) : stm_(stm) {}

  [[nodiscard]] tree::Exp *UnEx() const override {
    /* TODO: Put your lab5 code here */
    return new tree::EseqExp(stm_, new tree::ConstExp(0));
  }
  [[nodiscard]] tree::Stm *UnNx() const override {
    /* TODO: Put your lab5 code here */
    return stm_;
  }
  [[nodiscard]] Cx UnCx(err::ErrorMsg *errormsg) const override {
    /* TODO: Put your lab5 code here */
    assert(false);
    return tr::Cx(nullptr, nullptr, nullptr);
  }
};

class CxExp : public Exp {
public:
  Cx cx_;

  CxExp(temp::Label **trues, temp::Label **falses, tree::Stm *stm)
      : cx_(trues, falses, stm) {}

  [[nodiscard]] tree::Exp *UnEx() const override {
    /* TODO: Put your lab5 code here */
    temp::Temp *r = temp::TempFactory::NewTemp();
    temp::Label *t = temp::LabelFactory::NewLabel();
    temp::Label *f = temp::LabelFactory::NewLabel();
    *(cx_.trues_) = t;
    *(cx_.falses_) = f;
    return new tree::EseqExp(
        new tree::MoveStm(new tree::TempExp(r), new tree::ConstExp(1)),
        new tree::EseqExp(
            cx_.stm_,
            new tree::EseqExp(
                new tree::LabelStm(f),
                new tree::EseqExp(new tree::MoveStm(new tree::TempExp(r),
                                                    new tree::ConstExp(0)),
                                  new tree::EseqExp(new tree::LabelStm(t),
                                                    new tree::TempExp(r))))));
  }
  [[nodiscard]] tree::Stm *UnNx() const override {
    /* TODO: Put your lab5 code here */
    return new tree::ExpStm(UnEx());
  }
  [[nodiscard]] Cx UnCx(err::ErrorMsg *errormsg) const override {
    /* TODO: Put your lab5 code here */
    return cx_;
  }
};

ProgTr::ProgTr(std::unique_ptr<absyn::AbsynTree> absyn_tree,
               std::unique_ptr<err::ErrorMsg> errormsg)
    : absyn_tree_(std::move(absyn_tree)), errormsg_(std::move(errormsg)),
      tenv_(std::make_unique<env::TEnv>()),
      venv_(std::make_unique<env::VEnv>()) {
  temp::Label *name = temp::LabelFactory::NamedLabel("main");
  main_level_ = std::make_unique<Level>(
      frame::NewFrame(name, std::list<bool>()), nullptr);
}

void ProgTr::Translate() {
  /* TODO: Put your lab5 code here */
  FillBaseVEnv();
  FillBaseTEnv();
  temp::Label *lable = temp::LabelFactory::NewLabel();
  absyn_tree_->Translate(venv_.get(), tenv_.get(), main_level_.get(), lable,
                         errormsg_.get());
}

tree::Exp *StaticLink(tr::Level *current, tr::Level *target) {
  tree::Exp *static_link = new tree::TempExp(reg_manager->FramePointer());
  while (current != target) {
    if (current == nullptr) {
      return nullptr;
    }
    static_link = current->frame_->formals_->front()->ToExp(static_link);
    current = current->parent_;
  }
  return static_link;
}

tr::Exp *SimpleVar(tr::Access *access, tr::Level *level) {
  tree::Exp *static_link = tr::StaticLink(level, access->level_);
  return new tr::ExExp(access->access_->ToExp(static_link));
}

tr::Exp *FieldVar(tr::Exp *exp, int word_count) {
  return new tr::ExExp(new tree::MemExp(new tree::BinopExp(
      tree::BinOp::PLUS_OP, exp->UnEx(),
      new tree::ConstExp(word_count * reg_manager->WordSize()))));
}

tr::Exp *SubscriptorVar(tr::Exp *exp, tr::Exp *index) {
  return new tr::ExExp(new tree::MemExp(new tree::BinopExp(
      tree::BinOp::PLUS_OP, exp->UnEx(),
      new tree::BinopExp(tree::BinOp::MUL_OP, index->UnEx(),
                         new tree::ConstExp(reg_manager->WordSize())))));
}

tr::Exp *NilExp() { return new tr::ExExp(new tree::ConstExp(0)); }

tr::Exp *IntExp(int val) { return new tr::ExExp(new tree::ConstExp(val)); }

tr::Exp *StringExp(std::string str) {
  temp::Label *label = temp::LabelFactory::NewLabel();
  frags->PushBack(new frame::StringFrag(label, str));
  return new tr::ExExp(new tree::NameExp(label));
}

tr::Exp *CallExp(temp::Label *label, tree::Exp *static_link,
                 std::list<tr::Exp *> args) {
  tree::ExpList *args_list = new tree::ExpList();
  for (tr::Exp *exp : args) {
    args_list->Append(exp->UnEx());
  }
  if (static_link != nullptr) {
    args_list->Insert(static_link);
    return new tr::ExExp(
        new tree::CallExp(new tree::NameExp(label), args_list));
  } else {
    return new tr::ExExp(
        frame::externalCall(temp::LabelFactory::LabelString(label), args_list));
  }
}

} // namespace tr

namespace absyn {

tr::ExpAndTy *AbsynTree::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,
                                   err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  root_->Translate(venv, tenv, level, label, errormsg);
}

tr::ExpAndTy *SimpleVar::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,
                                   err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  tr::Exp *exp = nullptr;
  env::EnvEntry *entry = venv->Look(sym_);
  assert(entry != nullptr && typeid(*entry) == typeid(env::VarEntry));
  env::VarEntry *var_entry = static_cast<env::VarEntry *>(entry);
  exp = tr::SimpleVar(var_entry->access_, level);
  return new tr::ExpAndTy(exp, var_entry->ty_->ActualTy());
}

tr::ExpAndTy *FieldVar::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                  tr::Level *level, temp::Label *label,
                                  err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  tr::ExpAndTy *var_info = var_->Translate(venv, tenv, level, label, errormsg);
  assert(var_info->exp_ != nullptr);
  assert(typeid(*(var_info->ty_->ActualTy())) == typeid(type::RecordTy));
  type::RecordTy *record_ty =
      static_cast<type::RecordTy *>(var_info->ty_->ActualTy());
  std::list<type::Field *> field_list = record_ty->fields_->GetList();
  int word_count = 0;
  for (type::Field *field : field_list) {
    if (field->name_->Name() == sym_->Name()) {
      return new tr::ExpAndTy(tr::FieldVar(var_info->exp_, word_count),
                              var_info->ty_->ActualTy());
    }
    word_count++;
  }
  errormsg->Error(pos_, "field %s dosen't exist", sym_->Name().c_str());
  return new tr::ExpAndTy(nullptr, type::IntTy::Instance());
}

tr::ExpAndTy *SubscriptVar::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                      tr::Level *level, temp::Label *label,
                                      err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  tr::ExpAndTy *var_info = var_->Translate(venv, tenv, level, label, errormsg);
  tr::ExpAndTy *subscript_info =
      subscript_->Translate(venv, tenv, level, label, errormsg);
  type::Ty *ty = var_info->ty_->ActualTy();
  assert(typeid(*ty) == typeid(type::ArrayTy));
  return new tr::ExpAndTy(
      tr::SubscriptorVar(var_info->exp_, subscript_info->exp_),
      static_cast<type::ArrayTy *>(ty)->ty_->ActualTy());
}

tr::ExpAndTy *VarExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  assert(typeid(*var_) == typeid(absyn::SimpleVar) ||
         typeid(*var_) == typeid(absyn::FieldVar) ||
         typeid(*var_) == typeid(absyn::SubscriptVar));
  return var_->Translate(venv, tenv, level, label, errormsg);
}

tr::ExpAndTy *NilExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  return new tr::ExpAndTy(tr::NilExp(), type::NilTy::Instance());
}

tr::ExpAndTy *IntExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  return new tr::ExpAndTy(tr::IntExp(val_), type::IntTy::Instance());
}

tr::ExpAndTy *StringExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,
                                   err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  return new tr::ExpAndTy(tr::StringExp(str_), type::StringTy::Instance());
}

tr::ExpAndTy *CallExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                 tr::Level *level, temp::Label *label,
                                 err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  env::EnvEntry *entry = venv->Look(func_);
  if (entry == nullptr || typeid(*entry) != typeid(env::FunEntry)) {
    errormsg->Error(pos_, "undefined function %s", func_->Name().c_str());
    return new tr::ExpAndTy(nullptr, type::IntTy::Instance());
  }
  env::FunEntry *fun_entry = static_cast<env::FunEntry *>(entry);
  std::list<tr::Exp *> args_trexp;
  for (absyn::Exp *exp : args_->GetList()) {
    tr::ExpAndTy *arg = exp->Translate(venv, tenv, level, label, errormsg);
    args_trexp.push_back(arg->exp_);
  }
  return new tr::ExpAndTy(
      tr::CallExp(this->func_,
                  tr::StaticLink(level, fun_entry->level_->parent_),
                  args_trexp),
      fun_entry->result_);
}

tr::ExpAndTy *OpExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                               tr::Level *level, temp::Label *label,
                               err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
}

tr::ExpAndTy *RecordExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,
                                   err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
}

tr::ExpAndTy *SeqExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
}

tr::ExpAndTy *AssignExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,
                                   err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
}

tr::ExpAndTy *IfExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                               tr::Level *level, temp::Label *label,
                               err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
}

tr::ExpAndTy *WhileExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                  tr::Level *level, temp::Label *label,
                                  err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
}

tr::ExpAndTy *ForExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
}

tr::ExpAndTy *BreakExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                  tr::Level *level, temp::Label *label,
                                  err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
}

tr::ExpAndTy *LetExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
}

tr::ExpAndTy *ArrayExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                  tr::Level *level, temp::Label *label,
                                  err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
}

tr::ExpAndTy *VoidExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                 tr::Level *level, temp::Label *label,
                                 err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
}

tr::Exp *FunctionDec::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
}

tr::Exp *VarDec::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                           tr::Level *level, temp::Label *label,
                           err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
}

tr::Exp *TypeDec::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                            tr::Level *level, temp::Label *label,
                            err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
}

type::Ty *NameTy::Translate(env::TEnvPtr tenv, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
}

type::Ty *RecordTy::Translate(env::TEnvPtr tenv,
                              err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
}

type::Ty *ArrayTy::Translate(env::TEnvPtr tenv, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
}

} // namespace absyn
