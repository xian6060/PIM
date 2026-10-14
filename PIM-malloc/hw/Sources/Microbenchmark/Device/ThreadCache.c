#include "Microbenchmark/Device/ThreadCache.h"

#include <alloc.h>
#include <assert.h>
#include <mram.h>
#include <stdio.h>

#include "Common/Device/Timer.h"
#include "Microbenchmark/Device/ThreadCache.h"

struct ThreadCache *initThreadCache(struct BuddyAllocator *buddyAllocator,
                                    int sizeClass, int pageSize, int ownerId,
                                    int sizeClassIndex) {
  struct ThreadCache *threadCache = mem_alloc(sizeof(struct ThreadCache));

  assert(sizeClass > 0);
  assert(isPowOf2(pageSize));

  threadCache->buddyAllocator = buddyAllocator;
  threadCache->sizeClass = sizeClass;
  threadCache->pageSize = pageSize;
  threadCache->ownerId = ownerId;
  threadCache->sizeClassIndex = sizeClassIndex;
  threadCache->root = buddyAllocate(buddyAllocator, pageSize);

  initThreadCacheNode(threadCache, threadCache->root, NULL);

  return threadCache;
}

void finiThreadCache(struct ThreadCache *threadCache) {
  __mram_ptr struct ThreadCacheNode *node = threadCache->root;

  while (node != NULL) {
    struct ThreadCacheNode *neighbors[2] __dma_aligned;

    mram_read((__mram_ptr void *)node, (void *)neighbors,
              2 * sizeof(__mram_ptr struct ThreadCacheNode *));

    __mram_ptr struct ThreadCacheNode *next =
        (__mram_ptr struct ThreadCacheNode *)neighbors[1];

    buddyFree(threadCache->buddyAllocator, node);

    node = next;
  }
}

__mram_ptr void *allocateThreadCache(struct ThreadCache *threadCache) {
  __mram_ptr struct ThreadCacheNode *prev = NULL;
  __mram_ptr struct ThreadCacheNode *node = threadCache->root;

  while (node != NULL) {
    __mram_ptr void *pointer = allocateThreadCacheNode(threadCache, node);

    if (pointer != NULL) {
      return pointer;
    }

    struct ThreadCacheNode *neighbors[2] __dma_aligned;

    mram_read((__mram_ptr void *)node, (void *)neighbors,
              2 * sizeof(__mram_ptr struct ThreadCacheNode *));

    __mram_ptr struct ThreadCacheNode *next =
        (__mram_ptr struct ThreadCacheNode *)neighbors[1];

    prev = node;
    node = next;
  }

  __mram_ptr struct ThreadCacheNode *newNode =
      buddyAllocate(threadCache->buddyAllocator, threadCache->pageSize);
  initThreadCacheNode(threadCache, newNode, prev);

  __mram_ptr void *pointer = allocateThreadCacheNode(threadCache, newNode);

  assert(pointer != NULL);

  return pointer;
}

int freeThreadCache(struct ThreadCache *threadCache, __mram_ptr void *pointer) {
  int isFreedInThreadCache = 0;

  __mram_ptr struct ThreadCacheNode *node = threadCache->root;
  while (node != NULL) {
    int isFreedInThreadCacheNode =
        freeThreadCacheNode(threadCache, node, pointer);

    if (isFreedInThreadCacheNode) {
      return 1;
    }

    struct ThreadCacheNode *neighbors[2] __dma_aligned;

    mram_read((__mram_ptr void *)node, (void *)neighbors,
              2 * sizeof(__mram_ptr struct ThreadCacheNode *));

    __mram_ptr struct ThreadCacheNode *next =
        (__mram_ptr struct ThreadCacheNode *)neighbors[1];

    node = next;
  }

  return isFreedInThreadCache;
}

void initThreadCacheNode(struct ThreadCache *threadCache,
                         __mram_ptr struct ThreadCacheNode *node,
                         __mram_ptr struct ThreadCacheNode *prev) {
  // NOTE: the new node is fully initialized before it is linked, so a
  // concurrent list walker never observes an uninitialized header
  int header[THREAD_CACHE_HEADER_SIZE / sizeof(int)] __dma_aligned = {
      (int)prev,  0,          0xFFFFFFFF,
      0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
      threadCache->ownerId,   threadCache->sizeClassIndex};

  mram_write(header, (__mram_ptr void *)node, THREAD_CACHE_HEADER_SIZE);

  // NOTE: the root node has no predecessor; linking it would clobber MRAM
  // address 0
  if (prev != NULL) {
    struct ThreadCacheNode *prevNeighbors[2] __dma_aligned;

    mram_read((__mram_ptr void *)prev, (void *)prevNeighbors,
              2 * sizeof(__mram_ptr struct ThreadCacheNode *));

    prevNeighbors[1] = (struct ThreadCacheNode *)node;

    mram_write(prevNeighbors, (__mram_ptr void *)prev,
               2 * sizeof(__mram_ptr struct ThreadCacheNode *));
  }
}

void finiThreadCacheNode(struct ThreadCache *threadCache,
                         __mram_ptr struct ThreadCacheNode *node) {
  struct ThreadCacheNode *neighbors[2] __dma_aligned;
  int metadata[4] __dma_aligned;

  mram_read((__mram_ptr void *)node, (void *)neighbors,
            2 * sizeof(__mram_ptr struct ThreadCacheNode *));
  mram_read(((__mram_ptr void *)node) +
                2 * sizeof(__mram_ptr struct ThreadCacheNode *),
            (void *)metadata, 4 * sizeof(int));

  assert(metadata[0] == 0xFFFFFFFF);
  assert(metadata[1] == 0xFFFFFFFF);
  assert(metadata[2] == 0xFFFFFFFF);
  assert(metadata[3] == 0xFFFFFFFF);

  __mram_ptr struct ThreadCacheNode *prev =
      (__mram_ptr struct ThreadCacheNode *)neighbors[0];
  __mram_ptr struct ThreadCacheNode *next =
      (__mram_ptr struct ThreadCacheNode *)neighbors[1];

  struct ThreadCacheNode *prevNeighbors[2] __dma_aligned;
  struct ThreadCacheNode *nextNeighbors[2] __dma_aligned;

  mram_read((__mram_ptr void *)prev, (void *)prevNeighbors,
            2 * sizeof(__mram_ptr struct ThreadCacheNode *));
  mram_read((__mram_ptr void *)next, (void *)nextNeighbors,
            2 * sizeof(__mram_ptr struct ThreadCacheNode *));

  prevNeighbors[1] = (struct ThreadCacheNode *)next;
  nextNeighbors[0] = (struct ThreadCacheNode *)prev;

  mram_write(prevNeighbors, (__mram_ptr void *)prev,
             2 * sizeof(__mram_ptr struct ThreadCacheNode *));
  mram_write(nextNeighbors, (__mram_ptr void *)next,
             2 * sizeof(__mram_ptr struct ThreadCacheNode *));

  buddyFree(threadCache->buddyAllocator, node);
}

__mram_ptr void *allocateThreadCacheNode(
    struct ThreadCache *threadCache, __mram_ptr struct ThreadCacheNode *node) {
  int metadata[4] __dma_aligned;

  mram_read(((__mram_ptr void *)node) +
                2 * sizeof(__mram_ptr struct ThreadCacheNode *),
            (void *)metadata, 4 * sizeof(int));

  int numChunks = getThreadCacheNumChunks(threadCache);

  int metadataIndex;
  int countLeadingZeros3 = count_leading_zeros(metadata[3]);
  metadataIndex = countLeadingZeros3;
  if (metadata[3] != 0 && metadataIndex < numChunks) {
    int metadataIndex = countLeadingZeros3;
    int metadataShift = 31 - countLeadingZeros3;

    int mask = ~(0b1 << metadataShift);

    metadata[3] &= mask;

    mram_write(metadata,
               ((__mram_ptr void *)node) +
                   2 * sizeof(__mram_ptr struct ThreadCacheNode *),
               4 * sizeof(int));

    return getThreadCachePointer(threadCache, node, metadataIndex);
  }

  int countLeadingZeros2 = count_leading_zeros(metadata[2]);
  metadataIndex = 32 + countLeadingZeros2;
  if (metadata[2] != 0 && metadataIndex < numChunks) {
    int metadataShift = 31 - countLeadingZeros2;

    int mask = ~(0b1 << metadataShift);

    metadata[2] &= mask;

    mram_write(metadata,
               ((__mram_ptr void *)node) +
                   2 * sizeof(__mram_ptr struct ThreadCacheNode *),
               4 * sizeof(int));

    return getThreadCachePointer(threadCache, node, metadataIndex);
  }

  int countLeadingZeros1 = count_leading_zeros(metadata[1]);
  metadataIndex = 64 + countLeadingZeros1;
  if (metadata[1] != 0 && metadataIndex < numChunks) {
    int metadataShift = 31 - countLeadingZeros1;

    int mask = ~(0b1 << metadataShift);

    metadata[1] &= mask;

    mram_write(metadata,
               ((__mram_ptr void *)node) +
                   2 * sizeof(__mram_ptr struct ThreadCacheNode *),
               4 * sizeof(int));

    return getThreadCachePointer(threadCache, node, metadataIndex);
  }

  int countLeadingZeros0 = count_leading_zeros(metadata[0]);
  metadataIndex = 96 + countLeadingZeros0;
  if (metadata[0] != 0 && metadataIndex < numChunks) {
    int metadataShift = 31 - countLeadingZeros0;

    int mask = ~(0b1 << metadataShift);

    metadata[0] &= mask;

    mram_write(metadata,
               ((__mram_ptr void *)node) +
                   2 * sizeof(__mram_ptr struct ThreadCacheNode *),
               4 * sizeof(int));

    return getThreadCachePointer(threadCache, node, metadataIndex);
  }

  return NULL;
}

int freeThreadCacheNode(struct ThreadCache *threadCache,
                        __mram_ptr struct ThreadCacheNode *node,
                        __mram_ptr void *pointer) {
  if (((__mram_ptr void *)node) + THREAD_CACHE_HEADER_SIZE <= pointer &&
      pointer < ((__mram_ptr void *)node) + threadCache->pageSize) {
    int metadata[4] __dma_aligned;
    mram_read(((__mram_ptr void *)node) +
                  2 * sizeof(__mram_ptr struct ThreadCacheNode *),
              (void *)metadata, 4 * sizeof(int));

    int chunkIndex = getThreadCacheChunkIndex(threadCache, node, pointer);

    assert((pointer - ((__mram_ptr void *)node) - THREAD_CACHE_HEADER_SIZE) %
               threadCache->sizeClass ==
           0);
    assert(chunkIndex < getThreadCacheNumChunks(threadCache) &&
           chunkIndex < 128);

    // NOTE: must mirror allocateThreadCacheNode, which hands out chunk k from
    // bit (31 - k % 32) of metadata[3 - k / 32]
    int metadataIndex = 3 - chunkIndex / 32;
    int metadataShift = 31 - chunkIndex % 32;

    int mask = 0b1 << metadataShift;

    assert((metadata[metadataIndex] & mask) == 0 && "double free");

    metadata[metadataIndex] |= mask;

    mram_write(metadata,
               ((__mram_ptr void *)node) +
                   2 * sizeof(__mram_ptr struct ThreadCacheNode *),
               4 * sizeof(int));

    return 1;
  } else {
    return 0;
  }
}

int getThreadCacheNumChunks(struct ThreadCache *threadCache) {
  return (threadCache->pageSize - THREAD_CACHE_HEADER_SIZE) /
         threadCache->sizeClass;
}

__mram_ptr void *getThreadCachePointer(struct ThreadCache *threadCache,
                                       __mram_ptr struct ThreadCacheNode *node,
                                       int chunkIndex) {
  assert(0 <= chunkIndex && chunkIndex < getThreadCacheNumChunks(threadCache));

  return ((__mram_ptr void *)node) + THREAD_CACHE_HEADER_SIZE +
         chunkIndex * threadCache->sizeClass;
}

int getThreadCacheChunkIndex(struct ThreadCache *threadCache,
                             __mram_ptr struct ThreadCacheNode *node,
                             __mram_ptr void *pointer) {
  assert(((__mram_ptr void *)node) + THREAD_CACHE_HEADER_SIZE <= pointer &&
         pointer < ((__mram_ptr void *)node) + threadCache->pageSize);

  return (((__mram_ptr void *)pointer) - ((__mram_ptr void *)node) -
          THREAD_CACHE_HEADER_SIZE) /
         threadCache->sizeClass;
}

void readThreadCacheNodeOwner(__mram_ptr struct ThreadCacheNode *node,
                              int *ownerId, int *sizeClassIndex) {
  int owner[2] __dma_aligned;

  mram_read(((__mram_ptr void *)node) + THREAD_CACHE_OWNER_OFFSET,
            (void *)owner, sizeof(owner));

  *ownerId = owner[0];
  *sizeClassIndex = owner[1];
}

int countThreadCachePages(struct ThreadCache *threadCache) {
  int numPages = 0;

  __mram_ptr struct ThreadCacheNode *node = threadCache->root;
  while (node != NULL) {
    numPages++;

    struct ThreadCacheNode *neighbors[2] __dma_aligned;

    mram_read((__mram_ptr void *)node, (void *)neighbors,
              2 * sizeof(__mram_ptr struct ThreadCacheNode *));

    node = (__mram_ptr struct ThreadCacheNode *)neighbors[1];
  }

  return numPages;
}

int countThreadCacheAllocatedChunks(struct ThreadCache *threadCache) {
  int numChunks = getThreadCacheNumChunks(threadCache);
  if (numChunks > 128) {
    numChunks = 128;
  }

  int numAllocatedChunks = 0;

  __mram_ptr struct ThreadCacheNode *node = threadCache->root;
  while (node != NULL) {
    int metadata[4] __dma_aligned;

    mram_read(((__mram_ptr void *)node) +
                  2 * sizeof(__mram_ptr struct ThreadCacheNode *),
              (void *)metadata, 4 * sizeof(int));

    for (int chunkIndex = 0; chunkIndex < numChunks; chunkIndex++) {
      int metadataIndex = 3 - chunkIndex / 32;
      int metadataShift = 31 - chunkIndex % 32;

      if ((metadata[metadataIndex] & (0b1 << metadataShift)) == 0) {
        numAllocatedChunks++;
      }
    }

    struct ThreadCacheNode *neighbors[2] __dma_aligned;

    mram_read((__mram_ptr void *)node, (void *)neighbors,
              2 * sizeof(__mram_ptr struct ThreadCacheNode *));

    node = (__mram_ptr struct ThreadCacheNode *)neighbors[1];
  }

  return numAllocatedChunks;
}
