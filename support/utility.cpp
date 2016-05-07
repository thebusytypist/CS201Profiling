#include <cstring>
#include <cstdio>
#include <typeinfo>
#include <algorithm>
using namespace std;

#define SEPARATOR "---------------------------\n"

extern "C" void outputProfilingResult(
  const char** bbFunctionNames,
  const char** bbNames,
  int* bbCounters,
  int* edgeCountersFlat,
  int* backEdgeTails,
  int* backEdgeHeads,
  int n, int nloop) {

  auto edgeCounters = reinterpret_cast<int (*)[n]>(edgeCountersFlat);

  printf("\nBASIC BLOCK PROFILING:\n");
  const char* prev = "";
  for (int id = 1; id < n; ++id) {
    if (strcmp(prev, bbFunctionNames[id]) != 0) {
      printf(SEPARATOR);
      printf("FUNCTION %s\n", bbFunctionNames[id]);
      prev = bbFunctionNames[id];
    }

    printf("%s (ID: %d): %d\n", bbNames[id], id, bbCounters[id]);
  }

  printf("\nEDGE PROFILING:\n");
  prev = "";
  for (int i = 1; i < n; ++i) {
    for (int j = 1; j < n; ++j) {
      if (i == j || edgeCounters[i][j] == 0)
        continue;

      if (strcmp(prev, bbFunctionNames[i]) != 0) {
        printf(SEPARATOR);
        printf("FUNCTION %s\n", bbFunctionNames[i]);
        prev = bbFunctionNames[i];
      }

      printf("%s (ID: %d) -> %s (ID: %d): %d\n",
        bbNames[i], i, bbNames[j], j, edgeCounters[i][j]);
    }
  }

  printf("\nLOOP PROFILING:\n");
  prev = "";
  for (int i = 0; i < nloop; ++i) {
    if (strcmp(prev, bbFunctionNames[backEdgeTails[i]]) != 0) {
      printf(SEPARATOR);
      printf("FUNCTION %s\n", bbFunctionNames[backEdgeTails[i]]);
      prev = bbFunctionNames[backEdgeTails[i]];
    }

    int tail = backEdgeTails[i];
    int head = backEdgeHeads[i];
    int count = min(bbCounters[tail], bbCounters[head]);
    printf("loop%d: %d\n", i, count);
  }
}

