#include <alloc.h>
#include <assert.h>
#include <barrier.h>
#include <defs.h>
#include <mram.h>
#include <perfcounter.h>
#include <stdint.h>
#include <stdio.h>

#include "Microbenchmark/Device/uMalloc.h"
#include "MicrobenchmarkTest/BenchmarkResult.h"

#ifndef NR_TASKLETS
#define NR_TASKLETS 16
#endif

#define MAX_BATCH_SIZE 128

// NOTE: inputs written by the host before every launch
__host int mallocableRegionSize;
__host int pageSize;
__host int bufferSize;
__host int alienCacheFlushThreshold;
__host int allocSize;
__host int batchSize;
__host int numRounds;
__host int localFreePercent;
__host int remotePattern;
__host int validate;

// NOTE: who frees the remote part of a batch
//   neighbor: pointer i of tasklet t is freed by tasklet t - 1
//   scatter:  pointer i of tasklet t is freed by tasklet t - 1 - i % 15, so
//             each tasklet frees pointers owned by all 15 other tasklets
#define REMOTE_PATTERN_NEIGHBOR 0
#define REMOTE_PATTERN_SCATTER 1

__host struct BenchmarkResult result;

struct UMalloc *uMalloc;
__mram_ptr void *sharedPointers[NR_TASKLETS][MAX_BATCH_SIZE];
uint8_t scatterVictims[NR_TASKLETS][MAX_BATCH_SIZE];
uint32_t validationErrors[NR_TASKLETS];

BARRIER_INIT(deviceBarrier, NR_TASKLETS);

static uint64_t makeTag(int threadId, int round, int index) {
  return ((uint64_t)0xA11C << 48) | ((uint64_t)threadId << 32) |
         ((uint64_t)round << 16) | (uint64_t)index;
}

static void collectResult(uint64_t allocCycles, uint64_t freeCycles,
                          uint64_t drainCycles) {
  result.allocCycles = allocCycles;
  result.freeCycles = freeCycles;
  result.drainCycles = drainCycles;
  result.totalCycles = allocCycles + freeCycles + drainCycles;

  result.localFrees = 0;
  result.alienEnqueues = 0;
  result.alienFlushes = 0;
  result.flushedPointers = 0;
  result.ownerLockAcquires = 0;
  result.cacheSearches = 0;
  result.buddyFrees = 0;
  result.directRemoteFrees = 0;
  result.validationErrors = 0;

  for (int threadId = 0; threadId < NR_TASKLETS; threadId++) {
    struct UMallocStats *stats = &uMalloc->stats[threadId];

    result.localFrees += stats->localFrees;
    result.alienEnqueues += stats->alienEnqueues;
    result.alienFlushes += stats->alienFlushes;
    result.flushedPointers += stats->flushedPointers;
    result.ownerLockAcquires += stats->ownerLockAcquires;
    result.cacheSearches += stats->cacheSearches;
    result.buddyFrees += stats->buddyFrees;
    result.directRemoteFrees += stats->directRemoteFrees;
    result.validationErrors += validationErrors[threadId];
  }

  int numPages;
  int numAllocatedChunks;
  getUMallocUsage(uMalloc, &numPages, &numAllocatedChunks);

  result.numPages = numPages;
  result.leakedChunks = numAllocatedChunks;
}

int main() {
  int threadId = me();

  if (threadId == 0) {
    mem_reset();

    assert(0 < batchSize && batchSize <= MAX_BATCH_SIZE);
    assert(0 <= localFreePercent && localFreePercent <= 100);
    assert(allocSize >= 8);
    assert(remotePattern == REMOTE_PATTERN_NEIGHBOR ||
           remotePattern == REMOTE_PATTERN_SCATTER);

    int sizeClasses[8] = {16, 32, 64, 128, 256, 512, 1024, 2048};
    int sizeClassesLength = 8;

    uMalloc = initUMalloc(mallocableRegionSize, pageSize, bufferSize,
                          DPU_MRAM_HEAP_POINTER, sizeClasses,
                          sizeClassesLength, alienCacheFlushThreshold);

    perfcounter_config(COUNT_CYCLES, true);
  }
  validationErrors[threadId] = 0;

  // NOTE: precomputed before timing starts so the timed loop avoids the
  // DPU's software modulo
  for (int i = 0; i < MAX_BATCH_SIZE; i++) {
    scatterVictims[threadId][i] =
        (threadId + 1 + i % (NR_TASKLETS - 1)) % NR_TASKLETS;
  }
  barrier_wait(&deviceBarrier);

  // NOTE: the first numLocalFrees pointers of every batch are freed by their
  // owner and the rest by another tasklet (see remotePattern), so each
  // pointer is freed exactly once
  int numLocalFrees = batchSize * localFreePercent / 100;
  int victimThreadId = (threadId + 1) % NR_TASKLETS;

  perfcounter_t phaseStart = 0;
  uint64_t allocCycles = 0;
  uint64_t freeCycles = 0;

  for (int round = 0; round < numRounds; round++) {
    if (threadId == 0) {
      phaseStart = perfcounter_get();
    }

    for (int i = 0; i < batchSize; i++) {
      __mram_ptr void *pointer = allocateUMalloc(uMalloc, allocSize);
      sharedPointers[threadId][i] = pointer;

      if (validate) {
        uint64_t tag __dma_aligned = makeTag(threadId, round, i);
        mram_write(&tag, pointer, sizeof(tag));
      }
    }
    barrier_wait(&deviceBarrier);

    if (threadId == 0) {
      allocCycles += perfcounter_get() - phaseStart;
    }

    // NOTE: overlapping allocations show up as overwritten tags
    if (validate) {
      for (int i = 0; i < batchSize; i++) {
        uint64_t tag __dma_aligned;
        mram_read(sharedPointers[threadId][i], &tag, sizeof(tag));

        if (tag != makeTag(threadId, round, i)) {
          validationErrors[threadId]++;
        }
      }
      barrier_wait(&deviceBarrier);
    }

    if (threadId == 0) {
      phaseStart = perfcounter_get();
    }

    if (remotePattern == REMOTE_PATTERN_SCATTER) {
      for (int i = 0; i < batchSize; i++) {
        if (i < numLocalFrees) {
          freeUMalloc(uMalloc, sharedPointers[threadId][i]);
        } else {
          freeUMalloc(uMalloc,
                      sharedPointers[scatterVictims[threadId][i]][i]);
        }
      }
    } else {
      for (int i = 0; i < batchSize; i++) {
        if (i < numLocalFrees) {
          freeUMalloc(uMalloc, sharedPointers[threadId][i]);
        } else {
          freeUMalloc(uMalloc, sharedPointers[victimThreadId][i]);
        }
      }
    }
    barrier_wait(&deviceBarrier);

    if (threadId == 0) {
      freeCycles += perfcounter_get() - phaseStart;
    }
  }

  if (threadId == 0) {
    phaseStart = perfcounter_get();
  }

  flushCurrentThreadAlienCache(uMalloc);
  barrier_wait(&deviceBarrier);

  if (threadId == 0) {
    uint64_t drainCycles = perfcounter_get() - phaseStart;

    collectResult(allocCycles, freeCycles, drainCycles);
  }

  return 0;
}
