#include "DGSimplify.hpp"

/*
 * Options of the dependence graph simplifier pass.
 */
static cl::opt<bool> ForceInlineToLoop("dgsimplify-inline-to-loop", cl::ZeroOrMore, cl::Hidden, cl::desc("Force inlining along the call graph from main to the loops being parallelized"));

DGSimplify::~DGSimplify () {
  for (auto orderedLoops : preOrderedLoops) {
    delete orderedLoops.second;
  }
  for (auto l : loopSummaries) {
    delete l;
  }
}

bool llvm::DGSimplify::doInitialization (Module &M) {
  errs() << "DGSimplify at \"doInitialization\"\n" ;

  return false;
}

bool llvm::DGSimplify::runOnModule (Module &M) {
  errs() << "DGSimplify at \"runOnModule\"\n";

  /*
   * Collect function and loop ordering to track inlining progress
   */
  auto main = M.getFunction("main");
  collectFnGraph(main);
  collectInDepthOrderFns(main);
  // OPTIMIZATION(angelo): Do this lazily, depending on what functions are considered in algorithms
  for (auto func : depthOrderedFns) {
    collectPreOrderedLoopsFor(func);
  }
  // printFnCallGraph();
  // printFnOrder();

  auto writeToContinueFile = []() -> void {
    ofstream continuefile("dgsimplify_continue.txt");
    continuefile << "1\n";
    continuefile.close();
  };

  /*
   * Inline calls within large SCCs of targeted loops
   */
  ifstream doCallInlineFile("dgsimplify_do_scc_call_inline.txt");
  bool doInline = doCallInlineFile.good();
  doCallInlineFile.close();
  if (doInline) {
    std::string filename = "dgsimplify_scc_call_inlining.txt";
    getLoopsToInline(filename);
    bool inlined = inlineCallsInMassiveSCCsOfLoops();
    if (inlined) {
      collectInDepthOrderFns(main);
      // printFnCallGraph();
      // printFnOrder();
    }
    bool remaining = registerRemainingLoops(filename);
    if (remaining) writeToContinueFile();
    else errs() << "DGSimplify:   No remaining call inlining in SCCs\n";
    return inlined;
  }

  /*
   * Inline functions containing targeted loops so the loop is in main
   */
  ifstream doHoistFile("dgsimplify_do_hoist.txt");
  bool doHoist = doHoistFile.good();
  doHoistFile.close();
  if (doHoist) {
    std::string filename = "dgsimplify_loop_hoisting.txt";
    getFunctionsToInline(filename);
    bool inlined = inlineFnsOfLoopsToCGRoot();
    if (inlined) {
      collectInDepthOrderFns(main);
      // printFnCallGraph();
      // printFnOrder();
    }
    bool remaining = registerRemainingFunctions(filename);
    if (remaining) writeToContinueFile();
    else errs() << "DGSimplify:   No remaining hoists\n";
    return inlined;
  }

  return false;
}

void llvm::DGSimplify::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addRequired<LoopInfoWrapperPass>();
  AU.addRequired<CallGraphWrapperPass>();
  AU.addRequired<PDGAnalysis>();
  AU.addRequired<PostDominatorTreeWrapperPass>();
  AU.addRequired<ScalarEvolutionWrapperPass>();
  AU.setPreservesAll();
  return ;
}

void llvm::DGSimplify::getLoopsToInline (std::string filename) {
  loopsToCheck.clear();
  ifstream infile(filename);
  if (infile.good()) {
    std::string line;
    std::string delimiter = ",";
    while(getline(infile, line)) {
      size_t i = line.find(delimiter);
      int fnInd = std::stoi(line.substr(0, i));
      int loopInd = std::stoi(line.substr(i + delimiter.length()));
      auto F = depthOrderedFns[fnInd];
      auto &loops = *preOrderedLoops[F];
      assert(loopInd < loops.size());
      loopsToCheck[F].insert(loops[loopInd]);
      errs() << "DGSimplify:   Checking: FN index: "
        << fnInd << " " << F->getName()
        << ", LOOP: " << loopInd << "\n";
    }
    return;
  }

  // NOTE(angelo): Default to selecting all loops in the program
  for (auto funcLoops : preOrderedLoops) {
    auto F = funcLoops.first;
    for (auto summary : *funcLoops.second) {
      loopsToCheck[F].insert(summary);
      errs() << "DGSimplify:   Checking: FN index: "
        << fnOrders[F] << " " << F->getName()
        << ", LOOP: " << summary->id << "\n";
    }
  }
}

void llvm::DGSimplify::getFunctionsToInline (std::string filename) {
  fnsToCheck.clear();
  ifstream infile(filename);
  if (infile.good()) {
    std::string line;
    while(getline(infile, line)) {
      int fnInd = std::stoi(line);
      auto F = depthOrderedFns[fnInd];
      fnsToCheck.insert(F);
      errs() << "DGSimplify:   Checking: FN index: "
        << fnInd << " " << F->getName() << "\n";
    }
    return;
  }

  // NOTE(angelo): Default to select all functions with loops in them
  for (auto funcLoops : preOrderedLoops) {
    auto F = funcLoops.first;
    fnsToCheck.insert(F);
      errs() << "DGSimplify:   Checking: FN index: "
        << fnOrders[F] << " " << F->getName() << "\n";
  }
}

bool llvm::DGSimplify::registerRemainingLoops (std::string filename) {
  remove(filename.c_str());
  if (loopsToCheck.size() == 0) return false;

  ofstream outfile(filename);
  for (auto funcLoops : loopsToCheck) {
    auto F = funcLoops.first;
    int fnInd = fnOrders[F];
    for (auto summary : funcLoops.second) {
      int loopInd = summary->id;
      errs() << "DGSimplify:   Remaining: FN index: "
        << fnInd << " " << F->getName()
        << ", LOOP index: " << loopInd << "\n";
      outfile << fnInd << "," << loopInd << "\n";
    }
  }
  outfile.close();
  return true;
}

bool llvm::DGSimplify::registerRemainingFunctions (std::string filename) {
  remove(filename.c_str());
  if (fnsToCheck.size() == 0) return false;

  ofstream outfile(filename);
  for (auto F : fnsToCheck) {
    int fnInd = fnOrders[F];
    errs() << "DGSimplify:   Remaining: FN index: "
      << fnInd << " " << F->getName() << "\n";
    outfile << fnInd << "\n";
  }
  outfile.close();
  return true;
}

bool llvm::DGSimplify::inlineCallsInMassiveSCCsOfLoops () {
  auto &PDGA = getAnalysis<PDGAnalysis>();
  bool anyInlined = false;

  // NOTE(angelo): Order these functions to prevent duplicating loops yet to be checked
  std::vector<Function *> orderedFns;
  for (auto fnLoops : loopsToCheck) orderedFns.push_back(fnLoops.first);
  std::sort(orderedFns.begin(), orderedFns.end(), [this](Function *a, Function *b) {
    // NOTE(angelo): Sort functions deepest first
    return fnOrders[a] > fnOrders[b];
  });

  std::set<Function *> fnsToAvoid;
  for (auto F : orderedFns) {
    // errs() << "Traversing looping functions; at fn: " << fnOrders[F] << "\n";

    // NOTE(angelo): If we avoid this function until next pass, we do the same with its parents
    if (fnsToAvoid.find(F) != fnsToAvoid.end()) {
      for (auto parentF : parentFns[F]) fnsToAvoid.insert(parentF);
      continue;
    }

    auto& PDT = getAnalysis<PostDominatorTreeWrapperPass>(*F).getPostDomTree();
    auto& LI = getAnalysis<LoopInfoWrapperPass>(*F).getLoopInfo();
    auto& SE = getAnalysis<ScalarEvolutionWrapperPass>(*F).getSE();
    auto fdg = PDGA.getFunctionPDG(*F);
    auto loopsPreorder = LI.getLoopsInPreorder();

    bool inlined = false;
    std::set<LoopSummary *> removeSummaries;
    for (auto summary : loopsToCheck[F]) {
      auto loop = loopsPreorder[summary->id];
      auto LDI = new LoopDependenceInfo(F, fdg, loop, LI, PDT);
      auto &attrs = LDI->sccdagAttrs;
      attrs.populate(LDI->loopSCCDAG, LDI->liSummary, SE);
      bool inlinedCall = inlineCallsInMassiveSCCs(F, LDI);
      if (!inlinedCall) removeSummaries.insert(summary);

      inlined |= inlinedCall;
      delete LDI;
      if (inlined) break;
    }

    delete fdg;
    anyInlined |= inlined;

    // NOTE(angelo): Avoid parents of affected fns; we are not finished with the affected fns
    if (inlined) {
      for (auto parentF : parentFns[F]) fnsToAvoid.insert(parentF);
    }
    for (auto summary : removeSummaries) loopsToCheck[F].erase(summary);
    // NOTE(angelo): Clear function entries without any more loops to check
    if (loopsToCheck[F].size() == 0) loopsToCheck.erase(F);
  }

  return anyInlined;
}

/*
 * GOAL: Go through loops in function
 * If there is only one non-clonable/reducable SCC,
 * try inlining the function call in that SCC with the
 * most memory edges to other internal/external values
 */
bool llvm::DGSimplify::inlineCallsInMassiveSCCs (Function *F, LoopDependenceInfo *LDI) {
  std::set<SCC *> sccsToCheck;
  for (auto sccNode : LDI->loopSCCDAG->getNodes()) {
    auto scc = sccNode->getT();
    if (!LDI->sccdagAttrs.executesCommutatively(scc)
        && !LDI->sccdagAttrs.executesIndependently(scc)
        && !LDI->sccdagAttrs.canBeCloned(scc)) {
      sccsToCheck.insert(scc);
    }
  }

  /*
   * NOTE: if there are more than two non-trivial SCCs, then
   * there is less incentive to continue trying to inline.
   * Why 2? Because 2 is always a simple non-trivial number
   * to start a heuristic at.
   */
  if (sccsToCheck.size() > 2) return false;

  int64_t maxMemEdges = 0;
  CallInst *inlineCall = nullptr;
  for (auto scc : sccsToCheck) {
    for (auto valNode : scc->getNodes()) {
      auto val = valNode->getT();
      if (auto call = dyn_cast<CallInst>(val)) {
        auto callF = call->getCalledFunction();
        if (!callF || callF->empty()) continue;

        // NOTE(angelo): Do not consider inlining a recursive function call
        if (callF == F) continue;

        // NOTE(angelo): Do not consider inlining calls to functions of lower depth
        if (fnOrders[callF] < fnOrders[F]) continue;

        auto memEdgeCount = 0;
        for (auto edge : valNode->getAllConnectedEdges()) {
          if (edge->isMemoryDependence()) memEdgeCount++;
        }
        if (memEdgeCount > maxMemEdges) {
          maxMemEdges = memEdgeCount;
          inlineCall = call;
        }
      }
    }
  }

  return inlineCall && inlineFunctionCall(F, inlineCall->getCalledFunction(), inlineCall);
}

bool llvm::DGSimplify::inlineFnsOfLoopsToCGRoot () {
  std::vector<Function *> orderedFns;
  for (auto fn : fnsToCheck) orderedFns.push_back(fn);
  std::sort(orderedFns.begin(), orderedFns.end(), [this](Function *a, Function *b) {
    // NOTE(angelo): Sort functions deepest first
    return fnOrders[a] > fnOrders[b];
  });

  int fnIndex = 0;
  std::set<Function *> fnsWillCheck(orderedFns.begin(), orderedFns.end());
  std::set<Function *> fnsToAvoid;
  bool inlined = false;
  while (fnIndex < orderedFns.size()) {
    auto childF = orderedFns[fnIndex++];
    // errs() << "Traversing through CG; at fn: " << fnOrders[childF] << "\n";
    // NOTE(angelo): If we avoid this function until next pass, we do the same with its parents
    if (fnsToAvoid.find(childF) != fnsToAvoid.end()) {
      for (auto parentF : parentFns[childF]) fnsToAvoid.insert(parentF);
      continue;
    }

    // NOTE(angelo): Cache parents as inlining may remove them
    std::set<Function *> parents;
    for (auto parent : parentFns[childF]) parents.insert(parent);

    // NOTE(angelo): Try to inline this child function in all of its parents
    bool inlinedInParents = true;
    for (auto parentF : parents) {
      if (!canInlineWithoutRecursiveLoop(parentF, childF)) continue;

      // NOTE(angelo): Do not inline recursive function calls
      if (parentF == childF) continue;

      // NOTE(angelo): Do not inline from less deep to more deep (to avoid recursive chains)
      if (fnOrders[parentF] > fnOrders[childF]) continue;

      // NOTE(angelo): Cache calls as inlining affects the call list in childrenFns
      std::set<CallInst *> calls;
      for (auto call : childrenFns[parentF][childF]) calls.insert(call);

      // NOTE(angelo): Since only one inline per function is permitted, this loop
      //  either inlines no calls (should the parent already be affected) or inlines
      //  the first call, indicating whether there are more calls to inline
      bool inlinedCalls = true;
      for (auto call : calls) {
        bool inlinedCall = inlineFunctionCall(parentF, childF, call);
        inlined |= inlinedCall;
        inlinedCalls &= inlinedCall;
      }
      inlinedInParents &= inlinedCalls;

      // NOTE(angelo): Function isn't completely inlined in parent; avoid parent
      if (!inlinedCalls) {
        fnsToAvoid.insert(parentF);
        continue;
      }

      // NOTE(angelo): Insert parent to affect (in depth order, if not already present)
      if (fnsWillCheck.find(parentF) != fnsWillCheck.end()) continue;
      fnsWillCheck.insert(parentF);
      int insertIndex = -1;
      while (fnOrders[orderedFns[++insertIndex]] > fnOrders[parentF]);
      orderedFns.insert(orderedFns.begin() + insertIndex, parentF);
    }

    if (inlinedInParents) fnsToCheck.erase(childF);
  }

  return inlined;
}

bool llvm::DGSimplify::canInlineWithoutRecursiveLoop (Function *parentF, Function *childF) {
  // NOTE(angelo): Prevent inlining a call to the entry of a recursive chain of functions
  if (recursiveChainEntranceFns.find(childF) != recursiveChainEntranceFns.end()) return false ;
  return true;
}

bool llvm::DGSimplify::inlineFunctionCall (Function *F, Function *childF, CallInst *call) {
  // NOTE(angelo): Prevent inlining a call within a function already altered by inlining
  if (fnsAffected.find(F) != fnsAffected.end()) return false ;
  if (!canInlineWithoutRecursiveLoop(F, childF)) return false ;
  // call->print(errs() << "DGSimplify:   Inlining in: " << F->getName() << ", "); errs() << "\n";
  LoopSummary *loopAfterCall = getNextPreorderLoopAfter(F, call);

  InlineFunctionInfo IFI;
  if (InlineFunction(call, IFI)) {
    fnsAffected.insert(F); 
    adjustOrdersAfterInline(F, childF, call, loopAfterCall);
    return true;
  }
  return false;
}

LoopSummary *llvm::DGSimplify::getNextPreorderLoopAfter (Function *F, CallInst *call) {
  if (preOrderedLoops.find(F) == preOrderedLoops.end()) return nullptr;
  auto summaries = preOrderedLoops[F];

  // NOTE(angelo): Mimic getLoopFor, getLoopDepth, and isLoopHeader of llvm LoopInfo API
  auto getSummaryFor = [&](BasicBlock *BB) -> LoopSummary * {
    LoopSummary *deepestSummary = nullptr;
    for (auto summary : *summaries) {
      if (summary->bbs.find(BB) == summary->bbs.end()) continue;
      if (deepestSummary && summary->depth < deepestSummary->depth) continue;
      deepestSummary = summary;
    }
    return deepestSummary;
  };
  auto getSummaryIfHeader = [&](BasicBlock *BB) -> LoopSummary * {
    for (auto summary : *summaries) {
      if (summary->header == BB) return summary;
    }
    return nullptr;
  };

  auto callBB = call->getParent();
  auto callLoop = getSummaryFor(callBB);
  auto callDepth = callLoop ? callLoop->depth : 0;
  bool startSearch = false;
  LoopSummary *prev = nullptr;
  LoopSummary *next = nullptr;
  // NOTE(angelo): Search in forward program order for next loop header
  for (auto &B : *F) {
    if (!startSearch) {
      auto l = getSummaryIfHeader(&B);
      if (l) prev = l;
    }
    if (callBB == &B) {
      startSearch = true;
      continue;
    }
    if (startSearch) {
      auto l = getSummaryIfHeader(&B);
      if (!l) continue;

      /*
       * NOTE(angelo): Next loop header must either be:
       * 1) a direct child of the inner-most loop the call resides in
       * 2) a loop with a smaller depth than the call's inner-most loop
       */
      assert(l->depth <= callDepth + 1);
      next = l;
      break;
    }
  }

  bool nextIsStart = !prev && next->id == 0;
  bool nextIsNone = !next && prev->id == summaries->size() - 1;
  bool prevThenNext = prev && next && prev->id + 1 == next->id;
  assert(nextIsStart || nextIsNone || prevThenNext);
  return next;
}

/* 
 * TODO(angelo): Flaw in approach: Determine previous and next loop in forward program
 * order BEFORE inline. Then inline, and adjust loop order based on loops introduced
 */
void llvm::DGSimplify::adjustOrdersAfterInline (Function *parentF, Function *childF, CallInst *call, LoopSummary *nextLoop) {
  removeFnPairInstance(parentF, childF, call);
  for (auto newChild : childrenFns[childF]) {
    for (auto childCall : newChild.second) {
      addFnPairInstance(parentF, newChild.first, childCall);
    }
  }
  // printFnCallGraph();

  bool parentHasLoops = preOrderedLoops.find(parentF) != preOrderedLoops.end();
  bool childHasLoops = preOrderedLoops.find(childF) != preOrderedLoops.end();
  if (!childHasLoops) return ;
  if (!parentHasLoops) preOrderedLoops[parentF] = new std::vector<LoopSummary *>();

  /*
   * NOTE(angelo): Starting after the loop in the parent function, index all loops in the
   * child function as being now in the parent function and adjust the indices of loops
   * after the call site by the number of loops inserted
   */
  auto &parentLoops = *preOrderedLoops[parentF];
  auto &childLoops = *preOrderedLoops[childF];
  auto startInd = nextLoop ? nextLoop->id : parentLoops.size();
  auto childLoopCount = childLoops.size();
  auto endInd = startInd + childLoopCount;

  // NOTE(angelo): Adjust parent loops after the call site
  parentLoops.resize(parentLoops.size() + childLoopCount);
  for (auto shiftIndex = parentLoops.size() - 1; shiftIndex >= endInd; --shiftIndex) {
    parentLoops[shiftIndex] = parentLoops[shiftIndex - childLoopCount];
  }

  // NOTE(angelo): Insert inlined loops from child function
  for (auto childIndex = startInd; childIndex < endInd; ++childIndex) {
    parentLoops[childIndex] = childLoops[childIndex - startInd];
  }

  // DEBUG(angelo): loop order after inlining
  // printFnLoopOrder(parentF);
}

void llvm::DGSimplify::collectFnGraph (Function *main) {
  auto &callGraph = getAnalysis<CallGraphWrapperPass>().getCallGraph();
  std::queue<Function *> funcToTraverse;
  std::set<Function *> reached;

  /*
   * NOTE(angelo): Traverse call graph, collecting function "parents":
   *  Parent functions are those encountered before their children in a
   *  breadth-first traversal of the call graph
   */
  funcToTraverse.push(main);
  reached.insert(main);
  while (!funcToTraverse.empty()) {
    auto func = funcToTraverse.front();
    funcToTraverse.pop();

    auto funcCGNode = callGraph[func];
    for (auto &callRecord : make_range(funcCGNode->begin(), funcCGNode->end())) {
      auto weakVH = callRecord.first;
      if (!weakVH.pointsToAliveValue() || !isa<CallInst>(*&weakVH)) continue;
      auto F = callRecord.second->getFunction();
      if (!F || F->empty()) continue;

      addFnPairInstance(func, F, (CallInst *)(&*weakVH));
      if (reached.find(F) != reached.end()) continue;
      reached.insert(F);
      funcToTraverse.push(F);
    }
  }
}

/*
 * NOTE(angelo): Determine the depth of functions in the call graph:
 *  next-depth functions are those where every parent function
 *  has already been assigned a previous depth
 * Obviously, recursive loops by this definition have undefined depth.
 *  These groups, each with a chain of recursive functions, are ordered
 *  by their entry points' relative depths. They are assigned depths
 *  after all other directed acyclic portions of the call graph (starting
 *  from their common ancestor) is traversed.
 */
void llvm::DGSimplify::collectInDepthOrderFns (Function *main) {
  depthOrderedFns.clear();
  recursiveChainEntranceFns.clear();
  fnOrders.clear();

  std::queue<Function *> funcToTraverse;
  std::set<Function *> reached;
  std::vector<Function *> *deferred = new std::vector<Function *>();

  funcToTraverse.push(main);
  fnOrders[main] = 0;
  depthOrderedFns.push_back(main);
  reached.insert(main);
  // NOTE(angelo): Check to see whether any functions remain to be traversed
  while (!funcToTraverse.empty()) {
    // NOTE(angelo): Check to see whether any order-able functions remain
    while (!funcToTraverse.empty()) {
      auto func = funcToTraverse.front();
      funcToTraverse.pop();

      for (auto fnCalls : childrenFns[func]) {
        auto F = fnCalls.first;
        if (reached.find(F) != reached.end()) continue;
        
        bool allParentsOrdered = true;
        for (auto parent : parentFns[F]) {
          if (reached.find(parent) == reached.end()) {
            allParentsOrdered = false;
            break;
          }
        }
        if (allParentsOrdered) {
          funcToTraverse.push(F);
          fnOrders[F] = depthOrderedFns.size();
          depthOrderedFns.push_back(F);
          reached.insert(F);
        } else {
          deferred->push_back(F);
        }
      }
    }

    /*
     * NOTE(angelo): Collect all deferred functions that never got ordered.
     * By definition of the ordering, they must all be parts of recursive chains.
     * Order their entry points, add them to the queue to traverse.
     */
    auto remaining = new std::vector<Function *>();
    for (auto left : *deferred) {
      if (fnOrders.find(left) == fnOrders.end()) {
        recursiveChainEntranceFns.insert(left);
        remaining->push_back(left);
        funcToTraverse.push(left);
        fnOrders[left] = depthOrderedFns.size();
        depthOrderedFns.push_back(left);
        reached.insert(left);
      }
    }
    delete deferred;
    deferred = remaining;
  }

  delete deferred;
}

void llvm::DGSimplify::collectPreOrderedLoopsFor (Function *F) {
  // NOTE(angelo): Enforce managing order instead of recalculating it entirely
  if (preOrderedLoops.find(F) != preOrderedLoops.end()) {
    errs() << "DGSimplify:   Misuse! Do not collect ordered loops more than once. Manage current ordering.\n";
  }
  auto& LI = getAnalysis<LoopInfoWrapperPass>(*F).getLoopInfo();
  if (LI.empty()) return;

  int count = 0;
  auto *orderedLoops = new std::vector<LoopSummary *>();
  auto liOrdered = LI.getLoopsInPreorder();
  std::unordered_map<Loop *, LoopSummary *> summaryMap;
  for (auto loop : liOrdered) {
    auto summary = new LoopSummary(count++, loop);
    loopSummaries.insert(summary);
    orderedLoops->push_back(summary);
    summaryMap[loop] = summary;
  }
  for (auto i = 0; i < count; ++i) {
    auto parent = liOrdered[i]->getParentLoop();
    (*orderedLoops)[i]->parent = parent ? summaryMap[parent] : nullptr;
    for (auto childLoop : liOrdered[i]->getSubLoops()) {
      (*orderedLoops)[i]->children.insert(summaryMap[childLoop]);
    }
  }
  preOrderedLoops[F] = orderedLoops;
}

void llvm::DGSimplify::addFnPairInstance (Function *parentF, Function *childF, CallInst *call) {
  auto &children = childrenFns[parentF];
  parentFns[childF].insert(parentF);
  children[childF].insert(call);
}

void llvm::DGSimplify::removeFnPairInstance (Function *parentF, Function *childF, CallInst *call) {
  auto &children = childrenFns[parentF];
  children[childF].erase(call);
  if (children[childF].size() == 0) {
    parentFns[childF].erase(parentF);
    children.erase(childF);
  }
}

void llvm::DGSimplify::printFnCallGraph () {
  for (auto fns : parentFns) {
    errs() << "DGSimplify:   Child function: " << fns.first->getName() << "\n";
    for (auto f : fns.second) {
      errs() << "DGSimplify:   \tParent: " << f->getName() << "\n";
    }
  }
}

void llvm::DGSimplify::printFnOrder () {
  int count = 0;
  for (auto fn : depthOrderedFns) {
    errs() << "DGSimplify:   Function: " << count++ << " " << fn->getName() << "\n";
  }
}

void llvm::DGSimplify::printFnLoopOrder (Function *F) {
  for (auto summary : *preOrderedLoops[F]) {
    auto headerBB = summary->header;
    errs() << "DGSimplify:   Loop " << summary->id << ", depth: " << summary->depth << "\n";
    // headerBB->print(errs()); errs() << "\n";
  }
}