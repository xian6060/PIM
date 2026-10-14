#include <dpu.h>
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdio.h>

// UPMEM 官方強烈建議的錯誤捕捉巨集
#ifndef DPU_ASSERT
#define DPU_ASSERT(x) assert((x) == DPU_OK)
#endif

#define NUM_DPUS 1
#define DEFAULT_REPETITIONS 10
#define DEFAULT_RAW_OUTPUT "alien_cache_threshold_raw.csv"
#define DEFAULT_SUMMARY_OUTPUT "alien_cache_threshold_summary.csv"

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

    time_t elapsed_seconds = t1.tv_sec - t0.tv_sec;
    long elapsed_nanoseconds = t1.tv_nsec - t0.tv_nsec;
    if (elapsed_nanoseconds < 0) {
        elapsed_seconds--;
        elapsed_nanoseconds += 1000000000L;
    }

    unsigned long long elapsed_us =
        (unsigned long long)elapsed_seconds * 1000000ULL +
        (unsigned long long)elapsed_nanoseconds / 1000ULL;

    DPU_ASSERT(dpu_free(dpu_set));
    return elapsed_us;
}

static int parse_positive_int(const char *value, const char *name) {
    char *end = NULL;
    long parsed = strtol(value, &end, 10);
    if (value[0] == '\0' || end == value || *end != '\0' || parsed <= 0 ||
        parsed > 1000000) {
        fprintf(stderr, "Invalid %s: %s\n", name, value);
        exit(EXIT_FAILURE);
    }
    return (int)parsed;
}

static double calculate_mean(const unsigned long long *samples, int count) {
    unsigned long long total = 0;
    for (int i = 0; i < count; i++) {
        total += samples[i];
    }
    return (double)total / count;
}

static double calculate_sample_stddev(const unsigned long long *samples,
                                      int count, double mean) {
    if (count < 2) {
        return 0.0;
    }

    double squared_difference_sum = 0.0;
    for (int i = 0; i < count; i++) {
        double difference = (double)samples[i] - mean;
        squared_difference_sum += difference * difference;
    }
    return sqrt(squared_difference_sum / (count - 1));
}

int main(int argc, char **argv) {
    int repetitions = DEFAULT_REPETITIONS;
    const char *raw_output = DEFAULT_RAW_OUTPUT;
    const char *summary_output = DEFAULT_SUMMARY_OUTPUT;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--repetitions") == 0 && i + 1 < argc) {
            repetitions = parse_positive_int(argv[++i], "repetitions");
        } else if (strcmp(argv[i], "--raw-output") == 0 && i + 1 < argc) {
            raw_output = argv[++i];
        } else if (strcmp(argv[i], "--summary-output") == 0 && i + 1 < argc) {
            summary_output = argv[++i];
        } else {
            fprintf(stderr,
                    "Usage: %s [--repetitions N] [--raw-output PATH] "
                    "[--summary-output PATH]\n",
                    argv[0]);
            return EXIT_FAILURE;
        }
    }

    FILE *raw_file = fopen(raw_output, "w");
    if (raw_file == NULL) {
        fprintf(stderr, "Cannot open raw CSV '%s': %s\n", raw_output,
                strerror(errno));
        return EXIT_FAILURE;
    }

    FILE *summary_file = fopen(summary_output, "w");
    if (summary_file == NULL) {
        fprintf(stderr, "Cannot open summary CSV '%s': %s\n", summary_output,
                strerror(errno));
        fclose(raw_file);
        return EXIT_FAILURE;
    }

    printf("[1] 程式啟動，準備設定參數...\n");
    int thresholds[] = {1, 8, 16, 32, 64};
    int thresholdsLength = sizeof(thresholds) / sizeof(thresholds[0]);

    fprintf(raw_file, "threshold,repetition,elapsed_us\n");
    fprintf(summary_file,
            "threshold,repetitions,mean_us,stddev_us,min_us,max_us\n");

    printf("[2] 每個 threshold 重複量測 %d 次...\n", repetitions);
    for (int i = 0; i < thresholdsLength; i++) {
        int threshold = thresholds[i];
        unsigned long long *samples =
            malloc((size_t)repetitions * sizeof(unsigned long long));
        if (samples == NULL) {
            fprintf(stderr, "Cannot allocate sample buffer\n");
            fclose(raw_file);
            fclose(summary_file);
            return EXIT_FAILURE;
        }

        unsigned long long minimum = ULLONG_MAX;
        unsigned long long maximum = 0;
        printf("[3.%d] 正在量測 threshold = %d ...\n", i + 1, threshold);
        for (int repetition = 0; repetition < repetitions; repetition++) {
            samples[repetition] = run_benchmark(threshold);
            fprintf(raw_file, "%d,%d,%llu\n", threshold, repetition + 1,
                    samples[repetition]);
            if (samples[repetition] < minimum) {
                minimum = samples[repetition];
            }
            if (samples[repetition] > maximum) {
                maximum = samples[repetition];
            }
            printf("[Host] threshold=%d repetition=%d elapsed=%llu us\n",
                   threshold, repetition + 1, samples[repetition]);
        }

        double mean = calculate_mean(samples, repetitions);
        double stddev =
            calculate_sample_stddev(samples, repetitions, mean);
        fprintf(summary_file, "%d,%d,%.3f,%.3f,%llu,%llu\n", threshold,
                repetitions, mean, stddev, minimum, maximum);
        printf("[Summary] threshold=%d mean=%.3f us stddev=%.3f us "
               "min=%llu us max=%llu us\n",
               threshold, mean, stddev, minimum, maximum);
        free(samples);
    }

    fclose(raw_file);
    fclose(summary_file);
    printf("[4] 全部 DPU 測試執行完畢，CSV 已寫入 %s 與 %s。\n", raw_output,
           summary_output);

    return EXIT_SUCCESS;
}