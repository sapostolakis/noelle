#pragma once

#include "llvm/IR/Instructions.h"
#include "llvm/Support/raw_ostream.h"
#include <unordered_map>
#include <queue>
#include <set>

using namespace std;
using namespace llvm;

namespace llvm {
  
  /*
   * Program Dependence Graph Node and Edge
   */
  template <class T> class DGNode;
  template <class T, class SubT> class DGEdgeBase;
  template <class T> class DGEdge;

  template <class T>
  class DG
  {
    public:
      typedef typename set<DGNode<T> *>::iterator nodes_iterator;
      typedef typename set<DGNode<T> *>::const_iterator nodes_const_iterator;
      
      typedef typename set<DGEdge<T> *>::iterator edges_iterator;
      typedef typename set<DGEdge<T> *>::const_iterator edges_const_iterator;

      typedef typename unordered_map<T *, DGNode<T> *>::iterator node_map_iterator;

      /*
       * Node and Edge Iterators
       */
      nodes_iterator begin_nodes() { allNodes.begin(); }
      nodes_iterator end_nodes() { allNodes.end(); }
      nodes_const_iterator begin_nodes() const { allNodes.begin(); }
      nodes_const_iterator end_nodes() const { allNodes.end(); }

      node_map_iterator begin_internal_node_map() { internalNodeMap.begin(); }
      node_map_iterator end_internal_node_map() { internalNodeMap.end(); }
      node_map_iterator begin_external_node_map() { externalNodeMap.begin(); }
      node_map_iterator end_external_node_map() { externalNodeMap.end(); }

      edges_iterator begin_edges() { allEdges.begin(); }
      edges_iterator end_edges() { allEdges.end(); }
      edges_const_iterator begin_edges() const { allEdges.begin(); }
      edges_const_iterator end_edges() const { allEdges.end(); }

      /*
       * Node and Edge Properties
       */
      DGNode<T> *getEntryNode() const { return entryNode; }
      void setEntryNode(DGNode<T> *node) { entryNode = node; }

      bool isInternal(T *theT) const { return internalNodeMap.find(theT) != internalNodeMap.end(); }
      bool isExternal(T *theT) const { return externalNodeMap.find(theT) != externalNodeMap.end(); }
      bool isInGraph(T *theT) const { return isInternal(theT) || isExternal(theT); }

      unsigned numNodes() const { return allNodes.size(); }
      unsigned numInternalNodes() const { return internalNodeMap.size(); }
      unsigned numExternalNodes() const { return externalNodeMap.size(); }
      unsigned numEdges() const { return allEdges.size(); }

      /*
       * Iterator ranges
       */
      iterator_range<nodes_iterator>
      getNodes() { return make_range(allNodes.begin(), allNodes.end()); }
      iterator_range<edges_iterator>
      getEdges() { return make_range(allEdges.begin(), allEdges.end()); }

      iterator_range<node_map_iterator>
      internalNodePairs() { return make_range(internalNodeMap.begin(), internalNodeMap.end()); }
      iterator_range<node_map_iterator>
      externalNodePairs() { return make_range(externalNodeMap.begin(), externalNodeMap.end()); }

      /*
       * Fetching/Creating Nodes and Edges
       */
      DGNode<T> *addNode(T *theT, bool inclusion);
      DGNode<T> *fetchOrAddNode(T *theT, bool inclusion);
      DGNode<T> *fetchNode(T *theT);

      DGEdge<T> *addEdge(T *from, T *to);
      DGEdge<T> *copyAddEdge(DGEdge<T> &edgeToCopy);

      /*
       * Merging/Extracting Graphs
       */
      std::set<DGNode<T> *> getTopLevelNodes();
      std::vector<std::set<DGNode<T> *> *> getDisconnectedSubgraphs();
      void removeNode(DGNode<T> *node);
      void addNodesIntoNewGraph(DG<T> &newGraph, std::set<DGNode<T> *> nodesToPartition, DGNode<T> *entryNode);
      void clear();

      raw_ostream & print(raw_ostream &stream);

    protected:
      std::set<DGNode<T> *> allNodes;
      std::set<DGEdge<T> *> allEdges;
      DGNode<T> *entryNode;
      unordered_map<T *, DGNode<T> *> internalNodeMap;
      unordered_map<T *, DGNode<T> *> externalNodeMap;
  };

  template <class T> 
  class DGNode
  {
    public:
      DGNode() : theT(nullptr) {}
      DGNode(T *node) : theT(node) {}

      typedef typename std::vector<DGNode<T> *>::iterator nodes_iterator;
      typedef typename std::set<DGEdge<T> *>::iterator edges_iterator;

      edges_iterator begin_edges() { return allConnectedEdges.begin(); }
      edges_iterator end_edges() { return allConnectedEdges.end(); }

      edges_iterator begin_outgoing_edges() { return outgoingEdges.begin(); }
      edges_iterator end_outgoing_edges() { return outgoingEdges.end(); }
      edges_iterator begin_incoming_edges() { return incomingEdges.begin(); }
      edges_iterator end_incoming_edges() { return incomingEdges.end(); }

      nodes_iterator begin_outgoing_nodes() { return outgoingNodeInstances.begin(); }
      nodes_iterator end_outgoing_nodes() { return outgoingNodeInstances.end(); }

      inline iterator_range<edges_iterator>
      getAllConnectedEdges() { return make_range(allConnectedEdges.begin(), allConnectedEdges.end()); }
      inline iterator_range<edges_iterator>
      getOutgoingEdges() { return make_range(outgoingEdges.begin(), outgoingEdges.end()); }
      inline iterator_range<edges_iterator>
      getIncomingEdges() { return make_range(incomingEdges.begin(), incomingEdges.end()); }

      T *getT() const { return theT; }
      std::set<DGEdge<T> *> & getEdgesToAndFromNode(DGNode<T> *node) { return nodeToEdgesMap[node]; }

      unsigned numConnectedEdges() { return allConnectedEdges.size(); }
      unsigned numOutgoingEdges() { return outgoingEdges.size(); }
      unsigned numIncomingEdges() { return incomingEdges.size(); }

      void addIncomingEdge(DGEdge<T> *edge);
      void addOutgoingEdge(DGEdge<T> *edge);
      void removeConnectedEdge(DGEdge<T> *edge);
      void removeConnectedNode(DGNode<T> *node);

      DGEdge<T> *getEdgeInstance(unsigned nodeInstance) { return outgoingEdgeInstances[nodeInstance]; }
      void removeInstance(DGEdge<T> *edge);
      void removeInstances(DGNode<T> *node);

      std::string toString();
      raw_ostream &print(raw_ostream &stream);

    protected:
      T *theT;
      std::set<DGEdge<T> *> allConnectedEdges;
      std::set<DGEdge<T> *> outgoingEdges;
      std::set<DGEdge<T> *> incomingEdges;

      // For use in unconventional graph iteration for LLVM SCCIterator
      std::vector<DGNode<T> *> outgoingNodeInstances;
      std::vector<DGEdge<T> *> outgoingEdgeInstances;

      unordered_map<DGNode<T> *, std::set<DGEdge<T> *>> nodeToEdgesMap;
  };

  template <class T>
  class DGEdge : public DGEdgeBase<T, T>
  {
   public:
    DGEdge(DGNode<T> *src, DGNode<T> *dst) : DGEdgeBase<T, T>(src, dst) {}
    DGEdge(const DGEdge<T> &oldEdge) : DGEdgeBase<T, T>(oldEdge) {}
  };

  template <class T, class SubT>
  class DGEdgeBase
  {
   public:
    DGEdgeBase(DGNode<T> *src, DGNode<T> *dst)
      : from(src), to(dst), memory(false), must(false), readAfterWrite(false), writeAfterWrite(false), isControl(false) {}
    DGEdgeBase(const DGEdgeBase<T, SubT> &oldEdge);

    typedef typename std::set<DGEdge<SubT> *>::iterator edges_iterator;

    edges_iterator begin_sub_edges() { return subEdges.begin(); }
    edges_iterator end_sub_edges() { return subEdges.end(); }
  
    inline iterator_range<edges_iterator>
    getSubEdges() { return make_range(subEdges.begin(), subEdges.end()); }

    std::pair<DGNode<T> *, DGNode<T> *> getNodePair() const { return std::make_pair(from, to); }
    void setNodePair(DGNode<T> *from, DGNode<T> *to) { this->from = from; this->to = to; }
    DGNode<T> * getOutgoingNode() const { return from; }
    DGNode<T> * getIncomingNode() const { return to; }
    T * getOutgoingT() const { return from->getT(); }
    T * getIncomingT() const { return to->getT(); }

    bool isMemoryDependence() const { return memory; }
    bool isMustDependence() const { return must; }
    bool isRAWDependence() const { return readAfterWrite; }
    bool isControlDependence() const { return isControl; }

    void setControl(bool ctrl) { isControl = ctrl; }
    void setMemMustRaw(bool mem, bool must, bool raw);

    void addSubEdge(DGEdge<SubT> *edge) { subEdges.insert(edge); }
    void removeSubEdge(DGEdge<SubT> *edge) { subEdges.erase(edge); }
    void clearSubEdges() { subEdges.clear(); }

    std::string toString();
    raw_ostream &print(raw_ostream &stream);

   protected:
    DGNode<T> *from, *to;
    std::set<DGEdge<SubT> *> subEdges;
    bool memory, must, readAfterWrite, writeAfterWrite, isControl;
  };

  /*
   * DG<T> class method implementations
   */
  template <class T>
  DGNode<T> *DG<T>::addNode(T *theT, bool inclusion)
  {
    auto *node = new DGNode<T>(theT);
    allNodes.insert(node);
    auto &map = inclusion ? internalNodeMap : externalNodeMap;
    map[theT] = node;
    return node;
  }

  template <class T>
  DGNode<T> *DG<T>::fetchOrAddNode(T *theT, bool inclusion)
  {
    if (isInGraph(theT)) return fetchNode(theT);
    return addNode(theT, inclusion);
  }

  template <class T>
  DGNode<T> *DG<T>::fetchNode(T *theT)
  {
    auto nodeI = internalNodeMap.find(theT);
    return (nodeI != internalNodeMap.end()) ? nodeI->second : externalNodeMap[theT];
  }

  template <class T>
  DGEdge<T> *DG<T>::addEdge(T *from, T *to)
  {
    auto fromNode = fetchNode(from);
    auto toNode = fetchNode(to);
    auto edge = new DGEdge<T>(fromNode, toNode);
    allEdges.insert(edge);
    fromNode->addOutgoingEdge(edge);
    toNode->addIncomingEdge(edge);
    return edge;
  }

  template <class T>
  DGEdge<T> *DG<T>::copyAddEdge(DGEdge<T> &edgeToCopy)
  {
    auto edge = new DGEdge<T>(edgeToCopy);
    allEdges.insert(edge);
    
    /*
     * Point copy of edge to equivalent nodes in this graph
     */
    auto nodePair = edgeToCopy.getNodePair();
    auto fromNode = fetchNode(nodePair.first->getT());
    auto toNode = fetchNode(nodePair.second->getT());
    edge->setNodePair(fromNode, toNode);

    fromNode->addOutgoingEdge(edge);
    toNode->addIncomingEdge(edge);
    return edge;
  }

  template <class T>
  std::set<DGNode<T> *> DG<T>::getTopLevelNodes()
  {
    std::set<DGNode<T> *> topLevelNodes;

    /*
     * Add all nodes that have no incoming nodes (other than self)
     */
    for (auto node : allNodes)
    {
      bool noOtherIncoming = true;
      for (auto incomingE : node->getIncomingEdges())
      {
        noOtherIncoming &= (incomingE->getOutgoingNode() == node);
      }
      if (noOtherIncoming) topLevelNodes.insert(node);
    }
    if (topLevelNodes.size() > 0) return topLevelNodes;

    /*
     * Add a node in the top cycle of the graph
     */
    std::set<DGNode<T> *> visitedNodes;
    auto node = *allNodes.begin();
    while (visitedNodes.find(node) == visitedNodes.end())
    {
      visitedNodes.insert(node);
      for (auto incomingE : node->getIncomingEdges())
      {
        auto incomingNode = incomingE->getOutgoingNode();
        if (incomingNode == node) continue;
        node = incomingNode;
        break;
      }
    }

    topLevelNodes.insert(node);
    return topLevelNodes;
  }

  template <class T>
  std::vector<std::set<DGNode<T> *> *> DG<T>::getDisconnectedSubgraphs()
  {
    std::vector<std::set<DGNode<T> *> *> connectedComponents;
    std::set<DGNode<T> *> visitedNodes;

    for (auto node : allNodes)
    {
      if (visitedNodes.find(node) != visitedNodes.end()) continue;

      /*
       * Perform BFS to find the connected component this node belongs to
       */
      auto component = new std::set<DGNode<T> *>();
      std::queue<DGNode<T> *> connectedNodes;

      visitedNodes.insert(node);
      connectedNodes.push(node);
      while (!connectedNodes.empty())
      {
        auto currentNode = connectedNodes.front();
        connectedNodes.pop();
        component->insert(currentNode);

        auto checkToVisitNode = [&](DGNode<T> *node) -> void {
          if (visitedNodes.find(node) != visitedNodes.end()) return;
          visitedNodes.insert(node);
          connectedNodes.push(node);
        };

        for (auto edge : currentNode->getOutgoingEdges()) checkToVisitNode(edge->getIncomingNode());
        for (auto edge : currentNode->getIncomingEdges()) checkToVisitNode(edge->getOutgoingNode());
      }

      connectedComponents.push_back(component);
    }

    return connectedComponents;
  }

  template <class T>
  void DG<T>::removeNode(DGNode<T> *node)
  {
    auto theT = node->getT();
    auto &map = isInternal(theT) ? internalNodeMap : externalNodeMap;
    map.erase(theT);
    allNodes.erase(node);

    for (auto edge : node->getIncomingEdges()) edge->getOutgoingNode()->removeConnectedNode(node);
    for (auto edge : node->getOutgoingEdges()) edge->getIncomingNode()->removeConnectedNode(node);
    for (auto edge : node->getAllConnectedEdges()) allEdges.erase(edge);
  }

  template <class T>
  void DG<T>::addNodesIntoNewGraph(DG<T> &newGraph, std::set<DGNode<T> *> nodesToPartition, DGNode<T> *entryNode)
  {
    newGraph.entryNode = entryNode;

    for (auto node : nodesToPartition)
    {
      auto theT = node->getT();
      newGraph.addNode(theT, isInternal(theT));
    }

    /*
     * Only add edges that connect between two nodes in the partition
     */
    for (auto node : nodesToPartition)
    {
      for (auto edgeToCopy : node->getOutgoingEdges())
      {
        auto incomingT = edgeToCopy->getIncomingNode()->getT();
        if (!newGraph.isInGraph(incomingT)) continue;
        newGraph.copyAddEdge(*edgeToCopy);
      }
    }
  }

  template <class T>
  void DG<T>::clear()
  {
    allNodes.clear();
    allEdges.clear();
    entryNode = nullptr;
    internalNodeMap.clear();
    externalNodeMap.clear();
  }

  template <class T>
  raw_ostream & DG<T>::print(raw_ostream &stream)
  {
    stream << "Total nodes: " << allNodes.size() << "\n";
    stream << "Internal nodes: " << internalNodeMap.size() << "\n";
    for (auto pair : internalNodePairs()) pair.second->print(errs()) << "\n";
    stream << "External nodes: " << externalNodeMap.size() << "\n";
    for (auto pair : externalNodePairs()) pair.second->print(errs()) << "\n";
    stream << "All edges: " << allEdges.size() << "\n";
    for (auto edge : allEdges) edge->print(errs()) << "\n";
  }

  /*
   * DGNode<T> class method implementations
   */
  template <class T>
  void DGNode<T>::addIncomingEdge(DGEdge<T> *edge)
  {
    incomingEdges.insert(edge);
    allConnectedEdges.insert(edge);
    auto node = edge->getOutgoingNode();
    nodeToEdgesMap[node].insert(edge);
  }
  
  template <class T>
  void DGNode<T>::addOutgoingEdge(DGEdge<T> *edge)
  {
    outgoingEdges.insert(edge);
    allConnectedEdges.insert(edge);
    auto node = edge->getIncomingNode();
    outgoingNodeInstances.push_back(node);
    outgoingEdgeInstances.push_back(edge);
    nodeToEdgesMap[node].insert(edge);
  }

  template <class T>
  void DGNode<T>::removeInstance(DGEdge<T> *edge)
  {
    auto instanceIter = std::find(outgoingEdgeInstances.begin(), outgoingEdgeInstances.end(), edge);
    auto nodeIter = outgoingNodeInstances.begin() + (instanceIter - outgoingEdgeInstances.begin());
    outgoingEdgeInstances.erase(instanceIter);
    outgoingNodeInstances.erase(nodeIter);
  }

  template <class T>
  void DGNode<T>::removeInstances(DGNode<T> *node)
  {
    for (int i = outgoingNodeInstances.size() - 1; i >= 0; --i)
    {
      if (outgoingNodeInstances[i] != node) continue;
      outgoingNodeInstances.erase(outgoingNodeInstances.begin() + i);
      outgoingEdgeInstances.erase(outgoingEdgeInstances.begin() + i);
    }
  }

  template <class T>
  void DGNode<T>::removeConnectedEdge(DGEdge<T> *edge)
  {
    DGNode<T> *node; 
    if (outgoingEdges.find(edge) != outgoingEdges.end())
    {
      outgoingEdges.erase(edge);
      node = edge->getIncomingNode();
      removeInstance(edge);
    }
    else
    {
      incomingEdges.erase(edge);
      node = edge->getOutgoingNode();
    }

    allConnectedEdges.erase(edge);
    nodeToEdgesMap[node].erase(edge);
    if (nodeToEdgesMap[node].empty()) nodeToEdgesMap.erase(node);
  }

  template <class T>
  void DGNode<T>::removeConnectedNode(DGNode<T> *node)
  {
    for (auto edge : nodeToEdgesMap[node])
    {
      outgoingEdges.erase(edge);
      incomingEdges.erase(edge);
      allConnectedEdges.erase(edge);
    }
    nodeToEdgesMap.erase(node);
    removeInstances(node);
  }

  template <class T>
  std::string DGNode<T>::toString()
  {
    std::string nodeStr;
    raw_string_ostream ros(nodeStr);
    theT->print(ros);
    return nodeStr;
  }

  template <>
  inline std::string DGNode<Instruction>::toString()
  {
    if (!theT) return "Empty node\n";
    std::string str;
    raw_string_ostream instStream(str);
    theT->print(instStream << theT->getFunction()->getName() << ": ");
    return str;
  }

  template <class T>
  raw_ostream & DGNode<T>::print(raw_ostream &stream)
  { 
    theT->print(stream);
    return stream;
  }

  /*
   * DGEdge<T> class method implementations
   */
  template <class T, class SubT>
  DGEdgeBase<T, SubT>::DGEdgeBase(const DGEdgeBase<T, SubT> &oldEdge)
  {
    auto nodePair = oldEdge.getNodePair();
    from = nodePair.first;
    to = nodePair.second;
    setMemMustRaw(oldEdge.isMemoryDependence(), oldEdge.isMustDependence(), oldEdge.isRAWDependence());
    setControl(oldEdge.isControlDependence());
    for (auto subEdge : oldEdge.subEdges) addSubEdge(subEdge);
  }

  template <class T, class SubT>
  void DGEdgeBase<T, SubT>::setMemMustRaw(bool mem, bool must, bool raw)
  {
    this->memory = mem;
    this->must = must;
    this->readAfterWrite = raw;
    this->writeAfterWrite = !raw;
  }

  template <class T, class SubT>
  std::string DGEdgeBase<T, SubT>::toString()
  {
    if (this->isControlDependence()) return "CTRL";
    std::string edgeStr;
    raw_string_ostream ros(edgeStr);
    ros << (readAfterWrite ? "RAW " : (writeAfterWrite ? "WAW " : ""));
    ros << (must ? "(must) " : "(may) ");
    ros << (memory ? "from memory " : "") << "\n";
    ros.flush();
    return edgeStr;
  }
  
  template <class T, class SubT>
  raw_ostream & DGEdgeBase<T, SubT>::print(raw_ostream &stream)
  {
    from->print(stream << "From:\t") << "\n";
    to->print(stream << "To:\t") << "\n";
    stream << this->toString();
    return stream;
  }
}