#pragma once

#include "llvm/IR/Module.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Transforms/Utils/LoopUtils.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Analysis/ScalarEvolutionExpressions.h"
#include "llvm/IR/Dominators.h"
#include "llvm/Analysis/AssumptionCache.h"

#include "LoopDependenceInfo.hpp"
#include "Queue.hpp"

using namespace llvm;

namespace llvm {

  enum class Verbosity { Disabled, Minimal, Pipeline, Maximal };

  struct Parallelization : public ModulePass {
    public:

      /*
       * Fields.
       */
      static char ID;
      IntegerType *int1, *int8, *int16, *int32, *int64;
      Queue queues;

      /*
       * Methods.
       */
      Parallelization();

      virtual ~Parallelization();

      bool doInitialization (Module &M) override ;

      void getAnalysisUsage(AnalysisUsage &AU) const override ;

      bool runOnModule (Module &M) override ;

      std::vector<Function *> * getModuleFunctionsReachableFrom (
        Module *module, 
        Function *startingPoint
        );

      std::vector<LoopDependenceInfo *> * getModuleLoops (
        Module *module, 
        std::function<LoopDependenceInfo * (Function *, PDG *, Loop *, LoopInfo &, PostDominatorTree &)> allocationFunction
        );

      void linkParallelizedLoopToOriginalFunction (
        Module *module, 
        BasicBlock *originalPreHeader, 
        BasicBlock *startOfParallelizedLoopWithinOriginalFunction,
        Value *envArray,
        Value *envIndexForExitVariable,
        SmallVector<BasicBlock *, 10> &loopExitBlocks
        );
  };
}