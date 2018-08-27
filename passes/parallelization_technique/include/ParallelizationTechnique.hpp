#pragma once

#include "llvm/IR/Module.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Analysis/ScalarEvolutionExpressions.h"

#include "Parallelization.hpp"
#include "LoopDependenceInfo.hpp"
#include "Heuristics.hpp"

namespace llvm {

  class ParallelizationTechnique {
    public:

      /*
       * Constructor.
       */
      ParallelizationTechnique (Module &module, Verbosity v);

      /*
       * Apply the parallelization technique to the loop LDI.
       */
      virtual bool apply (LoopDependenceInfo *LDI, Parallelization &par, Heuristics *h, ScalarEvolution &SE) = 0;

      /*
       * Can the current parallelization technique be applied to parallelize loop LDI?
       */
      virtual bool canBeAppliedToLoop (LoopDependenceInfo *LDI, Parallelization &par, Heuristics *h, ScalarEvolution &SE) const = 0 ;

    protected:

      /*
       * Fields
       */
      Module& module;
      Verbosity verbose;
  };

}
