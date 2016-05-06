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
#include <memory>
using namespace llvm;
using std::unique_ptr;
using std::pair;
using std::make_pair;

 
namespace {
  struct CS201Profiling : public FunctionPass {
    static char ID;
    CS201Profiling() : FunctionPass(ID) {}

    //----------------------------------
    bool doInitialization(Module &M) override {
      context = &M.getContext();

      // Initialize frequently used constants.
      zero = ConstantInt::get(*context, APInt(32, StringRef("0"), 10));

      // Preprocess all modules to compute the number of counters.
      preprocessModule(M);

      // Allocate counters.
      allocateCounters(M);

      counter = new GlobalVariable(
        M,
        Type::getInt32Ty(*context),
        false,
        GlobalValue::InternalLinkage,
        ConstantInt::get(Type::getInt32Ty(*context), 0),
        "counter");
      
      formatStr = createStaticString(M, "Counter: %d\n");
      
      // Declare external function printf.
      std::vector<Type*> argTypes;
      argTypes.push_back(Type::getInt8PtrTy(*context));

      FunctionType* type = FunctionType::get(
        Type::getInt32Ty(*context),
        argTypes,
        true);
      printfFunction = M.getFunction("printf");
      if (!printfFunction) {
        printfFunction = Function::Create(
          type,
          Function::ExternalLinkage,
          Twine("printf"),
          &M);
      }
      printfFunction->setCallingConv(CallingConv::C);
      
      return false;
    }

    //----------------------------------
    bool doFinalization(Module &M) override {
      return false;
    }
    
    //----------------------------------
    bool runOnFunction(Function &F) override {
      functionName = F.getName();
      outs() << functionName << "\n";
      
      preprocessFunction(F);
      computeLoops(F);

      if (F.getName() == "main") {
        for (auto bb = F.begin(); bb != F.end(); ++bb) {
          if (isa<ReturnInst>(bb->getTerminator())) {
            // Insert print functions just before the exit of program.
            IRBuilder<> builder(bb->getTerminator());
            instrumentDisplay(builder);
            break;
          }
        }
      }

      return true;
    }
  
  private:
    Constant* zero;

    LLVMContext* context;

    GlobalVariable* counter;
    GlobalVariable* bbCounters;
    GlobalVariable* formatStr;

    Function* printfFunction;
    
    // <functionName, bbName> -> bbID
    std::map<pair<StringRef, StringRef>, int> bbID;

    StringRef functionName;
    std::map<StringRef, std::vector<StringRef>> preds;
    std::vector<std::set<StringRef>> loops;

    GlobalVariable* createStaticString(Module& M, const char* text) {
      // Define format string for printf.
      Constant* value = ConstantDataArray::getString(*context, text);
      auto r = new GlobalVariable(
        M,
        ArrayType::get(IntegerType::get(*context, 8), strlen(text) + 1),
        true,
        GlobalValue::PrivateLinkage,
        value,
        "staticstr");
      return r;
    }

    void allocateCounters(Module& M) {
      int n = bbID.size();

      // Define types.
      ArrayType* Int1D = ArrayType::get(IntegerType::get(*context, 32), n);

      // Global variable initializers.
      ConstantAggregateZero* init1D = ConstantAggregateZero::get(Int1D);

      // Define gloal variables(counters).
      bbCounters = new GlobalVariable(
        M,
        Int1D,
        false,
        GlobalValue::ExternalLinkage,
        init1D,
        "bbCounters");
    }

    void increaseCounter(BasicBlock& bb, Value* value) {
      IRBuilder<> builder(
        bb.getFirstInsertionPt());

      Value* loaded = builder.CreateLoad(value);
      Value* added = builder.CreateAdd(
        ConstantInt::get(Type::getInt32Ty(*context), 1), loaded);
      builder.CreateStore(added, value);
    }

    void invokePrint(
      IRBuilder<>& builder,
      GlobalVariable* fmt,
      const std::vector<Value*>& args) {
      std::vector<Constant*> indices;
      indices.push_back(zero);
      indices.push_back(zero);
      Constant* c = ConstantExpr::getGetElementPtr(fmt, indices);
      
      // Push the format string.
      std::vector<Value*> loadedArgs;
      loadedArgs.push_back(c);
      // Load all other arguments.
      for (auto arg : args) {
        Value* x = builder.CreateLoad(arg);
        loadedArgs.push_back(x);
      }

      CallInst* call = builder.CreateCall(
        printfFunction, loadedArgs, "call");
      call->setTailCall(false);
    }

    void instrumentDisplay(IRBuilder<>& builder) {
      std::vector<Value*> args;
      args.push_back(counter);
      invokePrint(builder, formatStr, args);
    }
    
    void preprocessModule(Module& M) {
      bbID.clear();
      int id = 0;
      for (auto f = M.begin(); f != M.end(); ++f) {
        for (auto bb = f->begin(); bb != f->end(); ++bb) {
          auto k = make_pair(f->getName(), bb->getName());
          bbID[k] = id++;
        }
      }
      outs() << "Total number of BB: " << id << "\n";
    }

    void preprocessFunction(Function& F) {
      preds.clear();
      for (auto bb = F.begin(); bb != F.end(); ++bb) {
        preds[bb->getName()] = std::vector<StringRef>();
      }

      // Construct predecessors map.
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
      // Traverse all predessors of one basic block
      // and do the intersection.
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
    
    DomSet computeDOMSet(Function& F) {
      // Initialize dominator sets.
      DomSet dom;
      std::set<StringRef> a;
      for (auto bb = F.begin(); bb != F.end(); ++bb) {
        if (bb->getName() == F.getEntryBlock().getName()) {
          // The entry block dominates itself.
          std::set<StringRef> s;
          s.insert(bb->getName());
          dom[bb->getName()] = s;
        }
        // Compute the set of all blocks.
        a.insert(bb->getName());
      }
      for (auto bb = F.begin(); bb != F.end(); ++bb) {
        if (bb->getName() != F.getEntryBlock().getName()) {
          // All blocks except the entry
          // gets the set of all blocks.
          dom[bb->getName()] = a;
        }
      }
      // Compute dominator sets iteratively
      // until no modification is made.
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
      
      return dom;
    }

    std::set<StringRef> computeLoop(const StringRef& s, const StringRef& t) {
      std::set<StringRef> r;
      r.insert(t);

      std::vector<StringRef> stack;
      stack.push_back(s);

      while (!stack.empty()) {
        StringRef u = stack.back();
        stack.pop_back();
        r.insert(u);
        for (auto pred : preds[u]) {
          if (r.find(pred) == r.end()) {
            stack.push_back(pred);
          }
        }
      }
      return r;
    }

    void computeLoops(Function& F) {
      loops.clear();

      DomSet dom = computeDOMSet(F);
      // Find back edges.
      for (auto bb = F.begin(); bb != F.end(); ++bb) {
        auto t = bb->getTerminator();
        int n = t->getNumSuccessors();
        for (int i = 0; i < n; ++i) {
          StringRef head = t->getSuccessor(i)->getName();
          auto d = dom[bb->getName()];
          if (d.find(head) != d.end()) {
            outs() << bb->getName() << " -> " << head << "\n";
            loops.push_back(computeLoop(bb->getName(), head));
          }
        }
      }
      outs() << "loops:\n";
      for (auto l : loops) {
        for (auto i : l) {
          outs() << i << " ";
        }
        outs() << "\n";
      }
    }
  };
}

char CS201Profiling::ID = 0;
static RegisterPass<CS201Profiling> X("pathProfiling", "CS201Profiling Pass", false, false);

