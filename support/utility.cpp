#include <cstring>
#include <cstdio>
using namespace std;

#define SEPARATOR "---------------------------\n"

extern "C" void outputProfilingResult(
  const char** bbFunctionNames,
  const char** bbNames,
  int* bbCounters,
  int n) {

  printf("\nBASIC BLOCK PROFILING\n");
  const char* prev = "";
  for (int id = 1; id < n; ++id) {
    if (strcmp(prev, bbFunctionNames[id]) != 0) {
      printf(SEPARATOR);
      printf("FUNCTION %s\n", bbFunctionNames[id]);
      prev = bbFunctionNames[id];
    }

    printf("%s: %d\n", bbNames[id], bbCounters[id]);
  }
}

