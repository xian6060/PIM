#include "Microbenchmark/Device/uMalloc.h"

#include <alloc.h>
#include <assert.h>
#include <defs.h>
#include <mram.h>
#include <mutex.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

// NOTE: one hardware mutex per owner tasklet guards all of that tasklet's
// thread caches (bitmaps and page lists), because alien flushes from other
// tasklets modify them concurrently with the owner
uint8_t __atomic_bit ownerMutexes[UMALLOC_MAX_THREADS];

static struct UMallocStats *getStats(struct UMalloc *uMalloc) {
  return &uMalloc->stats[me()];
}

static void lockOwner(struct UMalloc *uMalloc, int ownerId) {
  mutex_lock(&ownerMutexes[ownerId]);
  getStats(uMalloc)->ownerLockAcquires++;
}

static void unlockOwner(int ownerId) { mutex_unlock(&ownerMutexes[ownerId]); }

static struct AlienCache *getAlienCache(struct UMalloc *uMalloc, int threadId) {
  if (threadId == 0) {
    return uMalloc->alienCache0;
  } else if (threadId == 1) {
    return uMalloc->alienCache1;
  } else if (threadId == 2) {
    return uMalloc->alienCache2;
  } else if (threadId == 3) {
    return uMalloc->alienCache3;
  } else if (threadId == 4) {
    return uMalloc->alienCache4;
  } else if (threadId == 5) {
    return uMalloc->alienCache5;
  } else if (threadId == 6) {
    return uMalloc->alienCache6;
  } else if (threadId == 7) {
    return uMalloc->alienCache7;
  } else if (threadId == 8) {
    return uMalloc->alienCache8;
  } else if (threadId == 9) {
    return uMalloc->alienCache9;
  } else if (threadId == 10) {
    return uMalloc->alienCache10;
  } else if (threadId == 11) {
    return uMalloc->alienCache11;
  } else if (threadId == 12) {
    return uMalloc->alienCache12;
  } else if (threadId == 13) {
    return uMalloc->alienCache13;
  } else if (threadId == 14) {
    return uMalloc->alienCache14;
  } else if (threadId == 15) {
    return uMalloc->alienCache15;
  } else {
    assert(0 && "threadId > 15");
    return NULL;
  }
}

static int clampAlienFlushThreshold(int alienFlushThreshold) {
  if (alienFlushThreshold < 1) {
    return 1;
  }

  if (alienFlushThreshold > ALIEN_CACHE_SIZE) {
    return ALIEN_CACHE_SIZE;
  }

  return alienFlushThreshold;
}

static void initAlienCache(struct AlienCache *alienCache,
                           int flushThreshold) {
  alienCache->count = 0;
  alienCache->flushThreshold = clampAlienFlushThreshold(flushThreshold);

  for (int i = 0; i < ALIEN_CACHE_SIZE; i++) {
    alienCache->entries[i] = NULL;
  }
}

static void initAlienCaches(struct UMalloc *uMalloc, int flushThreshold) {
  uMalloc->alienCache0 = mem_alloc(sizeof(struct AlienCache));
  uMalloc->alienCache1 = mem_alloc(sizeof(struct AlienCache));
  uMalloc->alienCache2 = mem_alloc(sizeof(struct AlienCache));
  uMalloc->alienCache3 = mem_alloc(sizeof(struct AlienCache));
  uMalloc->alienCache4 = mem_alloc(sizeof(struct AlienCache));
  uMalloc->alienCache5 = mem_alloc(sizeof(struct AlienCache));
  uMalloc->alienCache6 = mem_alloc(sizeof(struct AlienCache));
  uMalloc->alienCache7 = mem_alloc(sizeof(struct AlienCache));
  uMalloc->alienCache8 = mem_alloc(sizeof(struct AlienCache));
  uMalloc->alienCache9 = mem_alloc(sizeof(struct AlienCache));
  uMalloc->alienCache10 = mem_alloc(sizeof(struct AlienCache));
  uMalloc->alienCache11 = mem_alloc(sizeof(struct AlienCache));
  uMalloc->alienCache12 = mem_alloc(sizeof(struct AlienCache));
  uMalloc->alienCache13 = mem_alloc(sizeof(struct AlienCache));
  uMalloc->alienCache14 = mem_alloc(sizeof(struct AlienCache));
  uMalloc->alienCache15 = mem_alloc(sizeof(struct AlienCache));

  initAlienCache(uMalloc->alienCache0, flushThreshold);
  initAlienCache(uMalloc->alienCache1, flushThreshold);
  initAlienCache(uMalloc->alienCache2, flushThreshold);
  initAlienCache(uMalloc->alienCache3, flushThreshold);
  initAlienCache(uMalloc->alienCache4, flushThreshold);
  initAlienCache(uMalloc->alienCache5, flushThreshold);
  initAlienCache(uMalloc->alienCache6, flushThreshold);
  initAlienCache(uMalloc->alienCache7, flushThreshold);
  initAlienCache(uMalloc->alienCache8, flushThreshold);
  initAlienCache(uMalloc->alienCache9, flushThreshold);
  initAlienCache(uMalloc->alienCache10, flushThreshold);
  initAlienCache(uMalloc->alienCache11, flushThreshold);
  initAlienCache(uMalloc->alienCache12, flushThreshold);
  initAlienCache(uMalloc->alienCache13, flushThreshold);
  initAlienCache(uMalloc->alienCache14, flushThreshold);
  initAlienCache(uMalloc->alienCache15, flushThreshold);
}

static struct ThreadCache **getThreadCachesByThreadId(struct UMalloc *uMalloc,
                                                      int threadId) {
  if (threadId == 0) {
    return uMalloc->threadCaches0;
  } else if (threadId == 1) {
    return uMalloc->threadCaches1;
  } else if (threadId == 2) {
    return uMalloc->threadCaches2;
  } else if (threadId == 3) {
    return uMalloc->threadCaches3;
  } else if (threadId == 4) {
    return uMalloc->threadCaches4;
  } else if (threadId == 5) {
    return uMalloc->threadCaches5;
  } else if (threadId == 6) {
    return uMalloc->threadCaches6;
  } else if (threadId == 7) {
    return uMalloc->threadCaches7;
  } else if (threadId == 8) {
    return uMalloc->threadCaches8;
  } else if (threadId == 9) {
    return uMalloc->threadCaches9;
  } else if (threadId == 10) {
    return uMalloc->threadCaches10;
  } else if (threadId == 11) {
    return uMalloc->threadCaches11;
  } else if (threadId == 12) {
    return uMalloc->threadCaches12;
  } else if (threadId == 13) {
    return uMalloc->threadCaches13;
  } else if (threadId == 14) {
    return uMalloc->threadCaches14;
  } else if (threadId == 15) {
    return uMalloc->threadCaches15;
  } else {
    assert(0 && "threadId > 15");
    return NULL;
  }
}

// NOTE: the caller must hold the lock of the owner of threadCaches
static int tryFreeInThreadCaches(struct UMalloc *uMalloc,
                                 struct ThreadCache **threadCaches,
                                 __mram_ptr void *pointer) {
  for (int i = 0; i < uMalloc->sizeClassesLength; i++) {
    getStats(uMalloc)->cacheSearches++;

    if (freeThreadCache(threadCaches[i], pointer)) {
      return 1;
    }
  }

  return 0;
}

static int tryFreeInOwnerThreadCaches(struct UMalloc *uMalloc, int ownerId,
                                      __mram_ptr void *pointer) {
  lockOwner(uMalloc, ownerId);
  int isFreed = tryFreeInThreadCaches(
      uMalloc, getThreadCachesByThreadId(uMalloc, ownerId), pointer);
  unlockOwner(ownerId);

  return isFreed;
}

static int tryFreeInAllThreadCaches(struct UMalloc *uMalloc,
                                    __mram_ptr void *pointer) {
  for (int threadId = 0; threadId < UMALLOC_MAX_THREADS; threadId++) {
    if (tryFreeInOwnerThreadCaches(uMalloc, threadId, pointer)) {
      return 1;
    }
  }

  return 0;
}

static void flushAlienCache(struct UMalloc *uMalloc,
                            struct AlienCache *alienCache) {
  struct UMallocStats *stats = getStats(uMalloc);
  stats->alienFlushes++;

  for (int i = 0; i < alienCache->count; i++) {
    __mram_ptr void *pointer = alienCache->entries[i];

    if (!tryFreeInAllThreadCaches(uMalloc, pointer)) {
      buddyFree(uMalloc->buddyAllocator, pointer);
      stats->buddyFrees++;
    }

    alienCache->entries[i] = NULL;
    stats->flushedPointers++;
  }

  alienCache->count = 0;
}

static void enqueueAlienPointer(struct UMalloc *uMalloc,
                                struct AlienCache *alienCache,
                                __mram_ptr void *pointer) {
  assert(alienCache->count < ALIEN_CACHE_SIZE);

  alienCache->entries[alienCache->count] = pointer;
  alienCache->count++;
  getStats(uMalloc)->alienEnqueues++;

  if (alienCache->count >= alienCache->flushThreshold) {
    flushAlienCache(uMalloc, alienCache);
  }
}

void flushAllAlienCaches(struct UMalloc *uMalloc) {
  for (int threadId = 0; threadId < UMALLOC_MAX_THREADS; threadId++) {
    struct AlienCache *alienCache = getAlienCache(uMalloc, threadId);

    if (alienCache->count > 0) {
      flushAlienCache(uMalloc, alienCache);
    }
  }
}

void flushCurrentThreadAlienCache(struct UMalloc *uMalloc) {
  struct AlienCache *alienCache = getAlienCache(uMalloc, me());

  if (alienCache->count > 0) {
    flushAlienCache(uMalloc, alienCache);
  }
}

// NOTE: must only be called while no tasklet allocates or frees
void getUMallocUsage(struct UMalloc *uMalloc, int *numPages,
                     int *numAllocatedChunks) {
  *numPages = 0;
  *numAllocatedChunks = 0;

  for (int threadId = 0; threadId < UMALLOC_MAX_THREADS; threadId++) {
    struct ThreadCache **threadCaches =
        getThreadCachesByThreadId(uMalloc, threadId);

    for (int i = 0; i < uMalloc->sizeClassesLength; i++) {
      *numPages += countThreadCachePages(threadCaches[i]);
      *numAllocatedChunks += countThreadCacheAllocatedChunks(threadCaches[i]);
    }
  }
}

struct UMalloc *initUMalloc(int mallocableRegionSize, int pageSize,
                            int bufferSize, __mram_ptr void *mramBase,
                            int *sizeClasses, int sizeClassesLength,
                            int alienFlushThreshold) {
  assert(isPowOf2(pageSize));
  assert(sizeClassesLength > 0);

  
  struct UMalloc *uMalloc = mem_alloc(sizeof(struct UMalloc));

  uMalloc->buddyAllocator =
      initBuddyAllocator(mallocableRegionSize, pageSize, bufferSize, mramBase);

  uMalloc->sizeClasses = mem_alloc(sizeClassesLength * sizeof(int));
  uMalloc->sizeClassesLength = sizeClassesLength;

  for (int i = 0; i < sizeClassesLength; i++) {
    uMalloc->sizeClasses[i] = sizeClasses[i];
  }

  uMalloc->pageSize = pageSize;

  uMalloc->threadCaches0 =
      mem_alloc(sizeClassesLength * sizeof(struct ThreadCache *));
  uMalloc->threadCaches1 =
      mem_alloc(sizeClassesLength * sizeof(struct ThreadCache *));
  uMalloc->threadCaches2 =
      mem_alloc(sizeClassesLength * sizeof(struct ThreadCache *));
  uMalloc->threadCaches3 =
      mem_alloc(sizeClassesLength * sizeof(struct ThreadCache *));
  uMalloc->threadCaches4 =
      mem_alloc(sizeClassesLength * sizeof(struct ThreadCache *));
  uMalloc->threadCaches5 =
      mem_alloc(sizeClassesLength * sizeof(struct ThreadCache *));
  uMalloc->threadCaches6 =
      mem_alloc(sizeClassesLength * sizeof(struct ThreadCache *));
  uMalloc->threadCaches7 =
      mem_alloc(sizeClassesLength * sizeof(struct ThreadCache *));
  uMalloc->threadCaches8 =
      mem_alloc(sizeClassesLength * sizeof(struct ThreadCache *));
  uMalloc->threadCaches9 =
      mem_alloc(sizeClassesLength * sizeof(struct ThreadCache *));
  uMalloc->threadCaches10 =
      mem_alloc(sizeClassesLength * sizeof(struct ThreadCache *));
  uMalloc->threadCaches11 =
      mem_alloc(sizeClassesLength * sizeof(struct ThreadCache *));
  uMalloc->threadCaches12 =
      mem_alloc(sizeClassesLength * sizeof(struct ThreadCache *));
  uMalloc->threadCaches13 =
      mem_alloc(sizeClassesLength * sizeof(struct ThreadCache *));
  uMalloc->threadCaches14 =
      mem_alloc(sizeClassesLength * sizeof(struct ThreadCache *));
  uMalloc->threadCaches15 =
      mem_alloc(sizeClassesLength * sizeof(struct ThreadCache *));

  initAlienCaches(uMalloc, alienFlushThreshold);

  uMalloc->stats =
      mem_alloc(UMALLOC_MAX_THREADS * sizeof(struct UMallocStats));
  memset(uMalloc->stats, 0, UMALLOC_MAX_THREADS * sizeof(struct UMallocStats));

  for (int i = 0; i < sizeClassesLength; i++) {
    uMalloc->threadCaches0[i] =
        initThreadCache(uMalloc->buddyAllocator, sizeClasses[i], pageSize);
    uMalloc->threadCaches1[i] =
        initThreadCache(uMalloc->buddyAllocator, sizeClasses[i], pageSize);
    uMalloc->threadCaches2[i] =
        initThreadCache(uMalloc->buddyAllocator, sizeClasses[i], pageSize);
    uMalloc->threadCaches3[i] =
        initThreadCache(uMalloc->buddyAllocator, sizeClasses[i], pageSize);
    uMalloc->threadCaches4[i] =
        initThreadCache(uMalloc->buddyAllocator, sizeClasses[i], pageSize);
    uMalloc->threadCaches5[i] =
        initThreadCache(uMalloc->buddyAllocator, sizeClasses[i], pageSize);
    uMalloc->threadCaches6[i] =
        initThreadCache(uMalloc->buddyAllocator, sizeClasses[i], pageSize);
    uMalloc->threadCaches7[i] =
        initThreadCache(uMalloc->buddyAllocator, sizeClasses[i], pageSize);
    uMalloc->threadCaches8[i] =
        initThreadCache(uMalloc->buddyAllocator, sizeClasses[i], pageSize);
    uMalloc->threadCaches9[i] =
        initThreadCache(uMalloc->buddyAllocator, sizeClasses[i], pageSize);
    uMalloc->threadCaches10[i] =
        initThreadCache(uMalloc->buddyAllocator, sizeClasses[i], pageSize);
    uMalloc->threadCaches11[i] =
        initThreadCache(uMalloc->buddyAllocator, sizeClasses[i], pageSize);
    uMalloc->threadCaches12[i] =
        initThreadCache(uMalloc->buddyAllocator, sizeClasses[i], pageSize);
    uMalloc->threadCaches13[i] =
        initThreadCache(uMalloc->buddyAllocator, sizeClasses[i], pageSize);
    uMalloc->threadCaches14[i] =
        initThreadCache(uMalloc->buddyAllocator, sizeClasses[i], pageSize);
    uMalloc->threadCaches15[i] =
        initThreadCache(uMalloc->buddyAllocator, sizeClasses[i], pageSize);
  }

  return uMalloc;
}

void finiUMalloc(struct UMalloc *uMalloc) {
  flushAllAlienCaches(uMalloc);

  finiBuddyAllocator(uMalloc->buddyAllocator);

  free(uMalloc->alienCache0);
  free(uMalloc->alienCache1);
  free(uMalloc->alienCache2);
  free(uMalloc->alienCache3);
  free(uMalloc->alienCache4);
  free(uMalloc->alienCache5);
  free(uMalloc->alienCache6);
  free(uMalloc->alienCache7);
  free(uMalloc->alienCache8);
  free(uMalloc->alienCache9);
  free(uMalloc->alienCache10);
  free(uMalloc->alienCache11);
  free(uMalloc->alienCache12);
  free(uMalloc->alienCache13);
  free(uMalloc->alienCache14);
  free(uMalloc->alienCache15);

  for (int i = 0; i < uMalloc->sizeClassesLength; i++) {
    finiThreadCache(uMalloc->threadCaches0[i]);
    finiThreadCache(uMalloc->threadCaches1[i]);
    finiThreadCache(uMalloc->threadCaches2[i]);
    finiThreadCache(uMalloc->threadCaches3[i]);
    finiThreadCache(uMalloc->threadCaches4[i]);
    finiThreadCache(uMalloc->threadCaches5[i]);
    finiThreadCache(uMalloc->threadCaches6[i]);
    finiThreadCache(uMalloc->threadCaches7[i]);
    finiThreadCache(uMalloc->threadCaches8[i]);
    finiThreadCache(uMalloc->threadCaches9[i]);
    finiThreadCache(uMalloc->threadCaches10[i]);
    finiThreadCache(uMalloc->threadCaches11[i]);
    finiThreadCache(uMalloc->threadCaches12[i]);
    finiThreadCache(uMalloc->threadCaches13[i]);
    finiThreadCache(uMalloc->threadCaches14[i]);
    finiThreadCache(uMalloc->threadCaches15[i]);
  }
}

__mram_ptr void *allocateUMalloc(struct UMalloc *uMalloc, int size) {
  struct ThreadCache *threadCache = getThreadCache(uMalloc, size);

  if (threadCache != NULL) {
    int threadId = me();

    lockOwner(uMalloc, threadId);
    __mram_ptr void *pointer = allocateThreadCache(threadCache);
    unlockOwner(threadId);

    return pointer;
  } else {
    return buddyAllocate(uMalloc->buddyAllocator, size);
  }
}

void freeUMalloc(struct UMalloc *uMalloc, __mram_ptr void *pointer) {
  int threadId = me();

  if (tryFreeInOwnerThreadCaches(uMalloc, threadId, pointer)) {
    getStats(uMalloc)->localFrees++;
    return;
  }

  enqueueAlienPointer(uMalloc, getAlienCache(uMalloc, threadId), pointer);
}

struct ThreadCache *getThreadCache(struct UMalloc *uMalloc, int size) {
  int threadId = me();

  int sizeClassIndex = -1;
  for (int i = 0; i < uMalloc->sizeClassesLength; i++) {
    if (size <= uMalloc->sizeClasses[i]) {
      sizeClassIndex = i;
      break;
    }
  }

  if (sizeClassIndex == -1) {
    return NULL;
  }

  if (threadId == 0) {
    return uMalloc->threadCaches0[sizeClassIndex];
  } else if (threadId == 1) {
    return uMalloc->threadCaches1[sizeClassIndex];
  } else if (threadId == 2) {
    return uMalloc->threadCaches2[sizeClassIndex];
  } else if (threadId == 3) {
    return uMalloc->threadCaches3[sizeClassIndex];
  } else if (threadId == 4) {
    return uMalloc->threadCaches4[sizeClassIndex];
  } else if (threadId == 5) {
    return uMalloc->threadCaches5[sizeClassIndex];
  } else if (threadId == 6) {
    return uMalloc->threadCaches6[sizeClassIndex];
  } else if (threadId == 7) {
    return uMalloc->threadCaches7[sizeClassIndex];
  } else if (threadId == 8) {
    return uMalloc->threadCaches8[sizeClassIndex];
  } else if (threadId == 9) {
    return uMalloc->threadCaches9[sizeClassIndex];
  } else if (threadId == 10) {
    return uMalloc->threadCaches10[sizeClassIndex];
  } else if (threadId == 11) {
    return uMalloc->threadCaches11[sizeClassIndex];
  } else if (threadId == 12) {
    return uMalloc->threadCaches12[sizeClassIndex];
  } else if (threadId == 13) {
    return uMalloc->threadCaches13[sizeClassIndex];
  } else if (threadId == 14) {
    return uMalloc->threadCaches14[sizeClassIndex];
  } else if (threadId == 15) {
    return uMalloc->threadCaches15[sizeClassIndex];
  } else {
    assert(0 && "threadId > 15");
    return NULL;
  }
}
