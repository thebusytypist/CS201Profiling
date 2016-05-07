INPUT=${1}
LLVM_HOME=~/Workspace
PREFIX=Release+Debug+Asserts
if [ $(uname -s) == "Darwin" ]; then
    SHARED_LIB_EXT=dylib;
else
    SHARED_LIB_EXT=so;
fi

clang -emit-llvm support/${INPUT}.c -c -o support/${INPUT}.bc && \
    make clean && \
    make && \
    ${LLVM_HOME}/llvm/${PREFIX}/bin/clang++ -std=c++11 -c -emit-llvm -o support/utility.bc support/utility.cpp && \
    ${LLVM_HOME}/llvm/${PREFIX}/bin/opt -load ../../../${PREFIX}/lib/CS201Profiling.${SHARED_LIB_EXT} -pathProfiling support/${INPUT}.bc -S -o support/${INPUT}.ll && \
    ${LLVM_HOME}/llvm/${PREFIX}/bin/llvm-as support/${INPUT}.ll -o support/${INPUT}.bb.bc && \
    ${LLVM_HOME}/llvm/${PREFIX}/bin/llvm-link support/${INPUT}.bb.bc support/utility.bc -o support/${INPUT}.main.bc && \
    ${LLVM_HOME}/llvm/${PREFIX}/bin/lli support/${INPUT}.main.bc

