#ifndef UMALLOC_DEVICE_UMALLOC_H_
#define UMALLOC_DEVICE_UMALLOC_H_

#include <mram.h>
#include <stdint.h>

#include "Microbenchmark/Device/BuddyAllocator.h"
#include "Microbenchmark/Device/ThreadCache.h"

#define ALIEN_CACHE_SIZE 64
#define UMALLOC_MAX_THREADS 16

struct AlienCache {
  __mram_ptr void *entries[ALIEN_CACHE_SIZE];
  int count;
  int flushThreshold;
};

// NOTE: per-tasklet event counters; each tasklet only touches its own entry
struct UMallocStats {
  uint32_t localFrees;         // freed into the freeing tasklet's own cache
  uint32_t alienEnqueues;      // pushed into the freeing tasklet's alien cache
  uint32_t alienFlushes;       // alien cache flush events
  uint32_t flushedPointers;    // pointers drained by alien cache flushes
  uint32_t ownerLockAcquires;  // per-owner thread cache lock acquisitions
  uint32_t cacheSearches;      // page header reads to locate a pointer
  uint32_t buddyFrees;         // pointers handed back to the buddy allocator
  uint32_t directRemoteFrees;  // remote pointers freed under the owner lock
                               // without the alien cache (threshold 0)
};

struct UMalloc {
  struct BuddyAllocator *buddyAllocator;

  int *sizeClasses;
  int sizeClassesLength;

  int pageSize;

  // NOTE: 0 frees remote pointers directly under the owner lock and leaves
  // the alien caches unused
  int alienFlushThreshold;

  struct ThreadCache **threadCaches0;
  struct ThreadCache **threadCaches1;
  struct ThreadCache **threadCaches2;
  struct ThreadCache **threadCaches3;
  struct ThreadCache **threadCaches4;
  struct ThreadCache **threadCaches5;
  struct ThreadCache **threadCaches6;
  struct ThreadCache **threadCaches7;
  struct ThreadCache **threadCaches8;
  struct ThreadCache **threadCaches9;
  struct ThreadCache **threadCaches10;
  struct ThreadCache **threadCaches11;
  struct ThreadCache **threadCaches12;
  struct ThreadCache **threadCaches13;
  struct ThreadCache **threadCaches14;
  struct ThreadCache **threadCaches15;

  struct AlienCache *alienCache0;
  struct AlienCache *alienCache1;
  struct AlienCache *alienCache2;
  struct AlienCache *alienCache3;
  struct AlienCache *alienCache4;
  struct AlienCache *alienCache5;
  struct AlienCache *alienCache6;
  struct AlienCache *alienCache7;
  struct AlienCache *alienCache8;
  struct AlienCache *alienCache9;
  struct AlienCache *alienCache10;
  struct AlienCache *alienCache11;
  struct AlienCache *alienCache12;
  struct AlienCache *alienCache13;
  struct AlienCache *alienCache14;
  struct AlienCache *alienCache15;

  struct UMallocStats *stats;
};

struct UMalloc *initUMalloc(int mallocableRegionSize, int pageSize,
                            int bufferSize, __mram_ptr void *mramBase,
                            int *sizeClasses, int sizeClassesLength,
                            int alienFlushThreshold);
void finiUMalloc(struct UMalloc *uMalloc);

__mram_ptr void *allocateUMalloc(struct UMalloc *uMalloc, int size);
void freeUMalloc(struct UMalloc *uMalloc, __mram_ptr void *pointer);
void flushAllAlienCaches(struct UMalloc *uMalloc);
void flushCurrentThreadAlienCache(struct UMalloc *uMalloc);

void getUMallocUsage(struct UMalloc *uMalloc, int *numPages,
                     int *numAllocatedChunks);

struct ThreadCache *getThreadCache(struct UMalloc *uMalloc, int size);

#endif
