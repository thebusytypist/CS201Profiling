/*
 * Authors: Ounan Ding <oding001@ucr.edu>
 * 
 */

#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/Type.h"
#include <map>
#include <set>
#include <vector>
#include <iterator>
#include <algorithm>
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
      outs() << F.getName() << "\n";
      
      buildBBMap(F);
      computeLoops(F);

      return true;
    }
  
  private:
    std::map<StringRef, Function::iterator> bbMap;
    std::map<StringRef, std::vector<StringRef>> preds;
    std::vector<std::vector<Function::iterator>> loops;
    
    void buildBBMap(Function& F) {
      for (auto bb = F.begin(); bb != F.end(); ++bb) {
        bbMap[bb->getName()] = bb;
        preds[bb->getName()] = std::vector<StringRef>();
      }

      for (auto bb = F.begin(); bb != F.end(); ++bb) {
        auto t = bb->getTerminator();
        int n = t->getNumSuccessors();
        for (int i = 0; i < n; ++i) {
          auto d = t->getSuccessor(i);
          preds[d->getName()].push_back(bb->getName());
        }
      }
    }

    typedef std::map<StringRef, std::set<StringRef>> DomSet;

    std::set<StringRef> intersectPredecessorsDOM(
      Function::iterator bb, const DomSet& dom) {
      std::set<StringRef> r;
      for (auto pred : preds[bb->getName()]) {
        auto d = dom.find(pred)->second;
        if (r.empty())
          r = d;
        else {
          std::set<StringRef> k;
          std::set_intersection(
            r.begin(), r.end(),
            d.begin(), d.end(),
            std::inserter(k, k.begin()));
          r = k;
        }
      }
      return r;
    }

    void computeLoops(Function& F) {
      loops.clear();

      // Initialize dominator sets.
      DomSet dom;
      std::set<StringRef> a;
      for (auto bb = F.begin(); bb != F.end(); ++bb) {
        if (bb->getName() == F.getEntryBlock().getName()) {
          std::set<StringRef> s;
          s.insert(bb->getName());
          dom[bb->getName()] = s;
        }
        a.insert(bb->getName());
      }
      for (auto bb = F.begin(); bb != F.end(); ++bb) {
        if (bb->getName() != F.getEntryBlock().getName()) {
          dom[bb->getName()] = a;
        }
      }
      // Compute dominator sets.
      bool modified = false;
      do {
        modified = false;
        for (auto bb = F.begin(); bb != F.end(); ++bb) {
          auto intersection = intersectPredecessorsDOM(bb, dom);
          intersection.insert(bb->getName());
          if (intersection != dom[bb->getName()]) {
            dom[bb->getName()] = intersection;
            modified = true;
          }
        }
      } while (modified);
      for (auto i : dom) {
        outs() << i.first << ": ";
        for (auto d : i.second) {
          outs() << d << " ";
        }
        outs() << "\n";
      }
    }
  };
}

char CS201Profiling::ID = 0;
static RegisterPass<CS201Profiling> X("pathProfiling", "CS201Profiling Pass", false, false);

