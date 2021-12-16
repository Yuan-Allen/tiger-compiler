#include "tiger/regalloc/color.h"

extern frame::RegManager *reg_manager;

namespace col {
/* TODO: Put your lab6 code here */
col::Result Color::doColoring() {
  // fprintf(stderr, "doColoring------------------------\n");
  init();
  Build();
  MakeWorklist();
  int i = 0;
  do {
    // fprintf(stderr, "round %d\n", i++);
    // ShowStates();
    if (!simplifyWorklist.GetList().empty()) {
      Simplify();
    } else if (!worklistMoves->GetList().empty()) {
      Coalesce();
    } else if (!freezeWorklist.GetList().empty()) {
      Freeze();
    } else if (!spillWorklist.GetList().empty()) {
      SelectSpill();
    }

  } while (!simplifyWorklist.GetList().empty() ||
           !worklistMoves->GetList().empty() ||
           !freezeWorklist.GetList().empty() ||
           !spillWorklist.GetList().empty());
  AssignColors();
  return col::Result(coloring, spilledNodes);
}

void Color::init() {
  coloring = temp::Map::Empty();
  spilledNodes = new live::INodeList();
  if (noSpillTemp == nullptr) {
    noSpillTemp = new temp::TempList();
  }
}

bool Color::precolored(temp::Temp *t) {
  // rsp 已经在liveness过滤掉了，这里t不会是rsp
  assert(t != reg_manager->StackPointer());
  for (temp::Temp *reg : reg_manager->Registers()->GetList()) {
    if (reg == t)
      return true;
  }
  return false;
}

void Color::Build() {
  for (std::pair<live::INodePtr, live::INodePtr> move :
       liveness.moves->GetList()) {
    if (moveList.Look(move.first) == nullptr) {
      moveList.Enter(move.first, new live::MoveList());
    }
    if (moveList.Look(move.second) == nullptr) {
      moveList.Enter(move.second, new live::MoveList());
    }
    moveList.Look(move.first)->Append(move.first, move.second);
    moveList.Look(move.second)->Append(move.first, move.second);
  }
  worklistMoves = liveness.moves;
  for (live::INodePtr u : liveness.interf_graph->Nodes()->GetList()) {
    for (live::INodePtr v : u->Adj()->GetList()) {
      AddEdge(u, v);
    }
  }

  // precolor
  for (temp::Temp *reg : reg_manager->Registers()->GetList()) {
    coloring->Enter(reg, reg_manager->temp_map_->Look(reg));
  }
}

void Color::AddEdge(live::INodePtr u, live::INodePtr v) {
  if (adjSet.find(std::make_pair(u, v)) == adjSet.end() && u != v) {
    adjSet.insert(std::make_pair(u, v));
    adjSet.insert(std::make_pair(v, u));
    if (!precolored(u->NodeInfo())) {
      adjList[u].Append(v);
      // std::map会默认初始化为0
      degree[u]++;
    }
    if (!precolored(v->NodeInfo())) {
      adjList[v].Append(u);
      degree[v]++;
    }
  }
}

live::MoveList *Color::NodeMoves(live::INodePtr node) {
  if (moveList.Look(node) == nullptr) {
    // return nullptr;
    return new live::MoveList();
  }
  return moveList.Look(node)->Intersect(activeMoves.Union(worklistMoves));
}

bool Color::MoveRelated(live::INodePtr node) {
  live::MoveList *moves = NodeMoves(node);
  if (moves == nullptr || moves->GetList().empty()) {
    return false;
  } else {
    return true;
  }
}

void Color::MakeWorklist() {
  for (live::INodePtr node : liveness.interf_graph->Nodes()->GetList()) {
    if (precolored(node->NodeInfo()))
      continue;
    if (degree[node] >= reg_manager->ColorNum()) {
      spillWorklist.Append(node);
    } else if (MoveRelated(node)) {
      freezeWorklist.Append(node);
    } else {
      simplifyWorklist.Append(node);
    }
  }
}

live::INodeListPtr Color::Adjacent(live::INodePtr node) {
  return adjList[node].Diff(selectStack.Union(&coalescedNodes));
}

void Color::Simplify() {
  // fprintf(stderr, "Simplify\n");
  live::INodePtr node = simplifyWorklist.GetList().front();
  simplifyWorklist.DeleteNode(node);
  selectStack.Append(node);
  // Adjacent()?
  for (live::INodePtr adj : Adjacent(node)->GetList()) {
    DecrementDegree(adj);
  }
}

void Color::DecrementDegree(live::INodePtr node) {
  // fprintf(stderr, "DecrementDegree\n");
  // 认为precolor寄存器的degree是无限大
  if (precolored(node->NodeInfo()))
    return;
  int d = degree[node];
  degree[node] = d - 1;
  if (d == reg_manager->ColorNum()) {
    graph::NodeList<temp::Temp> *tmp = new graph::NodeList<temp::Temp>();
    tmp->Append(node);
    EnableMoves(Adjacent(node)->Union(tmp));
    spillWorklist.DeleteNode(node);
    if (MoveRelated(node)) {
      freezeWorklist.Append(node);
    } else {
      simplifyWorklist.Append(node);
    }
  }
}

void Color::EnableMoves(graph::NodeList<temp::Temp> *nodes) {
  // fprintf(stderr, "EnableMoves\n");
  for (live::INodePtr node : nodes->GetList()) {
    for (std::pair<live::INodePtr, live::INodePtr> move :
         NodeMoves(node)->GetList()) {
      if (activeMoves.Contain(move.first, move.second)) {
        activeMoves.Delete(move.first, move.second);
        worklistMoves->Append(move.first, move.second);
      }
    }
  }
}

void Color::Coalesce() {
  // fprintf(stderr, "coalesce\n");
  live::INodePtr x, y, u, v;
  // x: src, y: dst
  x = worklistMoves->GetList().front().first;
  y = worklistMoves->GetList().front().second;

  // 按照这个放法，如果x, y至少有一个是precolored，那么u一定是precolored
  if (precolored(y->NodeInfo())) {
    u = GetAlias(y);
    v = GetAlias(x);
  } else {
    u = GetAlias(x);
    v = GetAlias(y);
  }
  worklistMoves->Delete(x, y);
  if (u == v) {
    coalescedMoves.Prepend(x, y);
    AddWorkList(u);
  } else if (precolored(v->NodeInfo()) ||
             adjSet.find(std::make_pair(u, v)) != adjSet.end()) {
    constrainedMoves.Append(x, y);
    AddWorkList(u);
    AddWorkList(v);
  } else if ((precolored(u->NodeInfo()) && OKForAll(Adjacent(v), u)) ||
             (!precolored(u->NodeInfo()) &&
              Conservative(Adjacent(u)->Union(Adjacent(v))))) {
    coalescedMoves.Append(x, y);
    Combine(u, v);
    AddWorkList(u);
  } else {
    activeMoves.Append(x, y);
  }
}

live::INodePtr Color::GetAlias(live::INodePtr node) {
  if (coalescedNodes.Contain(node)) {
    assert(alias.Look(node) != nullptr);
    return GetAlias(alias.Look(node));
  } else {
    return node;
  }
}

void Color::AddWorkList(live::INodePtr node) {
  if (!precolored(node->NodeInfo()) && !MoveRelated(node) &&
      degree[node] < reg_manager->ColorNum()) {
    freezeWorklist.DeleteNode(node);
    simplifyWorklist.Append(node);
  }
}

bool Color::OK(live::INodePtr t, live::INodePtr r) {
  return degree[t] < reg_manager->ColorNum() || precolored(t->NodeInfo()) ||
         adjSet.find(std::make_pair(t, r)) != adjSet.end();
}

bool Color::OKForAll(live::INodeListPtr nodes, live::INodePtr r) {
  for (live::INodePtr node : nodes->GetList()) {
    if (!OK(node, r))
      return false;
  }
  return true;
}

bool Color::Conservative(live::INodeListPtr nodes) {
  int k = 0;
  for (live::INodePtr node : nodes->GetList()) {
    // precolored?
    if (precolored(node->NodeInfo()) ||
        degree[node] >= reg_manager->ColorNum()) {
      k++;
    }
  }
  return k < reg_manager->ColorNum();
}

void Color::Combine(live::INodePtr u, live::INodePtr v) {
  // fprintf(stderr, "Combine\n");
  if (freezeWorklist.Contain(v)) {
    freezeWorklist.DeleteNode(v);
  } else {
    spillWorklist.DeleteNode(v);
  }
  coalescedNodes.Append(v);
  if (alias.Look(v) == nullptr) {
    alias.Enter(v, u);
  } else {
    alias.Set(v, u);
  }
  assert(moveList.Look(u) != nullptr && moveList.Look(v) != nullptr);
  moveList.Set(u, moveList.Look(u)->Union(moveList.Look(v)));
  live::INodeListPtr tmp = new live::INodeList();
  tmp->Append(v);
  EnableMoves(tmp);

  for (live::INodePtr node : Adjacent(v)->GetList()) {
    AddEdge(node, u);
    DecrementDegree(node);
  }

  if (degree[u] >= reg_manager->ColorNum() && freezeWorklist.Contain(u)) {
    freezeWorklist.DeleteNode(u);
    spillWorklist.Append(u);
  }
}

void Color::Freeze() {
  // fprintf(stderr, "Freeze\n");
  live::INodePtr node = freezeWorklist.GetList().front();
  freezeWorklist.DeleteNode(node);
  simplifyWorklist.Append(node);
  FreezeMoves(node);
}

void Color::FreezeMoves(live::INodePtr u) {
  // fprintf(stderr, "FreezeMoves\n");
  for (std::pair<live::INodePtr, live::INodePtr> move :
       NodeMoves(u)->GetList()) {
    live::INodePtr v;
    // x: src, y: dst
    live::INodePtr x = move.first;
    live::INodePtr y = move.second;
    // v是这条move与u相连的另一个点
    if (GetAlias(y) == GetAlias(u)) {
      v = GetAlias(x);
    } else {
      v = GetAlias(y);
    }
    activeMoves.Delete(x, y);
    frozenMoves.Append(x, y);

    if ((!precolored(v->NodeInfo())) && NodeMoves(v)->GetList().empty() &&
        degree[v] < reg_manager->ColorNum()) {
      freezeWorklist.DeleteNode(v);
      simplifyWorklist.Append(v);
    }
  }
}

void Color::SelectSpill() {
  // fprintf(stderr, "SelectSpill\n");
  live::INodePtr m = nullptr;
  int maxWeight = 0;
  // 选一个degree最大的
  for (live::INodePtr node : spillWorklist.GetList()) {
    if (live::LiveGraphFactory::Contain(noSpillTemp, node->NodeInfo()))
      continue;
    if (degree[node] > maxWeight) {
      m = node;
      maxWeight = degree[node];
    }
  }
  if (m == nullptr) {
    m = spillWorklist.GetList().front();
  }

  spillWorklist.DeleteNode(m);
  // 强行simplify
  simplifyWorklist.Append(m);
  FreezeMoves(m);
}

void Color::AssignColors() {
  // fprintf(stderr, "AssignColors\n");
  while (!selectStack.GetList().empty()) {
    live::INodePtr n = selectStack.GetList().back();
    selectStack.DeleteNode(n);
    std::set<std::string> okColors;
    // 没有rsp
    for (temp::Temp *reg : reg_manager->Registers()->GetList()) {
      okColors.insert(*(reg_manager->temp_map_->Look(reg)));
    }
    for (live::INodePtr w : adjList[n].GetList()) {
      if (coloredNodes.Contain(GetAlias(w)) ||
          precolored(GetAlias(w)->NodeInfo())) {
        okColors.erase(*(coloring->Look(GetAlias(w)->NodeInfo())));
      }
    }
    if (okColors.empty()) {
      spilledNodes->Append(n);
    } else {
      coloredNodes.Append(n);
      coloring->Enter(n->NodeInfo(), new std::string(*(okColors.begin())));
    }
  }
  for (live::INodePtr n : coalescedNodes.GetList()) {
    coloring->Enter(n->NodeInfo(), coloring->Look(GetAlias(n)->NodeInfo()));
  }
}

void Color::ShowStates() {
  fprintf(stderr, "-----------------nodes:-----------------\n");
  fprintf(stderr, "nodes num:%ld\n",
          liveness.interf_graph->Nodes()->GetList().size());
  // for (live::INodePtr node : liveness.interf_graph->Nodes()->GetList()) {
  //   fprintf(stderr, "node %d\n", node->NodeInfo()->Int());
  // }
}

} // namespace col
