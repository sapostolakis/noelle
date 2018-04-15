#pragma once

#include "llvm/IR/Module.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/GraphWriter.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/ADT/iterator_range.h"
#include <set>

#include "DGBase.hpp"
#include "SCC.hpp"
#include "PDG.hpp"

using namespace std;
using namespace llvm;

namespace llvm {

  /*
   * Program Dependence Graph.
   */
  class SCCDAG : public DG<SCC> {
    public:
      SCCDAG() ;
      ~SCCDAG() ;

      static SCCDAG *createSCCDAGFrom(PDG *);
  };
}