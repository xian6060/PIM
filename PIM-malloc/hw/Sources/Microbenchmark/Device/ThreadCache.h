#ifndef UMALLOC_DEVICE_THREAD_CACHE_H_
#define UMALLOC_DEVICE_THREAD_CACHE_H_

#include <dpuruntime.h>
#include <mram.h>

#include "BuddyAllocator.h"
#include "Microbenchmark/Device/BuddyAllocator.h"

// NOTE: page header layout in MRAM (32 bytes, multiple of 8 for DMA)
//   [0, 8)   prev, next
//   [8, 24)  128-bit allocation bitmap (1 = free)
//   [24, 32) ownerId, sizeClassIndex
// Pages come from the buddy allocator and are pageSize-aligned relative to
// its region base, so any chunk pointer maps to its header in O(1)
#define THREAD_CACHE_HEADER_SIZE 32
#define THREAD_CACHE_OWNER_OFFSET 24

struct ThreadCache;
struct ThreadCacheNode;

struct ThreadCache {
  struct BuddyAllocator* buddyAllocator;
  int sizeClass;
  int pageSize;
  int ownerId;
  int sizeClassIndex;
  __mram_ptr struct ThreadCacheNode* root;
};

struct ThreadCacheNode {
  // NOTE(bongjoon.hyun@gmail.com): neighbors are prev and next
  __mram_ptr struct ThreadCacheNode* prev;
  __mram_ptr struct ThreadCacheNode* next;

  __mram_ptr int* metadata;

  // NOTE(bongjoon.hyun@gmail.com): data offset is here
};

struct ThreadCache* initThreadCache(struct BuddyAllocator* buddyAllocator,
                                    int sizeClass, int pageSize, int ownerId,
                                    int sizeClassIndex);
void finiThreadCache(struct ThreadCache* threadCache);

__mram_ptr void* allocateThreadCache(struct ThreadCache* threadCache);
int freeThreadCache(struct ThreadCache* threadCache, __mram_ptr void* pointer);

void initThreadCacheNode(struct ThreadCache* threadCache,
                         __mram_ptr struct ThreadCacheNode* node,
                         __mram_ptr struct ThreadCacheNode* prev);
void finiThreadCacheNode(struct ThreadCache* threadCache,
                         __mram_ptr struct ThreadCacheNode* node);

__mram_ptr void* allocateThreadCacheNode(
    struct ThreadCache* threadCache, __mram_ptr struct ThreadCacheNode* node);
int freeThreadCacheNode(struct ThreadCache* threadCache,
                        __mram_ptr struct ThreadCacheNode* node,
                        __mram_ptr void* pointer);

int getThreadCacheNumChunks(struct ThreadCache* threadCache);

int getThreadCacheMetadata(struct ThreadCache* threadCache,
                           __mram_ptr struct ThreadCacheNode* node,
                           int chunkIndex);
int setThreadCacheMetadata(struct ThreadCache* threadCache,
                           __mram_ptr struct ThreadCacheNode* node,
                           int chunkIndex, int value);

__mram_ptr void* getThreadCachePointer(struct ThreadCache* threadCache,
                                       __mram_ptr struct ThreadCacheNode* node,
                                       int chunkIndex);

int getThreadCacheChunkIndex(struct ThreadCache* threadCache,
                             __mram_ptr struct ThreadCacheNode* node,
                             __mram_ptr void* pointer);

// NOTE: batched free of several chunks in one page: mark each chunk into a
// zeroed freedBits, then apply them with a single bitmap read and write
void markThreadCacheChunk(struct ThreadCache* threadCache,
                          __mram_ptr struct ThreadCacheNode* node,
                          __mram_ptr void* pointer, int freedBits[4]);
void freeThreadCacheNodeChunks(struct ThreadCache* threadCache,
                               __mram_ptr struct ThreadCacheNode* node,
                               const int freedBits[4]);

void readThreadCacheNodeOwner(__mram_ptr struct ThreadCacheNode* node,
                              int* ownerId, int* sizeClassIndex);

int countThreadCachePages(struct ThreadCache* threadCache);
int countThreadCacheAllocatedChunks(struct ThreadCache* threadCache);

#endif
