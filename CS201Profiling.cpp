/*
 * Authors: Name(s) <email(s)>
 * 
 */

#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/Type.h"

using namespace llvm;

 
namespace {
  struct CS201Profiling : public FunctionPass {
  static char ID;
  CS201Profiling() : FunctionPass(ID) {}

    //----------------------------------
    bool doInitialization(Module &M) override {
      return false;
    }

    //----------------------------------
    bool doFinalization(Module &M) override {
      return false;
    }
    
    //----------------------------------
    bool runOnFunction(Function &F) override {
      return true;
    }

  };
}

char CS201Profiling::ID = 0;
static RegisterPass<CS201Profiling> X("pathProfiling", "CS201Profiling Pass", false, false);

