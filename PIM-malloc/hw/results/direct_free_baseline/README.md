# 直接釋放 baseline（threshold 0）

## 實驗目的

threshold 1 仍然會經過 alien cache（enqueue → flush，而且 flush 時會再讀一次 page header），所以不能當作「沒有 alien cache」的對照組。這次加上 threshold 0 作為正式的 baseline：釋放遠端 pointer 時，直接沿用 `freeUMalloc` 已經讀到的 owner，`lockOwner` → `freeThreadCacheNode` → unlock，完全不使用 alien cache。

## 程式變動

- `uMalloc.c` / `uMalloc.h`
  - `struct UMalloc` 新增 `alienFlushThreshold`。
  - threshold 允許設為 0。
  - 在 `freeUMalloc` 的遠端路徑中，threshold 為 0 時直接釋放。
  - 新增 `directRemoteFrees` 計數器，沿用原本的 `reserved` 欄位，所以 struct 大小不變。
- Host
  - 預設 thresholds 改成 `0,1,2,4,8,16,32,64`。
  - raw 和 summary CSV 都新增 `direct_remote_frees` 欄位。

## 執行設定

size=32、batch=64、rounds=8、16 tasklets、repetitions=3，使用 simulator。每個 threshold 都先跑一次 validation，全部 pass。

```
Microbenchmark_test_host --repetitions 3 --rounds 8 --batch 64 --size 32 \
  --local-percent {50,0} --remote-pattern {neighbor,scatter}
```

## 結果（median total cycles）

| 設定 | threshold 0（直接釋放） | threshold 1 | alien cache 最佳 | 最佳比直接釋放慢 |
|---|---|---|---|---|
| neighbor local 50% | 2,846,592 | 3,310,896（+16.3%） | 3,072,816（thr 32） | +7.9% |
| neighbor local 0%  | 2,883,216 | 3,813,728（+32.3%） | 3,329,888（thr 64） | +15.5% |
| scatter local 50%  | 2,842,432 | 3,308,448（+16.4%） | 3,071,056（thr 32） | +8.0% |
| scatter local 0%   | 2,875,424 | 3,807,472（+32.4%） | 3,324,512（thr 64） | +15.6% |

- 只看 free phase（local 0% neighbor），以 8192 個遠端 pointer 平均：
  - 經過 alien cache、threshold 64 時，每個遠端 pointer 多 **55 cycles**。
  - threshold 1 時，每個遠端 pointer 多 **114 cycles**。
- 直接釋放的 owner lock 次數和 alien cache 一樣，都是 16384。但 header 只讀一次：local 0% 時 `cache_searches` 是 8192，alien cache 是 16384。
- neighbor 和 scatter 在直接釋放下幾乎沒有差別，因為每次 free 都是獨立 lock，跟遠端 pointer 分散到幾個 owner 無關。

## 跟之前資料的差異

- **舊 baseline**：之前報告用的是已撤回的 Step 3（6fc5e1f）在 threshold 1 的路徑，數字是 2.95M–3.06M。這次的直接釋放更快，為 2.84M–2.88M。原因是舊路徑仍然會：
  - 經過 `getAlienCache` 的 if 判斷鏈；
  - 更新三個 alien 計數器；
  - 重新計算一次 page。
- **threshold 1–64**：比 `step1_owner_lookup` 的資料慢 0.27%–0.65%。原因是遠端路徑多了一次 `alienFlushThreshold == 0` 的判斷。所以 alien cache 和直接釋放的比較，以本資料夾內的數字為準。
