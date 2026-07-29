#include <alloc.h>
#include <assert.h>
#include <barrier.h>
#include <buddy_alloc.h>
#include <defs.h>
#include <mram.h>
#include <perfcounter.h>
#include <stdio.h>

#include "Common/Device/Timer.h"
#include "Microbenchmark/Device/uMalloc.h"

#ifndef NR_TASKLETS
#define NR_TASKLETS 16
#endif

__host int mallocableRegionSize;
__host int pageSize;
__host int bufferSize;
__host int alienCacheFlushThreshold;

struct UMalloc *uMalloc;
__mram_ptr void *sharedPointers[NR_TASKLETS];

// Performance counters
__host uint64_t totalCycles = 0;
__host int32_t crossThreadFrees = 0;
__host int32_t alienCacheFlushes = 0;

BARRIER_INIT(deviceBarrier, NR_TASKLETS);

int main() {
  int threadId = me();

  if (threadId == 0) {
    mem_reset();
    perfcounter_config(COUNT_CYCLES, true);

    int sizeClasses[8] = {16, 32, 64, 128, 256, 512, 1024, 2048};
    int sizeClassesLength = 8;

    uMalloc =
        initUMalloc(mallocableRegionSize, pageSize, bufferSize,
            DPU_MRAM_HEAP_POINTER, &sizeClasses, sizeClassesLength,
            alienCacheFlushThreshold);
  }
  barrier_wait(&deviceBarrier);

  struct Timer timer = {0, 0};

  startTimer(&timer);

  sharedPointers[threadId] = allocateUMalloc(uMalloc, 32);
  barrier_wait(&deviceBarrier);

  int victimThreadId = (threadId + 1) % NR_TASKLETS;
  freeUMalloc(uMalloc, sharedPointers[victimThreadId]);
  barrier_wait(&deviceBarrier);

  if (threadId == 0) {
    flushAllAlienCaches(uMalloc);
  }
  barrier_wait(&deviceBarrier);

  stopTimer(&timer);
  // Tasklet 0 prints device-side timing statistics
  if (threadId == 0) {
    printf("[DPU] Timer cycles: %lu\n", getTime(&timer));
    // Optionally add more counters here if tracked
  }

  return 0;
}
