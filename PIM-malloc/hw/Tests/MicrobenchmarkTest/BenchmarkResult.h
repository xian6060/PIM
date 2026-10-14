#ifndef MICROBENCHMARK_TEST_BENCHMARK_RESULT_H_
#define MICROBENCHMARK_TEST_BENCHMARK_RESULT_H_

#include <stdint.h>

// NOTE: shared by the host and the DPU program; every field is 64-bit so both
// compilers produce the same layout for dpu_copy_from
struct BenchmarkResult {
  // DPU cycles, measured by tasklet 0 between barriers
  uint64_t totalCycles;
  uint64_t allocCycles;
  uint64_t freeCycles;
  uint64_t drainCycles;

  // uMalloc event counters summed over all tasklets
  uint64_t localFrees;
  uint64_t alienEnqueues;
  uint64_t alienFlushes;
  uint64_t flushedPointers;
  uint64_t ownerLockAcquires;
  uint64_t cacheSearches;
  uint64_t buddyFrees;

  // allocator state after the final drain
  uint64_t numPages;
  uint64_t leakedChunks;

  // tag mismatches observed when validation is enabled
  uint64_t validationErrors;

  // free-path bitmap read-modify-writes (one per page run in alien flushes)
  uint64_t bitmapUpdates;
};

#endif
