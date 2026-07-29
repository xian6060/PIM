#include <dpu.h>
#include <assert.h>
#include <time.h>
#include <stdio.h>

// UPMEM 官方強烈建議的錯誤捕捉巨集
#ifndef DPU_ASSERT
#define DPU_ASSERT(x) assert((x) == DPU_OK)
#endif

#define NUM_DPUS 1
#define NUM_TASKLETS 1

static unsigned long long run_benchmark(int alienCacheFlushThreshold) {
    int mallocableRegionSize = 2 * 1024 * 1024;
    int pageSize = 4 * 1024;
    int bufferSize = 1 * 1024;

    struct dpu_set_t dpu_set;
    struct dpu_set_t dpu;

    DPU_ASSERT(dpu_alloc(NUM_DPUS, "backend=simulator", &dpu_set));
    DPU_ASSERT(dpu_load(dpu_set, DPU_BINARY, NULL));

    int i;
    DPU_FOREACH(dpu_set, dpu, i) { dpu_prepare_xfer(dpu, &mallocableRegionSize); }
    DPU_ASSERT(dpu_push_xfer(dpu_set, DPU_XFER_TO_DPU, "mallocableRegionSize", 0, sizeof(int), DPU_XFER_DEFAULT));

    DPU_FOREACH(dpu_set, dpu, i) { dpu_prepare_xfer(dpu, &pageSize); }
    DPU_ASSERT(dpu_push_xfer(dpu_set, DPU_XFER_TO_DPU, "pageSize", 0, sizeof(int), DPU_XFER_DEFAULT));

    DPU_FOREACH(dpu_set, dpu, i) { dpu_prepare_xfer(dpu, &bufferSize); }
    DPU_ASSERT(dpu_push_xfer(dpu_set, DPU_XFER_TO_DPU, "bufferSize", 0, sizeof(int), DPU_XFER_DEFAULT));

    DPU_FOREACH(dpu_set, dpu, i) { dpu_prepare_xfer(dpu, &alienCacheFlushThreshold); }
    DPU_ASSERT(dpu_push_xfer(dpu_set, DPU_XFER_TO_DPU, "alienCacheFlushThreshold", 0, sizeof(int), DPU_XFER_DEFAULT));

    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    DPU_ASSERT(dpu_launch(dpu_set, DPU_SYNCHRONOUS));
    clock_gettime(CLOCK_MONOTONIC, &t1);

    unsigned long long elapsed_us =
        (unsigned long long)(t1.tv_sec - t0.tv_sec) * 1000000ULL +
        (unsigned long long)(t1.tv_nsec - t0.tv_nsec) / 1000ULL;

    DPU_ASSERT(dpu_free(dpu_set));
    return elapsed_us;
}

int main() {
    printf("[1] 程式啟動，準備設定參數...\n");
    int thresholds[] = {1, 8, 16, 32, 64};
    int thresholdsLength = sizeof(thresholds) / sizeof(thresholds[0]);

    printf("[2] 開始依序量測 threshold = 1, 8, 16, 32, 64 ...\n");
    for (int i = 0; i < thresholdsLength; i++) {
        int threshold = thresholds[i];
        printf("[3.%d] 正在量測 threshold = %d ...\n", i + 1, threshold);
        unsigned long long elapsed_us = run_benchmark(threshold);
        printf("[Host] threshold=%d DPU execution time: %llu us\n", threshold, elapsed_us);
    }

    printf("[4] 全部 DPU 測試執行完畢，釋放資源。\n");

    return 0;
}