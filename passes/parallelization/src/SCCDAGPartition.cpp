#include "SCCDAGPartition.hpp"

using namespace llvm;

SCCDAGSubset::SCCDAGSubset (SCCDAGInfo *sccdagInfo, LoopInfoSummary *loopInfo, std::set<SCC *> &sccs)
  : SCCs{sccs}, cost{0} {
  collectSubsetLoopInfo(sccdagInfo, loopInfo);
  collectSubsetSCCInfo(sccdagInfo);
}

SCCDAGSubset::SCCDAGSubset (SCCDAGInfo *sccdagInfo, LoopInfoSummary *loopInfo, SCCDAGSubset *subsetA, SCCDAGSubset *subsetB) 
  : cost{0} {
  for (auto scc : subsetA->SCCs) this->SCCs.insert(scc);
  for (auto scc : subsetB->SCCs) this->SCCs.insert(scc);
  collectSubsetLoopInfo(sccdagInfo, loopInfo);
  collectSubsetSCCInfo(sccdagInfo);
}

void SCCDAGSubset::collectSubsetLoopInfo (SCCDAGInfo *sccdagInfo, LoopInfoSummary *loopInfo) {

  /*
   * Collect all potentially fully-contained loops in the subset
   */
  std::unordered_map<LoopSummary *, std::set<BasicBlock *>> loopToBBContainedMap;
  for (auto scc : SCCs) {
      for (auto bb : sccdagInfo->getBasicBlocks(scc)){
          loopToBBContainedMap[loopInfo->bbToLoop[bb]].insert(bb);
      }
  }

  /*
   * Determine which loops are fully contained
   */
  for (auto loopBBs : loopToBBContainedMap) {
      bool fullyContained = true;
      for (auto bb : loopBBs.first->bbs) {
          fullyContained &= loopBBs.second.find(bb) != loopBBs.second.end();
      }
      if (fullyContained) this->loopsContained.insert(loopBBs.first);
  }

  return ;
}

/*
 * TODO: Use info on contained loops to partially determine total cost
 * TODO: Determine whether subset is DOALL or SEQuential
 */
void SCCDAGSubset::collectSubsetSCCInfo (SCCDAGInfo *sccdagInfo) {
    this->cost = sccdagInfo->getSCCSubsetCost(SCCs);
}

raw_ostream &printMinimalSCCs (raw_ostream &stream, std::string prefixToUse, std::set<SCC *> &sccs) {
    for (auto &removableSCC : sccs) {
        stream << prefixToUse << "Internal nodes: " << "\n";
        for (auto nodePair : removableSCC->internalNodePairs()) {
            nodePair.first->print(stream << prefixToUse << "\t");
            stream << "\n";
        }
    }
    return stream;
}

raw_ostream &SCCDAGSubset::print (raw_ostream &stream, std::string prefixToUse) {
    return printMinimalSCCs(stream, prefixToUse, this->SCCs);
}

bool SCCDAGPartition::isValidSubset (SCCDAGSubset *subset) {
    return validSubsets.find(subset) != validSubsets.end();
}

SCCDAGSubset *SCCDAGPartition::addSubset (SCC * scc) {
    std::set<SCC *> sccs = { scc };
    return this->addSubset(sccs);
}

SCCDAGSubset *SCCDAGPartition::addSubset (std::set<SCC *> &sccs) {
    auto subset = std::make_unique<SCCDAGSubset>(sccdagInfo, loopInfo, sccs);
    auto subsetPtr = this->subsets.insert(std::move(subset)).first->get();
    this->manageAddedSubsetInfo(subsetPtr);
    return subsetPtr; 
}

void SCCDAGPartition::initialize (SCCDAG *dag, SCCDAGInfo *dagInfo, LoopInfoSummary *lInfo, int threads) {
    sccDAG = dag;
    sccdagInfo = dagInfo;
    loopInfo = lInfo;
    idealThreads = threads;
    totalCost = 0;
}

void SCCDAGPartition::removeSubset (SCCDAGSubset *subset) {
    for (auto &p : this->subsets) {
        if (p.get() == subset) {
            validSubsets.erase(subset);
            this->totalCost -= subset->cost;
            this->subsets.erase(p);
            return;
        }
    }
}

SCCDAGSubset *SCCDAGPartition::mergeSubsets (SCCDAGSubset *subsetA, SCCDAGSubset *subsetB) {
    auto subset = std::make_unique<SCCDAGSubset>(sccdagInfo, loopInfo, subsetA, subsetB);
    auto newSubset = this->subsets.insert(std::move(subset)).first->get();

    this->removeSubset(subsetA);
    this->removeSubset(subsetB);
    this->manageAddedSubsetInfo(newSubset);
    return newSubset;
}

SCCDAGSubset *SCCDAGPartition::demoMergeSubsets (SCCDAGSubset *subsetA, SCCDAGSubset *subsetB) {
    return new SCCDAGSubset(sccdagInfo, loopInfo, subsetA, subsetB);
}

bool SCCDAGPartition::canMergeSubsets (SCCDAGSubset *subsetA, SCCDAGSubset *subsetB) {
    std::set<SCC *> outgoingToB;
    for (auto scc : subsetB->SCCs) {
        for (auto edge : sccDAG->fetchNode(scc)->getIncomingEdges()) {
            outgoingToB.insert(edge->getOutgoingT());
        }
    }

    /*
     * Check that no cycle would form by merging the subsets
     */
    for (auto scc : subsetA->SCCs) {
        for (auto edge : sccDAG->fetchNode(scc)->getOutgoingEdges()) {
            if (subsetA->SCCs.find(edge->getIncomingT()) != subsetA->SCCs.end()) continue;
            if (outgoingToB.find(edge->getIncomingT()) != outgoingToB.end()) return false;
        }
    }
    return true;
}

void SCCDAGPartition::manageAddedSubsetInfo (SCCDAGSubset *subset) {
    this->totalCost += subset->cost;
    validSubsets.insert(subset);
    for (auto scc : subset->SCCs) this->fromSCCToSubset[scc] = subset;
}

SCCDAGSubset *SCCDAGPartition::subsetOf (SCC *scc) {
    auto iter = this->fromSCCToSubset.find(scc);
    return (iter == this->fromSCCToSubset.end() ? nullptr : iter->second);
}

bool SCCDAGPartition::isRemovable (SCC *scc) {
    return (this->removableNodes.find(scc) != this->removableNodes.end());
}

int SCCDAGPartition::numEdgesBetween (SCCDAGSubset *subsetA, SCCDAGSubset *subsetB) {
    int edgeCount = 0;
    for (auto scc : subsetA->SCCs) {
        for (auto edge : sccDAG->fetchNode(scc)->getOutgoingEdges()) {
            if (subsetB->SCCs.find(edge->getIncomingT()) != subsetB->SCCs.end()) {
                edgeCount++;
            }
        }
    }
    return edgeCount;
}

std::set<DGNode<SCC> *> SCCDAGPartition::getSCCNodes (SCCDAGSubset *subset) {
    std::set<DGNode<SCC> *> sccNodes;
    for (auto scc : subset->SCCs) sccNodes.insert(this->sccDAG->fetchNode(scc));
    return sccNodes;    
}

std::set<SCCDAGSubset *> SCCDAGPartition::getDependents (SCCDAGSubset *subset) {
    auto sccNodes = this->getSCCNodes(subset);
    std::set<SCCDAGSubset *> subsets = this->getDependents(sccNodes);
    if (subsets.find(subset) != subsets.end()) subsets.erase(subset);
    return subsets;
}

std::set<SCCDAGSubset *> SCCDAGPartition::getAncestors (SCCDAGSubset *subset) {
    auto sccNodes = this->getSCCNodes(subset);
    std::set<SCCDAGSubset *> subsets = this->getAncestors(sccNodes);
    if (subsets.find(subset) != subsets.end()) subsets.erase(subset);
    return subsets;
}

std::set<SCCDAGSubset *> SCCDAGPartition::getDependents (std::set<DGNode<SCC> *> &sccNodes) {
    auto addDependents = [&](std::queue<DGNode<SCC> *> &sccToCheck, DGNode<SCC> *sccNode) -> void {
        for (auto edge : sccNode->getOutgoingEdges()) {
            sccToCheck.push(edge->getIncomingNode());
        }
    };
    return getRelated(sccNodes, addDependents);
}

std::set<SCCDAGSubset *> SCCDAGPartition::getAncestors (std::set<DGNode<SCC> *> &sccNodes) {
    auto addAncestors = [&](std::queue<DGNode<SCC> *> &sccToCheck, DGNode<SCC> *sccNode) -> void {
        for (auto edge : sccNode->getIncomingEdges()) {
            sccToCheck.push(edge->getOutgoingNode());
        }
    };
    return getRelated(sccNodes, addAncestors);
}

std::set<SCCDAGSubset *> SCCDAGPartition::getRelated (std::set<DGNode<SCC> *> &sccNodes,
    std::function<void (std::queue<DGNode<SCC> *> &, DGNode<SCC> *)> addKinFunc) {

    std::set<SCCDAGSubset *> related;
    for (auto sccNode : sccNodes) {
        auto selfSubset = this->subsetOf(sccNode->getT());
        std::queue<DGNode<SCC> *> sccToCheck;
        sccToCheck.push(sccNode);
        while (!sccToCheck.empty()) {
            auto sccNode = sccToCheck.front();
            sccToCheck.pop();

            auto subset = this->subsetOf(sccNode->getT());
            if (subset && subset != selfSubset) {
                if (related.find(subset) == related.end()) {
                    related.insert(subset);
                }
                continue;
            }

            addKinFunc(sccToCheck, sccNode);
        }
    }
    return related;
}

std::set<SCCDAGSubset *> SCCDAGPartition::getCousins (SCCDAGSubset *subset) {
    auto sccNodes = this->getSCCNodes(subset);
    std::set<SCCDAGSubset *> subsets = this->getAncestors(sccNodes);
    if (subsets.find(subset) != subsets.end()) subsets.erase(subset);
    
    std::set<SCCDAGSubset *> neighbors;
    for (auto otherSubset : subsets) {
        auto partSCCNodes = this->getSCCNodes(otherSubset);
        auto otherParts = this->getDependents(partSCCNodes);
        if (otherParts.find(otherSubset) != otherParts.end()) otherParts.erase(otherSubset);
        if (otherParts.find(subset) != otherParts.end()) otherParts.erase(subset);
        neighbors.insert(otherParts.begin(), otherParts.end());
    }
    return neighbors;
}

std::set<SCCDAGSubset *> SCCDAGPartition::topLevelSubsets () {
    std::set<SCCDAGSubset *> topLevelSubsets;
    auto topLevelNodes = sccDAG->getTopLevelNodes();
    for (auto node : topLevelNodes) {
        auto subset = this->subsetOf(node->getT());
        if (subset) topLevelSubsets.insert(subset);
    }

    /*
     * Should the top level nodes be removable, grab their descendants which belong to subsets
     */
    if (topLevelSubsets.size() == 0) {
        topLevelSubsets = this->getDependents(topLevelNodes);
    }

    std::set<SCCDAGSubset *> rootSubsets;
    for (auto subset : topLevelSubsets) {
        if (this->getAncestors(subset).size() > 0) continue;
        rootSubsets.insert(subset);
    }

    return rootSubsets;
}

std::set<SCCDAGSubset *> SCCDAGPartition::nextLevelSubsets (SCCDAGSubset *subset) {
    auto subsets = this->getDependents(subset);
    std::set<SCCDAGSubset *> nextSubsets;
    for (auto depSub : subsets) {
        auto prevSubsets = this->getAncestors(depSub);
        bool noPresentAncestors = true;
        for (auto prevSub : prevSubsets) {
            if (subsets.find(prevSub) != subsets.end()) {
                noPresentAncestors = false;
                break;
            }
        }
        if (noPresentAncestors) nextSubsets.insert(depSub);
    }
    return nextSubsets;
}

raw_ostream &SCCDAGPartition::print (raw_ostream &stream, std::string prefixToUse) {
    for (auto &subset : this->subsets) {
        subset->print(stream << prefixToUse << "Subset:\n", prefixToUse);
    }
    return printMinimalSCCs(stream << prefixToUse << "Removable nodes:\n", prefixToUse, this->removableNodes);
}