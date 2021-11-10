#include "tiger/semant/semant.h"
#include "tiger/absyn/absyn.h"

namespace absyn {

void AbsynTree::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                           err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  root_->SemAnalyze(venv, tenv, 0, errormsg);
}

type::Ty *SimpleVar::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  env::EnvEntry *entry = venv->Look(sym_);
  if (entry && typeid(*entry) == typeid(env::VarEntry)) {
    return (static_cast<env::VarEntry *>(entry))->ty_->ActualTy();
  } else {
    errormsg->Error(pos_, "undefined variable %s", sym_->Name().data());
  }
  return type::IntTy::Instance();
}

type::Ty *FieldVar::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                               int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  type::Ty *ty = var_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  if (typeid(*ty) != typeid(type::RecordTy)) {
    errormsg->Error(pos_, "not a record type");
    return type::IntTy::Instance();
  }
  type::FieldList *fields = static_cast<type::RecordTy *>(ty)->fields_;
  for (type::Field *field : fields->GetList()) {
    if (field->name_ == sym_) {
      return field->ty_;
    }
  }
  errormsg->Error(pos_, "field %s doesn't exist", sym_->Name().c_str());
  return type::IntTy::Instance();
}

type::Ty *SubscriptVar::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   int labelcount,
                                   err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  type::Ty *var_ty =
      var_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  if (typeid(*var_ty) != typeid(type::ArrayTy)) {
    errormsg->Error(pos_, "array type required");
    return type::IntTy::Instance();
  }
  type::Ty *exp_ty =
      subscript_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  if (typeid(*exp_ty) != typeid(type::IntTy)) {
    errormsg->Error(pos_, "the type of subscript must be int");
    return type::IntTy::Instance();
  }
  return (static_cast<type::ArrayTy *>(var_ty)->ty_)->ActualTy();
}

type::Ty *VarExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  if (typeid(*var_) == typeid(absyn::SimpleVar) ||
      typeid(*var_) == typeid(absyn::FieldVar) ||
      typeid(*var_) == typeid(absyn::SubscriptVar)) {
    return var_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  } else {
    errormsg->Error(pos_, "wrong type of var_");
    return type::IntTy::Instance();
  }
}

type::Ty *NilExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  return type::NilTy::Instance();
}

type::Ty *IntExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  return type::IntTy::Instance();
}

type::Ty *StringExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  return type::StringTy::Instance();
}

type::Ty *CallExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                              int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  env::EnvEntry *entry = venv->Look(func_);
  if (entry == nullptr) {
    errormsg->Error(pos_, "undefined function %s", func_->Name().c_str());
    return type::IntTy::Instance();
  }
  if (typeid(*entry) != typeid(env::FunEntry)) {
    errormsg->Error(pos_, "%s is not a function", func_->Name().c_str());
    return type::IntTy::Instance();
  }
  std::list<type::Ty *> formals =
      static_cast<env::FunEntry *>(entry)->formals_->GetList();
  std::list<absyn::Exp *> exp_list = args_->GetList();

  std::list<type::Ty *>::iterator formals_it = formals.begin();
  std::list<absyn::Exp *>::iterator exp_list_it = exp_list.begin();
  while (formals_it != formals.end() && exp_list_it != exp_list.end()) {
    if (!(*formals_it)
             ->IsSameType((*exp_list_it)
                              ->SemAnalyze(venv, tenv, labelcount, errormsg))) {
      errormsg->Error(pos_, "para type mismatch");
    }
    formals_it++;
    exp_list_it++;
  }
  if (formals_it != formals.end()) {
    errormsg->Error(pos_, "too few params in function %s",
                    func_->Name().c_str());
    return type::IntTy::Instance();
  }
  if (exp_list_it != exp_list.end()) {
    errormsg->Error(pos_, "too many params in function %s",
                    func_->Name().c_str());
    return type::IntTy::Instance();
  }

  return static_cast<env::FunEntry *>(entry)->result_->ActualTy();
}

type::Ty *OpExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                            int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  type::Ty *left_ty =
      left_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  type::Ty *right_ty =
      right_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  if (oper_ == absyn::Oper::PLUS_OP || oper_ == absyn::Oper::MINUS_OP ||
      oper_ == absyn::Oper::TIMES_OP || oper_ == absyn::Oper::DIVIDE_OP) {
    if (typeid(*left_ty) != typeid(type::IntTy)) {
      errormsg->Error(left_->pos_, "integer required");
    }
    if (typeid(*right_ty) != typeid(type::IntTy)) {
      errormsg->Error(right_->pos_, "integer required");
    }
  } else {
    if (!left_ty->IsSameType(right_ty)) {
      errormsg->Error(pos_, "same type required");
    }
  }
  return type::IntTy::Instance();
}

type::Ty *RecordExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  type::Ty *ty = tenv->Look(typ_);
  if (ty == nullptr) {
    errormsg->Error(pos_, "undefined type %s", typ_->Name().c_str());
    return type::IntTy::Instance();
  }
  ty = ty->ActualTy();
  if (typeid(*ty) != typeid(type::RecordTy)) {
    errormsg->Error(pos_, "%s is not a record type", typ_->Name().c_str());
    return type::IntTy::Instance();
  }
  std::list<type::Field *> field_list =
      static_cast<type::RecordTy *>(ty)->fields_->GetList();
  std::list<absyn::EField *> efield_list = fields_->GetList();
  std::list<type::Field *>::iterator field_list_it = field_list.begin();
  std::list<absyn::EField *>::iterator efield_list_it = efield_list.begin();
  while (true) {
    if (field_list_it == field_list.end() ||
        efield_list_it == efield_list.end())
      break;
    if ((*field_list_it)->name_->Name() != (*efield_list_it)->name_->Name()) {
      errormsg->Error(pos_, "expected %s, got %s",
                      (*field_list_it)->name_->Name(),
                      (*efield_list_it)->name_->Name());
      return type::IntTy::Instance();
    } else if (!(*field_list_it)
                    ->ty_->IsSameType((*efield_list_it)
                                          ->exp_->SemAnalyze(venv, tenv,
                                                             labelcount,
                                                             errormsg))) {
      errormsg->Error(pos_, "type mismatch");
      return type::IntTy::Instance();
    }
    field_list_it++;
    efield_list_it++;
  }
  if (field_list_it != field_list.end()) {
    errormsg->Error(pos_, "too few arguments");
    return type::IntTy::Instance();
  } else if (efield_list_it != efield_list.end()) {
    errormsg->Error(pos_, "too many arguments");
    return type::IntTy::Instance();
  }
  return ty;
}

type::Ty *SeqExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  type::Ty *ty = nullptr;
  std::list<absyn::Exp *> exp_list = seq_->GetList();
  for (std::list<absyn::Exp *>::iterator it = exp_list.begin();
       it != exp_list.end(); ++it)
    ty = (*it)->SemAnalyze(venv, tenv, labelcount, errormsg);
  return ty;
}

type::Ty *AssignExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  type::Ty *var_ty = var_->SemAnalyze(venv, tenv, labelcount, errormsg);
  type::Ty *exp_ty = exp_->SemAnalyze(venv, tenv, labelcount, errormsg);
  if (!var_ty->IsSameType(exp_ty)) {
    errormsg->Error(pos_, "unmatched assign exp");
    return type::IntTy::Instance();
  }
  if (typeid(*var_) == typeid(absyn::SimpleVar)) {
    env::EnvEntry *entry =
        venv->Look(static_cast<absyn::SimpleVar *>(var_)->sym_);
    if (typeid(*entry) != typeid(env::VarEntry)) {
      errormsg->Error(
          pos_, "%s is not a varible",
          static_cast<absyn::SimpleVar *>(var_)->sym_->Name().c_str());
      return type::IntTy::Instance();
    }
    if (static_cast<env::VarEntry *>(entry)->readonly_) {
      errormsg->Error(pos_, "loop variable can't be assigned");
      return type::IntTy::Instance();
    }
  }
  return type::VoidTy::Instance();
}

type::Ty *IfExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                            int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  type::Ty *test_ty = test_->SemAnalyze(venv, tenv, labelcount, errormsg);
  if (!test_ty->IsSameType(type::IntTy::Instance())) {
    errormsg->Error(pos_, "the condition type of if is not integer");
    return type::IntTy::Instance();
  }
  type::Ty *then_ty = then_->SemAnalyze(venv, tenv, labelcount, errormsg);
  if (elsee_ == nullptr) {
    if (!then_ty->IsSameType(type::VoidTy::Instance())) {
      errormsg->Error(pos_, "if-then exp's body must produce no value");
      return type::IntTy::Instance();
    } else
      return type::VoidTy::Instance();
  } else {
    type::Ty *else_ty = elsee_->SemAnalyze(venv, tenv, labelcount, errormsg);
    if (!then_ty->IsSameType(else_ty)) {
      errormsg->Error(pos_, "then exp and else exp type mismatch");
      return type::IntTy::Instance();
    }
    return then_ty;
  }
}

type::Ty *WhileExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                               int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  type::Ty *test_ty = test_->SemAnalyze(venv, tenv, labelcount, errormsg);
  if (!test_ty->IsSameType(type::IntTy::Instance())) {
    errormsg->Error(pos_, "while condition type is not integer");
    return type::IntTy::Instance();
  }
  type::Ty *body_ty = body_->SemAnalyze(venv, tenv, labelcount + 1, errormsg);
  if (!body_ty->IsSameType(type::VoidTy::Instance())) {
    errormsg->Error(pos_, "while body must produce no value");
    return type::IntTy::Instance();
  }
  return type::VoidTy::Instance();
}

type::Ty *ForExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  type::Ty *lo_ty = lo_->SemAnalyze(venv, tenv, labelcount, errormsg);
  type::Ty *hi_ty = hi_->SemAnalyze(venv, tenv, labelcount, errormsg);
  if (!lo_ty->IsSameType(type::IntTy::Instance()) ||
      !hi_ty->IsSameType(type::IntTy::Instance())) {
    errormsg->Error(pos_, "for exp's range type is not integer");
  }
  venv->BeginScope();
  venv->Enter(var_, new env::VarEntry(type::IntTy::Instance(), true));
  type::Ty *body_ty = body_->SemAnalyze(venv, tenv, labelcount + 1, errormsg);
  if (!body_ty->IsSameType(type::VoidTy::Instance())) {
    errormsg->Error(pos_, "the body of for loop should not produce value");
    venv->EndScope();
    return type::IntTy::Instance();
  }
  venv->EndScope();
  return type::VoidTy::Instance();
}

type::Ty *BreakExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                               int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  if (labelcount == 0) {
    errormsg->Error(pos_, "break is not inside any loop");
    return type::IntTy::Instance();
  }
  return type::VoidTy::Instance();
}

type::Ty *LetExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  venv->BeginScope();
  tenv->BeginScope();
  for (Dec *dec : decs_->GetList())
    dec->SemAnalyze(venv, tenv, labelcount, errormsg);
  type::Ty *result;

  if (!body_)
    result = type::VoidTy::Instance();
  else
    result = body_->SemAnalyze(venv, tenv, labelcount, errormsg);

  tenv->EndScope();
  venv->EndScope();
  return result;
}

type::Ty *ArrayExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                               int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  type::Ty *ty = tenv->Look(typ_)->ActualTy();
  if (ty == nullptr) {
    errormsg->Error(pos_, "undefined type %s", typ_->Name().c_str());
    return type::IntTy::Instance();
  }
  if (typeid(*ty) != typeid(type::ArrayTy)) {
    errormsg->Error(pos_, "not an array type");
    return type::IntTy::Instance();
  }
  type::Ty *size_ty = size_->SemAnalyze(venv, tenv, labelcount, errormsg);
  if (!size_ty->IsSameType(type::IntTy::Instance())) {
    errormsg->Error(size_->pos_, "the size of array is not integer");
    return type::IntTy::Instance();
  }
  type::Ty *init_ty = init_->SemAnalyze(venv, tenv, labelcount, errormsg);
  if (!size_ty->IsSameType(init_ty)) {
    errormsg->Error(init_->pos_, "type mismatch");
    return type::IntTy::Instance();
  }
  return ty;
}

type::Ty *VoidExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                              int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  return type::VoidTy::Instance();
}

void FunctionDec::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  for (absyn::FunDec *function : functions_->GetList()) {
    for (absyn::FunDec *check_name : functions_->GetList()) {
      if (check_name == function)
        continue;
      if (check_name->name_->Name() == function->name_->Name()) {
        errormsg->Error(check_name->pos_, "two functions have the same name");
        return;
      }
    }
    type::Ty *result_ty = function->result_ == nullptr
                              ? type::VoidTy::Instance()
                              : tenv->Look(function->result_);
    type::TyList *formals = function->params_->MakeFormalTyList(tenv, errormsg);
    venv->Enter(function->name_, new env::FunEntry(formals, result_ty));
  }

  for (absyn::FunDec *function : functions_->GetList()) {
    venv->BeginScope();
    env::EnvEntry *entry = venv->Look(function->name_);
    if (entry == nullptr) {
      errormsg->Error(function->pos_, "%s is undefined",
                      function->name_->Name().c_str());
      return;
    }
    if (typeid(*entry) != typeid(env::FunEntry)) {
      errormsg->Error(function->pos_, "%s is not a function",
                      function->name_->Name().c_str());
      return;
    }
    type::TyList *formals = static_cast<env::FunEntry *>(entry)->formals_;
    auto formal_it = formals->GetList().begin();
    auto param_it = function->params_->GetList().begin();
    for (; param_it != function->params_->GetList().end();
         formal_it++, param_it++)
      venv->Enter((*param_it)->name_, new env::VarEntry(*formal_it));
    type::Ty *body_ty =
        function->body_->SemAnalyze(venv, tenv, labelcount, errormsg);
    type::Ty *result_ty = static_cast<env::FunEntry *>(entry)->result_;
    if (!result_ty->IsSameType(body_ty)) {
      if (result_ty->IsSameType(type::VoidTy::Instance()))
        errormsg->Error(pos_, "procedure returns value");
      else
        errormsg->Error(function->pos_, "return type mismtach");
    }
    venv->EndScope();
  }
}

void VarDec::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv, int labelcount,
                        err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  type::Ty *exp_ty = init_->SemAnalyze(venv, tenv, labelcount, errormsg);
  if (typ_ == nullptr) {
    if (typeid(*exp_ty) == typeid(type::NilTy)) {
      errormsg->Error(init_->pos_,
                      "init should not be nil without type specified");
      return;
    }
    venv->Enter(var_, new env::VarEntry(exp_ty));
  } else {
    type::Ty *ty = tenv->Look(typ_)->ActualTy();
    if (ty == nullptr) {
      errormsg->Error(pos_, "undefined type %s", typ_->Name().c_str());
      return;
    }
    if (!ty->IsSameType(exp_ty)) {
      errormsg->Error(init_->pos_, "type mismatch");
      return;
    }
    venv->Enter(var_, new env::VarEntry(ty));
  }
}

void TypeDec::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv, int labelcount,
                         err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  for (absyn::NameAndTy *name_and_type : types_->GetList()) {
    for (absyn::NameAndTy *check_name : types_->GetList()) {
      if (check_name == name_and_type)
        continue;
      if (check_name->name_->Name() == name_and_type->name_->Name()) {
        errormsg->Error(pos_, "two types have the same name");
        return;
      }
    }
    tenv->Enter(name_and_type->name_,
                new type::NameTy(name_and_type->name_, nullptr));
  }
  for (absyn::NameAndTy *name_and_type : types_->GetList()) {
    type::Ty *ty = name_and_type->ty_->SemAnalyze(tenv, errormsg);
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
          return;
        }
      }
      trace.push_back(name_ty->sym_);
      if (name_ty->ty_ == nullptr) {
        errormsg->Error(pos_, "undefined type %s",
                        name_ty->sym_->Name().c_str());
        return;
      }
      ty = name_ty->ty_;
    }
  }
}

type::Ty *NameTy::SemAnalyze(env::TEnvPtr tenv, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  type::Ty *ty = tenv->Look(name_);
  if (ty == nullptr) {
    errormsg->Error(pos_, "undefined type %s", name_->Name().c_str());
    return type::IntTy::Instance();
  }
  return ty;
}

type::Ty *RecordTy::SemAnalyze(env::TEnvPtr tenv,
                               err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  type::FieldList *fields = record_->MakeFieldList(tenv, errormsg);
  return new type::RecordTy(fields);
}

type::Ty *ArrayTy::SemAnalyze(env::TEnvPtr tenv,
                              err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  type::Ty *ty = tenv->Look(array_);
  if (ty == nullptr) {
    errormsg->Error(pos_, "undefined type %s", array_->Name().c_str());
    return type::IntTy::Instance();
  }
  return new type::ArrayTy(ty);
}

} // namespace absyn

namespace sem {

void ProgSem::SemAnalyze() {
  FillBaseVEnv();
  FillBaseTEnv();
  absyn_tree_->SemAnalyze(venv_.get(), tenv_.get(), errormsg_.get());
}

} // namespace tr
