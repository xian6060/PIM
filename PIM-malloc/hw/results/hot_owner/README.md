# Hot-owner 模式

## 實驗目的

前面的 neighbor 和 scatter 模式中，每個 owner 只被一兩個 tasklet 釋放，lock 幾乎沒有競爭。hot-owner 測的是相反的極端：所有遠端釋放都回到同一個 owner，也就是 tasklet 0，讓 owner lock 的競爭最大。這代表 alien cache 可能有用的上限。

baseline 是 **threshold 1**。

## 模式說明（`--remote-pattern hot-owner`）

- **alloc 階段**
  - 每個 tasklet 自己配置 batch 的前 `local-percent` 部分。
  - 剩下的部分由 tasklet 0 替**所有** tasklet 配置。
- **free 階段**：每個 tasklet 釋放自己的整個 batch。前段是 local free，後段是 owner 為 tasklet 0 的遠端釋放。
- **`local-percent` 就是偏斜程度**：0% 時，除了 tasklet 0 自己以外，所有釋放都送回 tasklet 0。這是典型的生產者–消費者情境。
- **比較方式**：tasklet 0 在 alloc 階段做的事比其他 tasklet 多，所以 alloc cycles 本來就比較大。比較 threshold 時，以同一組設定內的相對變化為準。

## 執行設定

size=32、batch=64、rounds=8、16 tasklets、repetitions=3，使用 simulator，validation 全部 pass。

```
Microbenchmark_test_host --repetitions 3 --rounds 8 --batch 64 --size 32 \
  --local-percent {0,25,50,75} --remote-pattern hot-owner
```

## 結果

**相對 threshold 1 的 total cycles 變化**（負值代表較快）：

| local % | thr 1 total | thr 2 | thr 4 | thr 8 | thr 16 | thr 32 | thr 64 |
|---|---|---|---|---|---|---|---|
| 0  | 19,379,456 | −0.04% | −0.04% | −0.13% | −0.23% | −0.58% | **−0.98%** |
| 25 | 14,863,920 | −0.04% | −0.05% | −0.18% | **−0.33%** | +0.97% | +2.81% |
| 50 | 10,576,224 | −0.05% | −0.07% | −0.26% | −0.46% | **−0.88%** | +1.15% |
| 75 | 6,513,616  | −0.09% | −0.12% | −0.42% | **−0.75%** | −0.36% | +2.51% |

**free 階段（local 0%，threshold 1）**

| 模式 | free cycles | 每次 free 平均 |
|---|---|---|
| hot-owner | 16,247,152 | 約 1,983 cycles |
| neighbor | 2,455,888 | 約 300 cycles |

## 觀察

1. **hot-owner 的代價非常大。** 同樣是 8192 次 free，hot-owner 的 free 階段是 neighbor 的 6.6 倍，在 local 0% 時占 total 的 84%。原因：
   - 所有釋放都要排隊拿 tasklet 0 的 lock。
   - 等 lock 的 tasklet 在 busy-wait，會占走 pipeline 的執行機會，持有 lock 的 tasklet 因此跑得更慢。
2. **目前的 alien cache（Step 1）幾乎沒有幫助。** 最佳的情況也只比 threshold 1 快不到 1%。原因是 Step 1 flush 時，每個 pointer 都要各拿一次 lock：
   - 所有 threshold 的 `owner_lock_acquires` 都是 16384；
   - 批次處理只是把搶 lock 的時間點往後延，搶 lock 的次數完全沒有減少。
3. **threshold 太大反而變慢。** 當每個 tasklet 每 round 的遠端 pointer 數少於 threshold 時，pointer 會留在 alien cache 裡跨 round 不還，例如 local 50% 時每 round 只有 32 個遠端 pointer，但 threshold 是 64。結果：
   - tasklet 0 下一輪配置時拿不回這些記憶體，只能多要新 page（pages 132 → 136/139）；
   - alloc 階段因此慢 23%–26%；
   - free 階段雖然快了 1.5%–3.7%，但抵不過 alloc 的損失。

## 結論

- 以 threshold 1 為 baseline，目前 Step 1 形式的 alien cache 在最壞的熱點情況下也幾乎沒有收益（≤1%）。
- 真正的成本來自 owner lock 的競爭。要降低它，必須減少「拿 lock 的次數」或「lock 的粒度」，例如：
  - 一次 flush 對同一個 owner 只拿一次 lock（類似已撤回的 Step 2）；
  - 改成 per-page 或 per-size-class 的 lock。

  只延後釋放的時間點是不夠的。

## 備註

- benchmark 的 alloc 迴圈改成共用的 `allocateSlot()`，並多了 pattern 判斷。這讓 neighbor 和 scatter 的 alloc 階段比 `step1_owner_lookup` 的資料少約 7,400 cycles，約 0.56%；free 階段只差約 128 cycles。uMalloc 本身沒有改，每個 threshold 的位移都相同，所以不影響 threshold 之間的比較。
