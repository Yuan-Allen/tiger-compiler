#include "straightline/slp.h"

#include <iostream>

namespace A {
int A::CompoundStm::MaxArgs() const {
  // TODO: put your code here (lab1).
  int args1 = stm1->MaxArgs();
  int args2 = stm2->MaxArgs();
  return args1 > args2 ? args1 : args2;
}

Table *A::CompoundStm::Interp(Table *t) const {
  // TODO: put your code here (lab1).
}

int A::AssignStm::MaxArgs() const {
  // TODO: put your code here (lab1).
  return exp->MaxArgs();
}

Table *A::AssignStm::Interp(Table *t) const {
  // TODO: put your code here (lab1).
}

int A::PrintStm::MaxArgs() const {
  // TODO: put your code here (lab1).
  int num_exps = exps->NumExps();
  int explist_max_args = exps->MaxArgs();
  return num_exps > explist_max_args ? num_exps : explist_max_args;
}

Table *A::PrintStm::Interp(Table *t) const {
  // TODO: put your code here (lab1).
}

int A::IdExp::MaxArgs() const { return 0; }

int A::NumExp::MaxArgs() const { return 0; }

int A::OpExp::MaxArgs() const {
  int args1 = left->MaxArgs();
  int args2 = right->MaxArgs();
  return args1 > args2 ? args1 : args2;
}

int A::EseqExp::MaxArgs() const {
  int stm_args = stm->MaxArgs();
  int exp_args = exp->MaxArgs();
  return stm_args > exp_args ? stm_args : exp_args;
}

int A::PairExpList::MaxArgs() const {
  int exp_args = exp->MaxArgs();
  int tail_args = tail->MaxArgs();
  return exp_args > tail_args ? exp_args : tail_args;
}

int A::PairExpList::NumExps() const { return tail->NumExps() + 1; }

int A::LastExpList::MaxArgs() const { return exp->MaxArgs(); }

int A::LastExpList::NumExps() const { return 1; }

int Table::Lookup(const std::string &key) const {
  if (id == key) {
    return value;
  } else if (tail != nullptr) {
    return tail->Lookup(key);
  } else {
    assert(false);
  }
}

Table *Table::Update(const std::string &key, int val) const {
  return new Table(key, val, this);
}
} // namespace A
