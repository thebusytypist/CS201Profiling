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
using std::pair;
using std::make_pair;

static const char* SEPARATOR = "===========================\n";
static const char* SEPARATOR2 = "---------------------------\n";

cl::opt<bool> dumpBasicBlock(
  "dumpbb",
  cl::desc("Dump basic block textural IR."));

namespace {
  struct CS201Profiling : public FunctionPass {
    static char ID;
    CS201Profiling() : FunctionPass(ID) {}

    //----------------------------------
    bool doInitialization(Module &M) override {
      context = &M.getContext();

      // Initialize frequently used constants.
      zero32 = ConstantInt::get(*context, APInt(32, StringRef("0"), 10));

      // Preprocess all modules to compute the number of counters.
      preprocessModule(M);

      // Allocate global variables(e.g. counters, names).
      allocateGlobalVariables(M);

      // Allocate static strings.
      allocateStaticStrings(M);
      
      // Declare external function to output profiling results.
      std::vector<Type*> outputArgTypes;
      outputArgTypes.push_back(Type::getInt8PtrTy(*context)->getPointerTo());
      outputArgTypes.push_back(Type::getInt8PtrTy(*context)->getPointerTo());
      outputArgTypes.push_back(Type::getInt32PtrTy(*context));
      outputArgTypes.push_back(Type::getInt32PtrTy(*context));
      outputArgTypes.push_back(Type::getInt32PtrTy(*context));
      outputArgTypes.push_back(Type::getInt32PtrTy(*context));
      outputArgTypes.push_back(Type::getInt32Ty(*context));
      outputArgTypes.push_back(Type::getInt32Ty(*context));
      
      FunctionType* outputType = FunctionType::get(
        Type::getVoidTy(*context),
        outputArgTypes,
        false);
      outputFunction = Function::Create(
        outputType,
        Function::ExternalLinkage,
        Twine("outputProfilingResult"),
        &M);
      outputFunction->setCallingConv(CallingConv::C);
      
      return false;
    }

    //----------------------------------
    bool doFinalization(Module &M) override {
      outs() << "\nEND OF ANALYSIS\n\n";
      return false;
    }
    
    //----------------------------------
    bool runOnFunction(Function &F) override {
      functionName = F.getName();
      outs() << SEPARATOR;
      outs() << "FUNCTION: " << functionName << "\n";

      preprocessFunction(F);
      computeLoops(F);

      // Display basic blocks and their predecessors.
      outs() << SEPARATOR2 << "BASIC BLOCKS: " << F.size() << "\n";
      for (auto bb = F.begin(); bb != F.end(); ++bb) {
        outs() << bb->getName() << " ";
        outs() << "(preds: ";
        for (auto pred : preds[bb->getName()]) {
          outs() << pred << " ";
        }
        outs() << ")\n";

        if (dumpBasicBlock)
          bb->dump();
      }

      instrumentFunction(F);

      if (F.getName() == "main") {
        instrumentMainFunction(F);
      }

      currentLoopID = loops.size();

      return true;
    }
  
  private:
    Constant* zero32;

    LLVMContext* context;

    GlobalVariable* bbNameArray;
    GlobalVariable* bbFunctionNameArray;

    GlobalVariable* lastBB;
    GlobalVariable* bbCounters;
    GlobalVariable* edgeCounters;

    std::vector<GlobalVariable*> bbNames;
    std::vector<GlobalVariable*> functionNames;

    GlobalVariable* backEdgeHeads;
    GlobalVariable* backEdgeTails;

    Function* outputFunction;
    
    // <functionName, bbName> -> bbID
    std::map<pair<StringRef, StringRef>, int> bbID;
    std::map<int, StringRef> invbbID;
    std::vector<std::set<int>> loops;
    std::vector<int> tails, heads;
    int currentLoopID;

    StringRef functionName;
    std::map<StringRef, std::vector<StringRef>> preds;

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

    void allocateGlobalVariables(Module& M) {
      int n = bbID.size();

      // Variable to keep track of the last executed basic block.
      lastBB = new GlobalVariable(
        M,
        Type::getInt32Ty(*context),
        false,
        GlobalValue::ExternalLinkage,
        ConstantInt::get(Type::getInt32Ty(*context), 0),
        "lastBB");

      // Define types.
      ArrayType* Int1D = ArrayType::get(IntegerType::get(*context, 32), n);
      ArrayType* Int2D = ArrayType::get(Int1D, n);
      PointerType* CharPtr = PointerType::get(
        IntegerType::get(*context, 8), 0);
      ArrayType* CharPtr1D = ArrayType::get(CharPtr, n);

      // Global variable initializers.
      ConstantAggregateZero* init1D = ConstantAggregateZero::get(Int1D);
      ConstantAggregateZero* init2D = ConstantAggregateZero::get(Int2D);
      ConstantAggregateZero* initCharPtr1D =
        ConstantAggregateZero::get(CharPtr1D);

      // Define gloal variables.
      bbNameArray = new GlobalVariable(
        M,
        CharPtr1D,
        false,
        GlobalValue::ExternalLinkage,
        initCharPtr1D,
        "bbNames");

      bbFunctionNameArray = new GlobalVariable(
        M,
        CharPtr1D,
        false,
        GlobalValue::ExternalLinkage,
        initCharPtr1D,
        "bbFunctionNames");

      bbCounters = new GlobalVariable(
        M,
        Int1D,
        false,
        GlobalValue::ExternalLinkage,
        init1D,
        "bbCounters");

      edgeCounters = new GlobalVariable(
        M,
        Int2D,
        false,
        GlobalValue::ExternalLinkage,
        init2D,
        "edgeCounters");

      backEdgeHeads = new GlobalVariable(
        M,
        Int1D,
        false,
        GlobalValue::ExternalLinkage,
        init1D,
        "backEdgeHeads");

      backEdgeTails = new GlobalVariable(
        M,
        Int1D,
        false,
        GlobalValue::ExternalLinkage,
        init1D,
        "backEdgeTails");
    }

    void allocateStaticStrings(Module& M) {
      // Allocate basic block and function names.
      bbNames.resize(bbID.size());
      functionNames.resize(bbID.size());
      for (auto f = M.begin(); f != M.end(); ++f) {
        for (auto bb = f->begin(); bb != f->end(); ++bb) {
          int id = bbID[make_pair(f->getName(), bb->getName())];
          bbNames[id] = createStaticString(M, bb->getName().data());
          functionNames[id] = createStaticString(M, f->getName().data());
        }
      }
    }

    Constant* indexArray1D(GlobalVariable* arr, int i) {
      std::vector<Constant*> indices;
      indices.push_back(zero32);
      ConstantInt* ci = ConstantInt::get(*context,
        APInt(64, i, 10));
      indices.push_back(ci);
      return ConstantExpr::getGetElementPtr(arr, indices);
    }

    Value* loadAndCastInt(IRBuilder<>& builder, GlobalVariable* v) {
      // Load and cast integer to index array.
      Value* loaded = builder.CreateLoad(v);
      Value* r = builder.CreateSExt(loaded, IntegerType::get(*context, 32));
      return r;
    }

    Value* indexArray2D(
      IRBuilder<>& builder,
      GlobalVariable* arr,
      Value* i, int j) {
      // Use indirect indexing for the first dimension.
      std::vector<Value*> indices;
      indices.push_back(zero32);
      indices.push_back(i);
      ConstantInt* ci = ConstantInt::get(*context,
        APInt(64, j, 10));
      indices.push_back(ci);
      return builder.CreateGEP(arr, indices);
    }

    Constant* indexArray2D(GlobalVariable* arr, int i, int j) {
      std::vector<Constant*> indices;
      indices.push_back(zero32);
      ConstantInt* ci = ConstantInt::get(*context,
        APInt(64, i, 10));
      ConstantInt* cj = ConstantInt::get(*context,
        APInt(64, j, 10));
      indices.push_back(ci);
      indices.push_back(cj);
      return ConstantExpr::getGetElementPtr(arr, indices);
    }

    void increaseCounter(IRBuilder<>& builder, Value* value) {
      Value* loaded = builder.CreateLoad(value);
      Value* added = builder.CreateAdd(
        ConstantInt::get(Type::getInt32Ty(*context), 1), loaded);
      builder.CreateStore(added, value);
    }

    void invokeDisplay(IRBuilder<>& builder) {
      Constant* pbbFunctionNames = indexArray1D(bbFunctionNameArray, 0);
      Constant* pbbNames = indexArray1D(bbNameArray, 0);
      Constant* pbbCounters = indexArray1D(bbCounters, 0);
      Constant* pedgeCounters = indexArray2D(edgeCounters, 0, 0);
      Constant* pbackEdgeTails = indexArray1D(backEdgeTails, 0);
      Constant* pbackEdgeHeads = indexArray1D(backEdgeHeads, 0);

      ConstantInt* n = ConstantInt::get(*context,
        APInt(32, bbID.size(), 10));

      ConstantInt* nloop = ConstantInt::get(*context,
        APInt(32, loops.size(), 10));

      std::vector<Value*> args;
      args.push_back(pbbFunctionNames);
      args.push_back(pbbNames);
      args.push_back(pbbCounters);
      args.push_back(pedgeCounters);
      args.push_back(pbackEdgeTails);
      args.push_back(pbackEdgeHeads);
      args.push_back(n);
      args.push_back(nloop);

      CallInst* call = builder.CreateCall(
        outputFunction, args, "");
      call->setTailCall(false);
    }

    void instrumentFunction(Function& F) {
      for (auto bb = F.begin(); bb != F.end(); ++bb) {
        int id = bbID[make_pair(functionName, bb->getName())];

        IRBuilder<> builder(
          bb->getFirstInsertionPt());

        // Update basic block counter.
        increaseCounter(builder, indexArray1D(bbCounters, id));

        // Update edge counter.
        Value* i = loadAndCastInt(builder, lastBB);
        Value* edge = indexArray2D(builder, edgeCounters, i, id);
        increaseCounter(builder, edge);

        // Update last executed basic block.
        ConstantInt* n = ConstantInt::get(*context, APInt(32, id, 10));
        builder.CreateStore(n, lastBB);
      }
    }

    void instrumentMainFunction(Function& F) {
      for (auto bb = F.begin(); bb != F.end(); ++bb) {
        if (isa<ReturnInst>(bb->getTerminator())) {
          // Insert print functions just before the exit of program.
          IRBuilder<> builder(bb->getTerminator());
          buildNameArrays(builder);
          buildLoops(builder);
          invokeDisplay(builder);
          break;
        }
      }
    }
    
    void buildLoops(IRBuilder<>& builder) {
      for (int i = 0; i < (int)loops.size(); ++i) {
        int tail = tails[i];
        int head = heads[i];

        builder.CreateStore(
          ConstantInt::get(*context, APInt(32, tail, 10)),
          indexArray1D(backEdgeTails, i));

        builder.CreateStore(
          ConstantInt::get(*context, APInt(32, head, 10)),
          indexArray1D(backEdgeHeads, i));
      }
    }

    void buildNameArrays(IRBuilder<>& builder) {
      for (auto x : bbID) {
        int id = x.second;
        if (id == 0) {
          // Omit the dummy root node.
          continue;
        }

        builder.CreateStore(
          indexArray1D(functionNames[id], 0),
          indexArray1D(bbFunctionNameArray, id));
        builder.CreateStore(
          indexArray1D(bbNames[id], 0),
          indexArray1D(bbNameArray, id));
      }
    }
    
    void preprocessModule(Module& M) {
      currentLoopID = 0;
      loops.clear();
      tails.clear();
      heads.clear();

      bbID.clear();
      invbbID.clear();
      int id = 0;

      // Add a dummy node here.
      // This makes edge counting easier
      // since we do not need to consider the root node case.
      invbbID[id] = "";
      bbID[make_pair("", "")] = id++;

      for (auto f = M.begin(); f != M.end(); ++f) {
        for (auto bb = f->begin(); bb != f->end(); ++bb) {
          auto k = make_pair(f->getName(), bb->getName());
          invbbID[id] = bb->getName();
          bbID[k] = id++;
        }
      }
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

      // Output dominator sets.
      outs() << SEPARATOR2 << "DOMINATOR SETS:\n";
      for (auto bb : dom) {
        outs() << bb.first << " => ";
        for (auto d : bb.second) {
          outs() << d << ", ";
        }
        outs() << "\n";
      }
      
      return dom;
    }

    std::set<int> computeLoop(const StringRef& s, const StringRef& t) {
      std::set<int> r;
      r.insert(bbID[make_pair(functionName, t)]);

      std::vector<int> stack;
      stack.push_back(bbID[make_pair(functionName, s)]);

      while (!stack.empty()) {
        int u = stack.back();
        stack.pop_back();
        r.insert(u);
        for (auto pred : preds[invbbID[u]]) {
          int id = bbID[make_pair(functionName, pred)];
          if (r.find(id) == r.end()) {
            stack.push_back(id);
          }
        }
      }
      return r;
    }

    void computeLoops(Function& F) {
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

            tails.push_back(bbID[make_pair(functionName, bb->getName())]);
            heads.push_back(bbID[make_pair(functionName, head)]);
            loops.push_back(computeLoop(bb->getName(), head));
          }
        }
      }

      // Output loops.
      outs() << SEPARATOR2 << "LOOPS: "
        << loops.size() - currentLoopID << "\n";
      for (int j = currentLoopID, size = loops.size(); j < size; ++j) {
        outs() << "loop" << j << ": ";
        for (auto i : loops[j]) {
          outs() << invbbID[i] << ", ";
        }
        outs() << "\n";
      }
    }
  };
}

char CS201Profiling::ID = 0;
static RegisterPass<CS201Profiling> X("pathProfiling", "CS201Profiling Pass", false, false);

