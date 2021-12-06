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
  temp::Label *name = temp::LabelFactory::NamedLabel("tigermain");
  main_level_ = std::make_unique<Level>(
      frame::NewFrame(name, std::list<bool>()), nullptr);
}

void ProgTr::Translate() {
  /* TODO: Put your lab5 code here */
  FillBaseVEnv();
  FillBaseTEnv();
  temp::Label *label = temp::LabelFactory::NewLabel();
  tr::ExpAndTy *root_info = absyn_tree_->Translate(
      venv_.get(), tenv_.get(), main_level_.get(), label, errormsg_.get());
  // tigermain也要当成一个函数，加入frags，作为程序入口
  frags->PushBack(
      new frame::ProcFrag(root_info->exp_->UnNx(), main_level_->frame_));
}

tree::Exp *StaticLink(tr::Level *current, tr::Level *target) {
  // fprintf(stderr, "static link begin\n");
  tree::Exp *static_link = new tree::TempExp(reg_manager->FramePointer());
  while (current != target) {
    if (current == nullptr || current->parent_ == nullptr) {
      // fprintf(stderr, "static nullptr end\n");
      return nullptr;
    }
    static_link = current->frame_->formals_->front()->ToExp(static_link);
    current = current->parent_;
  }
  // fprintf(stderr, "static link end\n");
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
                 std::list<tr::Exp *> args, tr::Level *caller) {
  tree::ExpList *args_list = new tree::ExpList();
  for (tr::Exp *exp : args) {
    args_list->Append(exp->UnEx());
  }
  if (static_link != nullptr) {
    args_list->Insert(static_link);
    if (static_cast<int>(args.size() + 1) > caller->frame_->maxArgs) {
      caller->frame_->maxArgs = static_cast<int>(args.size() + 1);
    }
    return new tr::ExExp(
        new tree::CallExp(new tree::NameExp(label), args_list));
  } else {
    if (static_cast<int>(args.size()) > caller->frame_->maxArgs) {
      caller->frame_->maxArgs = static_cast<int>(args.size());
    }
    return new tr::ExExp(
        frame::externalCall(temp::LabelFactory::LabelString(label), args_list));
  }
}

tr::Exp *OpExp(absyn::Oper oper, tr::Exp *left, tr::Exp *right,
               bool string_eq) {
  // string_eq为true表明是字符串类型相等的判定，需要调用external call
  if (string_eq) {
    tree::ExpList *exp_list = new tree::ExpList();
    exp_list->Append(left->UnEx());
    exp_list->Append(right->UnEx());
    return new tr::ExExp(frame::externalCall("string_equal", exp_list));
  }
  tree::CjumpStm *stm = nullptr;
  switch (oper) {
  // + - * /
  case absyn::Oper::PLUS_OP:
    return new tr::ExExp(
        new tree::BinopExp(tree::BinOp::PLUS_OP, left->UnEx(), right->UnEx()));
    break;
  case absyn::Oper::MINUS_OP:
    return new tr::ExExp(
        new tree::BinopExp(tree::BinOp::MINUS_OP, left->UnEx(), right->UnEx()));
    break;
  case absyn::Oper::TIMES_OP:
    return new tr::ExExp(
        new tree::BinopExp(tree::BinOp::MUL_OP, left->UnEx(), right->UnEx()));
    break;
  case absyn::Oper::DIVIDE_OP:
    return new tr::ExExp(
        new tree::BinopExp(tree::BinOp::DIV_OP, left->UnEx(), right->UnEx()));
    break;
  // == != < <= > >=
  case absyn::Oper::EQ_OP:
    stm = new tree::CjumpStm(tree::RelOp::EQ_OP, left->UnEx(), right->UnEx(),
                             nullptr, nullptr);
    break;
  case absyn::Oper::NEQ_OP:
    stm = new tree::CjumpStm(tree::RelOp::NE_OP, left->UnEx(), right->UnEx(),
                             nullptr, nullptr);
    break;
  case absyn::Oper::LT_OP:
    stm = new tree::CjumpStm(tree::RelOp::LT_OP, left->UnEx(), right->UnEx(),
                             nullptr, nullptr);
    break;
  case absyn::Oper::LE_OP:
    stm = new tree::CjumpStm(tree::RelOp::LE_OP, left->UnEx(), right->UnEx(),
                             nullptr, nullptr);
    break;
  case absyn::Oper::GT_OP:
    stm = new tree::CjumpStm(tree::RelOp::GT_OP, left->UnEx(), right->UnEx(),
                             nullptr, nullptr);
    break;
  case absyn::Oper::GE_OP:
    stm = new tree::CjumpStm(tree::RelOp::GE_OP, left->UnEx(), right->UnEx(),
                             nullptr, nullptr);
    break;
  }
  // 其他情况都在上面return过了，下面只有整数类型的比较
  return new CxExp(&(stm->true_label_), &(stm->false_label_), stm);
}

tree::Stm *RecordField(const std::vector<tr::Exp *> &exp_list, temp::Temp *r,
                       int index) {
  if (exp_list.size() == 0)
    return nullptr;
  if (index == static_cast<int>(exp_list.size() - 1)) {
    return new tree::MoveStm(
        new tree::MemExp(new tree::BinopExp(
            tree::BinOp::PLUS_OP, new tree::TempExp(r),
            new tree::ConstExp(index * reg_manager->WordSize()))),
        exp_list[index]->UnEx());
  } else {
    return new tree::SeqStm(
        new tree::MoveStm(
            new tree::MemExp(new tree::BinopExp(
                tree::BinOp::PLUS_OP, new tree::TempExp(r),
                new tree::ConstExp(index * reg_manager->WordSize()))),
            exp_list[index]->UnEx()),
        tr::RecordField(exp_list, r, index + 1)); // 用index+1递归
  }
}

tr::Exp *RecordExp(std::vector<tr::Exp *> exp_list) {
  temp::Temp *r = temp::TempFactory::NewTemp();
  tree::ExpList *externalCall_arg = new tree::ExpList();
  externalCall_arg->Append(new tree::ConstExp(
      reg_manager->WordSize() * static_cast<int>(exp_list.size())));
  tree::Stm *stm =
      new tree::MoveStm(new tree::TempExp(r),
                        frame::externalCall("alloc_record", externalCall_arg));
  stm = new tree::SeqStm(stm, RecordField(exp_list, r, 0));
  return new tr::ExExp(new tree::EseqExp(stm, new tree::TempExp(r)));
}

tr::Exp *SeqExp(std::list<tr::Exp *> seqExp) {
  tr::Exp *ret = new tr::ExExp(new tree::ConstExp(0));
  for (tr::Exp *exp : seqExp) {
    // 内存泄漏？
    if (exp != nullptr) {
      ret = new tr::ExExp(new tree::EseqExp(ret->UnNx(), exp->UnEx()));
    } else {
      ret =
          new tr::ExExp(new tree::EseqExp(ret->UnNx(), new tree::ConstExp(0)));
    }
  }
  return ret;
}

tr::Exp *AssignExp(tr::Exp *var, tr::Exp *exp) {
  return new tr::NxExp(new tree::MoveStm(var->UnEx(), exp->UnEx()));
}

tr::Exp *IfExp(tr::Exp *test, tr::Exp *then, tr::Exp *elsee,
               err::ErrorMsg *errormsg) {
  temp::Temp *r = temp::TempFactory::NewTemp();
  temp::Label *t = temp::LabelFactory::NewLabel(),
              *f = temp::LabelFactory::NewLabel();
  temp::Label *joint = temp::LabelFactory::NewLabel();
  std::vector<temp::Label *> *joint_label_list =
      new std::vector<temp::Label *>();
  joint_label_list->push_back(joint);
  Cx cx = test->UnCx(errormsg);
  *(cx.trues_) = t;
  *(cx.falses_) = f;
  if (elsee != nullptr) {
    return new ExExp(new tree::EseqExp(
        cx.stm_,
        new tree::EseqExp(
            new tree::LabelStm(t),
            new tree::EseqExp(
                new tree::MoveStm(new tree::TempExp(r), then->UnEx()),
                new tree::EseqExp(
                    new tree::JumpStm(new tree::NameExp(joint),
                                      joint_label_list),
                    new tree::EseqExp(
                        new tree::LabelStm(f),
                        new tree::EseqExp(
                            new tree::MoveStm(new tree::TempExp(r),
                                              elsee->UnEx()),
                            new tree::EseqExp(
                                new tree::JumpStm(new tree::NameExp(joint),
                                                  joint_label_list),
                                new tree::EseqExp(new tree::LabelStm(joint),
                                                  new tree::TempExp(r))))))))));
  } else {
    return new NxExp(new tree::SeqStm(
        cx.stm_, new tree::SeqStm(
                     new tree::LabelStm(t),
                     new tree::SeqStm(then->UnNx(), new tree::LabelStm(f)))));
  }
}

// done_label从外面传进来是因为body_的Translate也需要用到done_label(用于break)
tr::Exp *WhileExp(tr::Exp *test, tr::Exp *body, temp::Label *done_label,
                  err::ErrorMsg *errormsg) {
  Cx cx = test->UnCx(errormsg);
  temp::Label *test_label = temp::LabelFactory::NewLabel();
  temp::Label *body_label = temp::LabelFactory::NewLabel();
  *(cx.trues_) = body_label;
  *(cx.falses_) = done_label;
  std::vector<temp::Label *> *test_label_list =
      new std::vector<temp::Label *>();
  test_label_list->push_back(test_label);
  return new tr::NxExp(new tree::SeqStm(
      new tree::LabelStm(test_label),
      new tree::SeqStm(
          cx.stm_, new tree::SeqStm(
                       new tree::LabelStm(body_label),
                       new tree::SeqStm(
                           body->UnNx(),
                           new tree::SeqStm(
                               new tree::JumpStm(new tree::NameExp(test_label),
                                                 test_label_list),
                               new tree::LabelStm(done_label)))))));
}

tr::Exp *ForExp(frame::Access *access, tr::Exp *lo, tr::Exp *hi, tr::Exp *body,
                temp::Label *done_label) {
  temp::Label *body_label = temp::LabelFactory::NewLabel();
  temp::Label *test_label = temp::LabelFactory::NewLabel();
  tree::Exp *i = access->ToExp(new tree::TempExp(reg_manager->FramePointer()));
  return new tr::NxExp(new tree::SeqStm(
      new tree::MoveStm(i, lo->UnEx()),
      new tree::SeqStm(
          new tree::LabelStm(test_label),
          new tree::SeqStm(
              new tree::CjumpStm(tree::RelOp::LE_OP, i, hi->UnEx(), body_label,
                                 done_label),
              new tree::SeqStm(
                  new tree::LabelStm(body_label),
                  new tree::SeqStm(
                      body->UnNx(),
                      new tree::SeqStm(
                          new tree::MoveStm(
                              i, new tree::BinopExp(tree::BinOp::PLUS_OP, i,
                                                    new tree::ConstExp(1))),
                          new tree::SeqStm(
                              new tree::JumpStm(new tree::NameExp(test_label),
                                                new std::vector<temp::Label *>(
                                                    1, test_label)),
                              new tree::LabelStm(done_label)))))))));
}

tr::Exp *BreakExp(temp::Label *label) {
  return new tr::NxExp(new tree::JumpStm(
      new tree::NameExp(label), new std::vector<temp::Label *>(1, label)));
}

tr::Exp *LetExp(std::list<Exp *> dec_list, tr::Exp *body) {
  tr::Exp *ret = nullptr;
  for (tr::Exp *dec : dec_list) {
    if (ret == nullptr) {
      ret = dec;
    } else {
      if (dec != nullptr)
        ret = new tr::ExExp(new tree::EseqExp(ret->UnNx(), dec->UnEx()));
      else
        ret = new tr::ExExp(
            new tree::EseqExp(ret->UnNx(), new tree::ConstExp(0)));
    }
  }
  if (ret == nullptr) {
    ret = body;
  } else {
    ret = new tr::ExExp(new tree::EseqExp(ret->UnNx(), body->UnEx()));
  }
  return ret;
}

tr::Exp *ArrayExp(tr::Exp *size, tr::Exp *init) {
  return new tr::ExExp(frame::externalCall(
      "init_array", new tree::ExpList({size->UnEx(), init->UnEx()})));
}

// 加入到frags即可，不需要返回什么
void FunctionDec(tr::Exp *body, tr::Level *level) {
  tree::Stm *stm = new tree::MoveStm(
      new tree::TempExp(reg_manager->ReturnValue()), body->UnEx());
  stm = frame::procEntryExit1(level->frame_, stm);
  frags->PushBack(new frame::ProcFrag(stm, level->frame_));
}

} // namespace tr

namespace absyn {

tr::ExpAndTy *AbsynTree::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,
                                   err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  return root_->Translate(venv, tenv, level, label, errormsg);
}

tr::ExpAndTy *SimpleVar::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,
                                   err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  // fprintf(stderr, "SimpleVar\n");
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
  // fprintf(stderr, "FieldVar\n");
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
  // fprintf(stderr, "SubscriptVar\n");
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
  // fprintf(stderr, "VarExp\n");
  assert(typeid(*var_) == typeid(absyn::SimpleVar) ||
         typeid(*var_) == typeid(absyn::FieldVar) ||
         typeid(*var_) == typeid(absyn::SubscriptVar));
  return var_->Translate(venv, tenv, level, label, errormsg);
}

tr::ExpAndTy *NilExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  // fprintf(stderr, "NilExp\n");
  return new tr::ExpAndTy(tr::NilExp(), type::NilTy::Instance());
}

tr::ExpAndTy *IntExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  // fprintf(stderr, "IntExp\n");
  return new tr::ExpAndTy(tr::IntExp(val_), type::IntTy::Instance());
}

tr::ExpAndTy *StringExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,
                                   err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  // fprintf(stderr, "StringExp\n");
  return new tr::ExpAndTy(tr::StringExp(str_), type::StringTy::Instance());
}

tr::ExpAndTy *CallExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                 tr::Level *level, temp::Label *label,
                                 err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  // fprintf(stderr, "CallExp\n");
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
                  tr::StaticLink(level, fun_entry->level_->parent_), args_trexp,
                  level),
      fun_entry->result_);
}

tr::ExpAndTy *OpExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                               tr::Level *level, temp::Label *label,
                               err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  // fprintf(stderr, "OpExp\n");
  tr::ExpAndTy *left = left_->Translate(venv, tenv, level, label, errormsg);
  tr::ExpAndTy *right = right_->Translate(venv, tenv, level, label, errormsg);
  if (left->ty_->IsSameType(type::StringTy::Instance()) &&
      right->ty_->IsSameType(type::StringTy::Instance())) {
    assert(oper_ == absyn::Oper::EQ_OP || oper_ == absyn::Oper::NEQ_OP);
    return new tr::ExpAndTy(tr::OpExp(oper_, left->exp_, right->exp_, true),
                            type::IntTy::Instance());
  } else {
    return new tr::ExpAndTy(tr::OpExp(oper_, left->exp_, right->exp_, false),
                            type::IntTy::Instance());
  }
}

tr::ExpAndTy *RecordExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,
                                   err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  // fprintf(stderr, "RecordExp\n");
  type::Ty *ty = tenv->Look(typ_)->ActualTy();
  assert(ty != nullptr);
  std::vector<tr::Exp *> exp_list;
  for (absyn::EField *efield : fields_->GetList()) {
    tr::ExpAndTy *expAndTy =
        efield->exp_->Translate(venv, tenv, level, label, errormsg);
    exp_list.push_back(expAndTy->exp_);
  }
  return new tr::ExpAndTy(tr::RecordExp(exp_list), ty);
}

tr::ExpAndTy *SeqExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  // fprintf(stderr, "SeqExp\n");
  if (seq_ == nullptr) {
    return new tr::ExpAndTy(nullptr, type::VoidTy::Instance());
  }
  std::list<tr::Exp *> exp_list;
  type::Ty *ty = type::VoidTy::Instance();
  for (absyn::Exp *exp : seq_->GetList()) {
    tr::ExpAndTy *expAndTy = exp->Translate(venv, tenv, level, label, errormsg);
    exp_list.push_back(expAndTy->exp_);
    ty = expAndTy->ty_;
  }
  return new tr::ExpAndTy(tr::SeqExp(exp_list), ty);
}

tr::ExpAndTy *AssignExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,
                                   err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  // fprintf(stderr, "AssignExp\n");
  tr::ExpAndTy *var_info = var_->Translate(venv, tenv, level, label, errormsg);
  tr::ExpAndTy *exp_info = exp_->Translate(venv, tenv, level, label, errormsg);
  return new tr::ExpAndTy(tr::AssignExp(var_info->exp_, exp_info->exp_),
                          type::VoidTy::Instance());
}

tr::ExpAndTy *IfExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                               tr::Level *level, temp::Label *label,
                               err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  // fprintf(stderr, "IfExp\n");
  tr::ExpAndTy *test_info =
      test_->Translate(venv, tenv, level, label, errormsg);
  tr::ExpAndTy *then_info =
      then_->Translate(venv, tenv, level, label, errormsg);
  tr::ExpAndTy *elsee_info = nullptr;
  if (elsee_ != nullptr) {
    elsee_info = elsee_->Translate(venv, tenv, level, label, errormsg);
    return new tr::ExpAndTy(
        tr::IfExp(test_info->exp_, then_info->exp_, elsee_info->exp_, errormsg),
        then_info->ty_);
  } else {
    return new tr::ExpAndTy(
        tr::IfExp(test_info->exp_, then_info->exp_, nullptr, errormsg),
        then_info->ty_);
  }
}

tr::ExpAndTy *WhileExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                  tr::Level *level, temp::Label *label,
                                  err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  // fprintf(stderr, "WhileExp\n");
  temp::Label *done_label = temp::LabelFactory::NewLabel();
  tr::ExpAndTy *test_info =
      test_->Translate(venv, tenv, level, label, errormsg);
  tr::ExpAndTy *body_info =
      body_->Translate(venv, tenv, level, done_label, errormsg);
  return new tr::ExpAndTy(
      tr::WhileExp(test_info->exp_, body_info->exp_, done_label, errormsg),
      body_info->ty_);
}

tr::ExpAndTy *ForExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  // fprintf(stderr, "ForExp\n");
  venv->BeginScope();
  temp::Label *done_label = temp::LabelFactory::NewLabel();
  tr::ExpAndTy *lo_info = lo_->Translate(venv, tenv, level, label, errormsg);
  tr::ExpAndTy *hi_info = hi_->Translate(venv, tenv, level, label, errormsg);
  tr::Access *access = tr::Access::AllocLocal(level, this->escape_);
  venv->Enter(var_, new env::VarEntry(access, lo_info->ty_, true));
  tr::ExpAndTy *body_info =
      body_->Translate(venv, tenv, level, done_label, errormsg);
  venv->EndScope();
  tr::ExpAndTy *ret =
      new tr::ExpAndTy(tr::ForExp(access->access_, lo_info->exp_, hi_info->exp_,
                                  body_info->exp_, done_label),
                       type::VoidTy::Instance());
  return ret;
}

tr::ExpAndTy *BreakExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                  tr::Level *level, temp::Label *label,
                                  err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  // fprintf(stderr, "BreakExp\n");
  return new tr::ExpAndTy(tr::BreakExp(label), type::VoidTy::Instance());
}

tr::ExpAndTy *LetExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  // fprintf(stderr, "LetExp\n");
  std::list<tr::Exp *> dec_list;
  for (absyn::Dec *dec : decs_->GetList()) {
    dec_list.push_back(dec->Translate(venv, tenv, level, label, errormsg));
  }
  tr::ExpAndTy *body_info =
      body_->Translate(venv, tenv, level, label, errormsg);
  tr::ExpAndTy *ret =
      new tr::ExpAndTy(tr::LetExp(dec_list, body_info->exp_), body_info->ty_);
  return ret;
}

tr::ExpAndTy *ArrayExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                  tr::Level *level, temp::Label *label,
                                  err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  // fprintf(stderr, "ArrayExp\n");
  tr::ExpAndTy *size_info =
      size_->Translate(venv, tenv, level, label, errormsg);
  tr::ExpAndTy *init_info =
      init_->Translate(venv, tenv, level, label, errormsg);
  tr::ExpAndTy *ret = new tr::ExpAndTy(
      tr::ArrayExp(size_info->exp_, init_info->exp_), tenv->Look(typ_));
  return ret;
}

tr::ExpAndTy *VoidExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                 tr::Level *level, temp::Label *label,
                                 err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  // fprintf(stderr, "VoidExp\n");
  return new tr::ExpAndTy(nullptr, type::VoidTy::Instance());
}

tr::Exp *FunctionDec::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  // fprintf(stderr, "FunctionDec\n");
  for (absyn::FunDec *function : functions_->GetList()) {
    // 参数类型
    type::TyList *tyList = function->params_->MakeFormalTyList(tenv, errormsg);
    // 函数名
    temp::Label *function_label =
        temp::LabelFactory::NamedLabel(function->name_->Name());
    // escape?
    std::list<bool> formals;
    for (absyn::Field *field : function->params_->GetList()) {
      formals.push_back(field->escape_);
    }
    // 新的level
    tr::Level *new_level = tr::Level::NewLevel(level, function_label, formals);
    // 返回类型
    if (function->result_ != nullptr) {
      type::Ty *result_ty = tenv->Look(function->result_);
      venv->Enter(function->name_, new env::FunEntry(new_level, function_label,
                                                     tyList, result_ty));
    } else {
      venv->Enter(function->name_,
                  new env::FunEntry(new_level, function_label, tyList,
                                    type::VoidTy::Instance()));
    }
  }
  for (absyn::FunDec *function : functions_->GetList()) {
    // 参数类型
    type::TyList *tyList = function->params_->MakeFormalTyList(tenv, errormsg);
    venv->BeginScope();
    env::FunEntry *entry =
        static_cast<env::FunEntry *>(venv->Look(function->name_));
    std::list<tr::Access *> access_list = entry->level_->GetAccessList();
    auto formal_ty_it = tyList->GetList().begin();
    auto param_it = function->params_->GetList().begin();
    auto access_list_it = ++access_list.begin(); //跳过static link
    for (; param_it != function->params_->GetList().end();
         formal_ty_it++, param_it++, access_list_it++) {
      venv->Enter((*param_it)->name_,
                  new env::VarEntry(*access_list_it, *formal_ty_it));
    }
    tr::ExpAndTy *body_info =
        function->body_->Translate(venv, tenv, entry->level_, label, errormsg);
    venv->EndScope();
    tr::FunctionDec(body_info->exp_, entry->level_);
  }
  return tr::NilExp();
}

tr::Exp *VarDec::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                           tr::Level *level, temp::Label *label,
                           err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  // fprintf(stderr, "VarDec\n");
  tr::ExpAndTy *init_info =
      init_->Translate(venv, tenv, level, label, errormsg);
  type::Ty *init_ty = init_info->ty_->ActualTy();
  if (this->typ_ != nullptr) {
    init_ty = tenv->Look(this->typ_)->ActualTy();
  }
  tr::Access *access = tr::Access::AllocLocal(level, this->escape_);
  venv->Enter(this->var_, new env::VarEntry(access, init_ty));
  return new tr::NxExp(new tree::MoveStm(tr::SimpleVar(access, level)->UnEx(),
                                         init_info->exp_->UnEx()));
}

// 后面的内容，Translate没什么要做的，这里直接把semant.cc的大致抄过来了
tr::Exp *TypeDec::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                            tr::Level *level, temp::Label *label,
                            err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  // fprintf(stderr, "TypeDec\n");
  for (absyn::NameAndTy *name_and_type : types_->GetList()) {
    for (absyn::NameAndTy *check_name : types_->GetList()) {
      if (check_name == name_and_type)
        continue;
      if (check_name->name_->Name() == name_and_type->name_->Name()) {
        errormsg->Error(pos_, "two types have the same name");
        // return;
      }
    }
    tenv->Enter(name_and_type->name_,
                new type::NameTy(name_and_type->name_, nullptr));
  }
  for (absyn::NameAndTy *name_and_type : types_->GetList()) {
    type::Ty *ty = name_and_type->ty_->Translate(tenv, errormsg);
    type::NameTy *name_ty =
        static_cast<type::NameTy *>(tenv->Look(name_and_type->name_));
    name_ty->ty_ = ty;
  }
  for (absyn::NameAndTy *name_and_type : types_->GetList()) {
    type::Ty *ty = tenv->Look(name_and_type->name_);
    std::vector<sym::Symbol *> trace;
    while (typeid(*ty) == typeid(type::NameTy)) {
      type::NameTy *name_ty = static_cast<type::NameTy *>(ty);
      for (sym::Symbol *symbol : trace) {
        if (symbol->Name() == name_ty->sym_->Name()) {
          errormsg->Error(pos_, "illegal type cycle");
          // return;
        }
      }
      trace.push_back(name_ty->sym_);
      if (name_ty->ty_ == nullptr) {
        errormsg->Error(pos_, "undefined type %s",
                        name_ty->sym_->Name().c_str());
        // return;
      }
      ty = name_ty->ty_;
    }
  }
  return tr::NilExp();
}

type::Ty *NameTy::Translate(env::TEnvPtr tenv, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  // fprintf(stderr, "NameTy\n");
  return new type::NameTy(name_, tenv->Look(name_));
}

type::Ty *RecordTy::Translate(env::TEnvPtr tenv,
                              err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  // fprintf(stderr, "RecordTy\n");
  type::FieldList *fields = record_->MakeFieldList(tenv, errormsg);
  return new type::RecordTy(fields);
}

type::Ty *ArrayTy::Translate(env::TEnvPtr tenv, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  // fprintf(stderr, "ArrayTy\n");
  type::Ty *ty = tenv->Look(array_);
  if (ty == nullptr) {
    errormsg->Error(pos_, "undefined type %s", array_->Name().c_str());
    return type::IntTy::Instance();
  }
  return new type::ArrayTy(ty);
}

} // namespace absyn
