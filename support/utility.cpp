#include <cstdio>
using namespace std;

extern "C" void outputProfilingResult(
  int* bbCounters,
  int n) {
  for (int i = 0; i < n; ++i) {
    printf("%d ", bbCounters[i]);
  }
  printf("\n");
}

