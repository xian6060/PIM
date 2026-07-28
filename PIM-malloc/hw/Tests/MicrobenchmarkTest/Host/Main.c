#include <dpu.h>
#include <stdio.h>
#include <assert.h>
#include <time.h>

// UPMEM 官方強烈建議的錯誤捕捉巨集
#ifndef DPU_ASSERT
#define DPU_ASSERT(x) assert((x) == DPU_OK)
#endif

#define NUM_DPUS 1
#define NUM_TASKLETS 1

int main() {
    printf("[1] 程式啟動，準備設定參數...\n");
    // 先將分配的記憶體縮小為 2MB 進行基礎測試
    int mallocableRegionSize = 2 * 1024 * 1024; 
    int pageSize = 4 * 1024;
    int bufferSize = 1 * 1024;

    struct dpu_set_t dpu_set;
    struct dpu_set_t dpu;

    printf("[2] 正在向系統請求分配 %d 顆 DPU...\n", NUM_DPUS);
    // 將 NULL 改為 "backend=simulator"，明確告訴 SDK 使用模擬器，消除警告
    DPU_ASSERT(dpu_alloc(NUM_DPUS, "backend=simulator", &dpu_set));

    printf("[3] DPU 分配成功！正在載入 DPU 執行檔 (Device Binary)...\n");
    // 這裡如果崩潰，通常代表 DPU_BINARY 檔案不存在或編譯失敗
    DPU_ASSERT(dpu_load(dpu_set, DPU_BINARY, NULL));

    printf("[4] 執行檔載入成功！正在傳輸初始化參數到 DPU...\n");
    int i;
    DPU_FOREACH(dpu_set, dpu, i) { dpu_prepare_xfer(dpu, &mallocableRegionSize); }
    DPU_ASSERT(dpu_push_xfer(dpu_set, DPU_XFER_TO_DPU, "mallocableRegionSize", 0, sizeof(int), DPU_XFER_DEFAULT));

    DPU_FOREACH(dpu_set, dpu, i) { dpu_prepare_xfer(dpu, &pageSize); }
    DPU_ASSERT(dpu_push_xfer(dpu_set, DPU_XFER_TO_DPU, "pageSize", 0, sizeof(int), DPU_XFER_DEFAULT));

    DPU_FOREACH(dpu_set, dpu, i) { dpu_prepare_xfer(dpu, &bufferSize); }
    DPU_ASSERT(dpu_push_xfer(dpu_set, DPU_XFER_TO_DPU, "bufferSize", 0, sizeof(int), DPU_XFER_DEFAULT));

    printf("[5] 參數傳輸完畢！正式啟動 DPU 執行運算...\n");
    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    DPU_ASSERT(dpu_launch(dpu_set, DPU_SYNCHRONOUS));
    clock_gettime(CLOCK_MONOTONIC, &t1);
    unsigned long long elapsed_us = (unsigned long long)(t1.tv_sec - t0.tv_sec) * 1000000ULL + (unsigned long long)(t1.tv_nsec - t0.tv_nsec) / 1000ULL;
    printf("[Host] DPU execution time: %llu us\n", elapsed_us);

    printf("[6] DPU 執行完畢，釋放資源。\n");
    DPU_ASSERT(dpu_free(dpu_set));

    return 0;
}