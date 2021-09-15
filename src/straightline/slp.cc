#include "straightline/slp.h"

#include <iostream>

namespace A {

// STATEMENT---------------------------------------------------------------------------------------
int A::CompoundStm::MaxArgs() const {
  // TODO: put your code here (lab1).
  int args1 = stm1->MaxArgs();
  int args2 = stm2->MaxArgs();
  return args1 > args2 ? args1 : args2;
}

Table *A::CompoundStm::Interp(Table *t) const {
  // TODO: put your code here (lab1).
  Table *table1 = stm1->Interp(t);
  return stm2->Interp(table1);
}

int A::AssignStm::MaxArgs() const {
  // TODO: put your code here (lab1).
  return exp->MaxArgs();
}

Table *A::AssignStm::Interp(Table *t) const {
  // TODO: put your code here (lab1).
  IntAndTable *result = exp->Interp(t);
  Table *result_table = result->t->Update(id, result->i);
  return result_table;
}

int A::PrintStm::MaxArgs() const {
  // TODO: put your code here (lab1).
  int num_exps = exps->NumExps();
  int explist_max_args = exps->MaxArgs();
  return num_exps > explist_max_args ? num_exps : explist_max_args;
}

Table *A::PrintStm::Interp(Table *t) const {
  // TODO: put your code here (lab1).
  ExpList *p = exps;
  IntAndTable *result = new IntAndTable(0, t);
  while (p) {
    result = p->Interp(result->t);
    std::cout << result->i << ' ';
    p = p->GetTail();
  }
  std::cout << std::endl;
  return result->t;
}

// EXPRESSION---------------------------------------------------------------------------------------
int A::IdExp::MaxArgs() const { return 0; }

IntAndTable *A::IdExp::Interp(Table *t) const {
  IntAndTable *result = new IntAndTable(t->Lookup(id), t);
  return result;
}

int A::NumExp::MaxArgs() const { return 0; }

IntAndTable *A::NumExp::Interp(Table *t) const {
  IntAndTable *result = new IntAndTable(num, t);
  return result;
}

int A::OpExp::MaxArgs() const {
  int args1 = left->MaxArgs();
  int args2 = right->MaxArgs();
  return args1 > args2 ? args1 : args2;
}

IntAndTable *A::OpExp::Interp(Table *t) const {
  IntAndTable *result = nullptr;
  IntAndTable *left_result = left->Interp(t),
              *right_result = right->Interp(left_result->t);
  switch (oper) {
  case PLUS:
    result = new IntAndTable(left_result->i + right_result->i, right_result->t);
    break;
  case MINUS:
    result = new IntAndTable(left_result->i - right_result->i, right_result->t);
    break;
  case TIMES:
    result = new IntAndTable(left_result->i * right_result->i, right_result->t);
    break;
  case DIV:
    assert(right_result->i != 0);
    result = new IntAndTable(left_result->i / right_result->i, right_result->t);
    break;
  default:
    break;
  }
  assert(result != nullptr);
  return result;
}

int A::EseqExp::MaxArgs() const {
  int stm_args = stm->MaxArgs();
  int exp_args = exp->MaxArgs();
  return stm_args > exp_args ? stm_args : exp_args;
}

IntAndTable *A::EseqExp::Interp(Table *t) const {
  Table *stm_result = stm->Interp(t);
  IntAndTable *result = exp->Interp(stm_result);
  return result;
}

// EXPRESSION
// LIST--------------------------------------------------------------------------------------------
int A::PairExpList::MaxArgs() const {
  int exp_args = exp->MaxArgs();
  int tail_args = tail->MaxArgs();
  return exp_args > tail_args ? exp_args : tail_args;
}

int A::PairExpList::NumExps() const { return tail->NumExps() + 1; }

ExpList *A::PairExpList::GetTail() const { return tail; }

IntAndTable *A::PairExpList::Interp(Table *t) const { return exp->Interp(t); }

int A::LastExpList::MaxArgs() const { return exp->MaxArgs(); }

int A::LastExpList::NumExps() const { return 1; }

ExpList *A::LastExpList::GetTail() const { return nullptr; }

IntAndTable *A::LastExpList::Interp(Table *t) const { return exp->Interp(t); }

// TABLE---------------------------------------------------------------------------------------
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
