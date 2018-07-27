#pragma once

#include "SCC.hpp"
#include "SCCDAG.hpp"
#include "SCCDAGInfo.hpp"
#include "LoopInfoSummary.hpp"

class SCCDAGSubset {
  public:
    std::set<SCC *> SCCs;
    int cost;
    std::set<LoopSummary *> loopsContained;

    SCCDAGSubset (SCCDAGInfo *sccdagInfo, LoopInfoSummary *loopInfo, std::set<SCC *> &sccs);
    SCCDAGSubset (SCCDAGInfo *sccdagInfo, LoopInfoSummary *loopInfo, SCCDAGSubset *subsetA, SCCDAGSubset *subsetB);

    void collectSubsetLoopInfo(SCCDAGInfo *sccdagInfo, LoopInfoSummary *loopInfo);
    void collectSubsetSCCInfo(SCCDAGInfo *sccdagInfo);

    raw_ostream &print(raw_ostream &stream, std::string prefixToUse);
};

class SCCDAGPartition {
  public:
    std::set<std::unique_ptr<SCCDAGSubset>> subsets;
    std::set<SCC *> removableNodes;

    void initialize (SCCDAG *dag, SCCDAGInfo *dagInfo, LoopInfoSummary *lInfo, int idealThreads);

    bool isValidSubset (SCCDAGSubset *subset);
    SCCDAGSubset *addSubset (SCC *node);
    SCCDAGSubset *addSubset (std::set<SCC *> &subset);
    void removeSubset (SCCDAGSubset *subset);
    SCCDAGSubset *mergeSubsets (SCCDAGSubset *subsetA, SCCDAGSubset *subsetB);
    bool canMergeSubsets (SCCDAGSubset *subsetA, SCCDAGSubset *subsetB);
    SCCDAGSubset *demoMergeSubsets (SCCDAGSubset *subsetA, SCCDAGSubset *subsetB);

    SCCDAGSubset *subsetOf (SCC *scc);
    bool isRemovable (SCC *scc);
    std::set<SCCDAGSubset *> getDependents (SCCDAGSubset *subset);
    std::set<SCCDAGSubset *> getDependents (std::set<DGNode<SCC> *> &sccs);
    std::set<SCCDAGSubset *> getAncestors (SCCDAGSubset *subset);
    std::set<SCCDAGSubset *> getAncestors (std::set<DGNode<SCC> *> &sccs);
    std::set<SCCDAGSubset *> getCousins (SCCDAGSubset *subset);
    std::set<SCCDAGSubset *> topLevelSubsets ();
    std::set<SCCDAGSubset *> nextLevelSubsets (SCCDAGSubset *subset);

    int numEdgesBetween (SCCDAGSubset *subsetA, SCCDAGSubset *subsetB);
    int maxSubsetCost () { return totalCost / idealThreads; }
    int idealThreadCount () { return idealThreads; }

    raw_ostream &print(raw_ostream &stream, std::string prefixToUse);

  private:
    std::set<SCCDAGSubset *> getRelated (std::set<DGNode<SCC> *> &sccNodes, std::function<void (std::queue<DGNode<SCC> *> &, DGNode<SCC> *)> addKinFunc);
    void manageAddedSubsetInfo (SCCDAGSubset *subset);
    std::set<DGNode<SCC> *> getSCCNodes (SCCDAGSubset *subset);

    SCCDAG *sccDAG;
    SCCDAGInfo *sccdagInfo;
    LoopInfoSummary *loopInfo;
    std::set<SCCDAGSubset *> validSubsets;
    std::unordered_map<SCC *, SCCDAGSubset *> fromSCCToSubset;
    int totalCost;
    int idealThreads;
};