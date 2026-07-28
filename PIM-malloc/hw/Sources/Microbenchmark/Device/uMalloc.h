#ifndef UMALLOC_DEVICE_UMALLOC_H_
#define UMALLOC_DEVICE_UMALLOC_H_

#include <mram.h>

#include "Microbenchmark/Device/BuddyAllocator.h"
#include "Microbenchmark/Device/ThreadCache.h"

#define ALIEN_CACHE_SIZE 16

struct AlienCache {
  __mram_ptr void *entries[ALIEN_CACHE_SIZE];
  int count;
  int flushThreshold;
};

struct UMalloc {
  struct BuddyAllocator *buddyAllocator;

  int *sizeClasses;
  int sizeClassesLength;

  int pageSize;

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
};

struct UMalloc *initUMalloc(int mallocableRegionSize, int pageSize,
                            int bufferSize, __mram_ptr void *mramBase,
                            int *sizeClasses, int sizeClassesLength);
void finiUMalloc(struct UMalloc *uMalloc);

__mram_ptr void *allocateUMalloc(struct UMalloc *uMalloc, int size);
void freeUMalloc(struct UMalloc *uMalloc, __mram_ptr void *pointer);
void flushAllAlienCaches(struct UMalloc *uMalloc);

struct ThreadCache *getThreadCache(struct UMalloc *uMalloc, int size);

#endif
