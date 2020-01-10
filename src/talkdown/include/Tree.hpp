#pragma once

#include "llvm/IR/Function.h"

#include "Node.hpp"

using namespace llvm;

namespace llvm {

  class FunctionTree
  {
    public:
      FunctionTree();
      FunctionTree(Function *f);
      ~FunctionTree();
      bool constructTree(Function *f);

      SESENode *getInnermostNode(Instruction *);
      SESENode *getParent(SESENode *);
      SESENode *getFirstCommonAncestor(SESENode *, SESENode *);

      friend std::ostream &operator<<(std::ostream &, const FunctionTree &);

      void print();

    private:

      /*
       * Associated function
       */
      Function *associated_function;

      /*
       * Root node of tree
       */
      SESENode *root;

      /*
       * Split nodes of tree recursively
       */
      bool splitNodesRecursive(SESENode *node);

#if 0
      /*
       * Split basic block when annotation changes
       */
      bool insertSplits();
#endif
  };

} // namespace llvm