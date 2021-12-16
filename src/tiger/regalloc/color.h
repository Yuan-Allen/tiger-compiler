#ifndef TIGER_COMPILER_COLOR_H
#define TIGER_COMPILER_COLOR_H

#include "tiger/codegen/assem.h"
#include "tiger/frame/temp.h"
#include "tiger/liveness/liveness.h"
#include "tiger/util/graph.h"
#include <map>
#include <set>

namespace col {
struct Result {
  Result() : coloring(nullptr), spills(nullptr) {}
  Result(temp::Map *coloring, live::INodeListPtr spills)
      : coloring(coloring), spills(spills) {}
  temp::Map *coloring;
  live::INodeListPtr spills;
};

class Color {
  /* TODO: Put your lab6 code here */
public:
  Color(live::LiveGraph ln, temp::TempList *noSpillTemps)
      : liveness(ln), noSpillTemp(noSpillTemps) {}
  col::Result doColoring();

private:
  live::LiveGraph liveness;
  // 从一个结点到与该结点相关的传送指令集合
  graph::Table<temp::Temp, live::MoveList> moveList;
  // 有可能合并的传送指令集合
  live::MoveList *worklistMoves;
  temp::Map *coloring;
  // 图中冲突边的集合
  std::set<std::pair<live::INodePtr, live::INodePtr>> adjSet;
  // 图的邻接表表示
  std::map<live::INodePtr, graph::NodeList<temp::Temp>> adjList;
  // 包含每个非precolor节点的当前度数
  std::map<live::INodePtr, int> degree;
  // 高度数的结点表
  graph::NodeList<temp::Temp> spillWorklist;
  // 低度数传送无关的结点表
  graph::NodeList<temp::Temp> simplifyWorklist;
  // 低度数的传送有关的结点表
  graph::NodeList<temp::Temp> freezeWorklist;
  // 还未做好合并准备的传送指令集合
  live::MoveList activeMoves;
  // 一个包含从图中删除的临时变量的栈
  graph::NodeList<temp::Temp> selectStack;
  // 已合并的寄存器集合
  graph::NodeList<temp::Temp> coalescedNodes;
  tab::Table<live::INode, live::INode> alias;
  // 已经合并的传送指令集合
  live::MoveList coalescedMoves;
  // 源操作数和目标操作数冲突的传送指令集合
  live::MoveList constrainedMoves;
  // 不再考虑合并的传送指令集合
  live::MoveList frozenMoves;
  temp::TempList *noSpillTemp;
  // 已成功着色的结点集合
  graph::NodeList<temp::Temp> coloredNodes;
  // 在本轮中要被溢出的结点集合
  graph::NodeList<temp::Temp> *spilledNodes;

  void init();
  void Build();
  void AddEdge(live::INodePtr u, live::INodePtr v);
  bool precolored(temp::Temp *t);
  void MakeWorklist();
  live::INodeListPtr Adjacent(live::INodePtr node);
  live::MoveList *NodeMoves(live::INodePtr node);
  bool MoveRelated(live::INodePtr node);
  void Simplify();
  void DecrementDegree(live::INodePtr node);
  void EnableMoves(graph::NodeList<temp::Temp> *nodes);
  void Coalesce();
  live::INodePtr GetAlias(live::INodePtr node);
  void AddWorkList(live::INodePtr node);
  bool OK(live::INodePtr t, live::INodePtr r);
  bool OKForAll(live::INodeListPtr nodes, live::INodePtr r);
  bool Conservative(live::INodeListPtr nodes);
  void Combine(live::INodePtr u, live::INodePtr v);
  void Freeze();
  void FreezeMoves(live::INodePtr u);
  void SelectSpill();
  void AssignColors();

  void ShowStates();
};
} // namespace col

#endif // TIGER_COMPILER_COLOR_H
