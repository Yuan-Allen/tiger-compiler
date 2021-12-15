#include "tiger/liveness/liveness.h"

extern frame::RegManager *reg_manager;

namespace live {

bool MoveList::Contain(INodePtr src, INodePtr dst) {
  return std::any_of(move_list_.cbegin(), move_list_.cend(),
                     [src, dst](std::pair<INodePtr, INodePtr> move) {
                       return move.first == src && move.second == dst;
                     });
}

void MoveList::Delete(INodePtr src, INodePtr dst) {
  assert(src && dst);
  auto move_it = move_list_.begin();
  for (; move_it != move_list_.end(); move_it++) {
    if (move_it->first == src && move_it->second == dst) {
      break;
    }
  }
  move_list_.erase(move_it);
}

MoveList *MoveList::Union(MoveList *list) {
  auto *res = new MoveList();
  for (auto move : move_list_) {
    res->move_list_.push_back(move);
  }
  for (auto move : list->GetList()) {
    if (!res->Contain(move.first, move.second))
      res->move_list_.push_back(move);
  }
  return res;
}

MoveList *MoveList::Intersect(MoveList *list) {
  auto *res = new MoveList();
  for (auto move : list->GetList()) {
    if (Contain(move.first, move.second))
      res->move_list_.push_back(move);
  }
  return res;
}

void LiveGraphFactory::LiveMap() {
  /* TODO: Put your lab6 code here */
  // fprintf(stderr, "LiveMap\n");
  for (fg::FNodePtr node : flowgraph_->Nodes()->GetList()) {
    in_->Enter(node, new temp::TempList());
    out_->Enter(node, new temp::TempList());
  }

  bool flag = false;
  while (!flag) {
    flag = true;
    for (fg::FNodePtr node : flowgraph_->Nodes()->GetList()) {
      temp::TempList *old_in = in_->Look(node);
      temp::TempList *old_out = out_->Look(node);
      for (fg::FNodePtr succ : node->Succ()->GetList()) {
        out_->Set(node, Union(out_->Look(node), in_->Look(succ)));
      }
      in_->Set(node, Union(node->NodeInfo()->Use(),
                           Diff(out_->Look(node), node->NodeInfo()->Def())));
      if (!Equal(old_in, in_->Look(node)) ||
          !Equal(old_out, out_->Look(node))) {
        flag = false;
      }
    }
  }
}

void LiveGraphFactory::InterfGraph() {
  /* TODO: Put your lab6 code here */
  // fprintf(stderr, "InterfGraph\n");
  live_graph_.interf_graph = new IGraph();
  live_graph_.moves = new MoveList();
  // no rsp
  temp::TempList *registers = reg_manager->Registers();
  for (temp::Temp *reg : registers->GetList()) {
    INodePtr node = live_graph_.interf_graph->NewNode(reg);
    temp_node_map_->Enter(reg, node);
  }

  int reg_size = static_cast<int>(registers->GetList().size());
  assert(reg_size == 15);
  for (int i = 0; i < reg_size; ++i) {
    for (int j = i + 1; j < reg_size; ++j) {
      live_graph_.interf_graph->AddEdge(
          temp_node_map_->Look(registers->NthTemp(i)),
          temp_node_map_->Look(registers->NthTemp(j)));
      live_graph_.interf_graph->AddEdge(
          temp_node_map_->Look(registers->NthTemp(j)),
          temp_node_map_->Look(registers->NthTemp(i)));
    }
  }

  // 先添加node
  for (fg::FNodePtr node : flowgraph_->Nodes()->GetList()) {
    for (temp::Temp *t :
         Union(out_->Look(node), node->NodeInfo()->Def())->GetList()) {
      if (t == reg_manager->StackPointer())
        continue;
      if (temp_node_map_->Look(t) == nullptr) {
        INodePtr inode = live_graph_.interf_graph->NewNode(t);
        temp_node_map_->Enter(t, inode);
      }
    }
  }
  // 再添加edge
  for (fg::FNodePtr node : flowgraph_->Nodes()->GetList()) {
    assem::Instr *instr = node->NodeInfo();
    if (typeid(*instr) == typeid(assem::MoveInstr)) {
      for (temp::Temp *deft : instr->Def()->GetList()) {
        for (temp::Temp *outt :
             Diff(out_->Look(node), instr->Use())->GetList()) {
          if (deft == reg_manager->StackPointer() ||
              outt == reg_manager->StackPointer())
            continue;
          live_graph_.interf_graph->AddEdge(temp_node_map_->Look(deft),
                                            temp_node_map_->Look(outt));
          live_graph_.interf_graph->AddEdge(temp_node_map_->Look(outt),
                                            temp_node_map_->Look(deft));
        }
        for (temp::Temp *uset : instr->Use()->GetList()) {
          if (deft == reg_manager->StackPointer() ||
              uset == reg_manager->StackPointer())
            continue;
          if (!live_graph_.moves->Contain(temp_node_map_->Look(uset),
                                          temp_node_map_->Look(deft))) {
            live_graph_.moves->Append(temp_node_map_->Look(uset),
                                      temp_node_map_->Look(deft));
          }
        }
      }
    } else {
      for (temp::Temp *deft : instr->Def()->GetList()) {
        for (temp::Temp *outt : out_->Look(node)->GetList()) {
          if (deft == reg_manager->StackPointer() ||
              outt == reg_manager->StackPointer())
            continue;
          live_graph_.interf_graph->AddEdge(temp_node_map_->Look(deft),
                                            temp_node_map_->Look(outt));
          live_graph_.interf_graph->AddEdge(temp_node_map_->Look(outt),
                                            temp_node_map_->Look(deft));
        }
      }
    }
  }
}

void LiveGraphFactory::Liveness() {
  LiveMap();
  InterfGraph();
}

temp::TempList *LiveGraphFactory::Union(temp::TempList *left,
                                        temp::TempList *right) {
  temp::TempList *res = new temp::TempList();
  for (temp::Temp *t : left->GetList()) {
    res->Append(t);
  }
  for (temp::Temp *t : right->GetList()) {
    if (!Contain(res, t)) {
      res->Append(t);
    }
  }

  return res;
}

temp::TempList *LiveGraphFactory::Diff(temp::TempList *left,
                                       temp::TempList *right) {
  temp::TempList *res = new temp::TempList();
  for (temp::Temp *t : left->GetList()) {
    if (!Contain(right, t)) {
      res->Append(t);
    }
  }
  return res;
}

bool LiveGraphFactory::Contain(temp::TempList *left, temp::Temp *right) {
  for (temp::Temp *t : left->GetList()) {
    if (t == right)
      return true;
  }
  return false;
}

bool LiveGraphFactory::Equal(temp::TempList *left, temp::TempList *right) {
  if (left->GetList().size() != right->GetList().size()) {
    return false;
  }
  for (temp::Temp *t : left->GetList()) {
    if (!Contain(right, t)) {
      return false;
    }
  }
  for (temp::Temp *t : right->GetList()) {
    if (!Contain(left, t)) {
      return false;
    }
  }
  return true;
}

} // namespace live