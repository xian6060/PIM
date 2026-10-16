#include <dpu.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "MicrobenchmarkTest/BenchmarkResult.h"

#define MAX_THRESHOLDS 32
#define DEFAULT_THRESHOLDS "1,2,4,8,16,32,64"
#define DEFAULT_REPETITIONS 3
#define DEFAULT_RAW_OUTPUT "alien_cache_threshold_raw.csv"
#define DEFAULT_SUMMARY_OUTPUT "alien_cache_threshold_summary.csv"

struct BenchmarkConfig {
  int mallocableRegionSize;
  int pageSize;
  int bufferSize;
  int allocSize;
  int batchSize;
  int numRounds;
  int localFreePercent;
  int remotePattern;
};

static void checkDpu(dpu_error_t error, const char *what) {
  if (error != DPU_OK) {
    fprintf(stderr, "%s failed: %s\n", what, dpu_error_to_string(error));
    exit(EXIT_FAILURE);
  }
}

static void copyIntToDpu(struct dpu_set_t dpuSet, const char *symbol,
                         int value) {
  checkDpu(dpu_copy_to(dpuSet, symbol, 0, &value, sizeof(value)), symbol);
}

static struct BenchmarkResult runBenchmark(struct dpu_set_t dpuSet,
                                           const struct BenchmarkConfig *config,
                                           int threshold, int validate) {
  checkDpu(dpu_load(dpuSet, DPU_BINARY, NULL), "dpu_load");

  copyIntToDpu(dpuSet, "mallocableRegionSize", config->mallocableRegionSize);
  copyIntToDpu(dpuSet, "pageSize", config->pageSize);
  copyIntToDpu(dpuSet, "bufferSize", config->bufferSize);
  copyIntToDpu(dpuSet, "alienCacheFlushThreshold", threshold);
  copyIntToDpu(dpuSet, "allocSize", config->allocSize);
  copyIntToDpu(dpuSet, "batchSize", config->batchSize);
  copyIntToDpu(dpuSet, "numRounds", config->numRounds);
  copyIntToDpu(dpuSet, "localFreePercent", config->localFreePercent);
  copyIntToDpu(dpuSet, "remotePattern", config->remotePattern);
  copyIntToDpu(dpuSet, "validate", validate);

  dpu_error_t error = dpu_launch(dpuSet, DPU_SYNCHRONOUS);
  if (error != DPU_OK) {
    struct dpu_set_t dpu;
    fprintf(stderr, "dpu_launch failed (threshold=%d): %s\n", threshold,
            dpu_error_to_string(error));
    DPU_FOREACH(dpuSet, dpu) { dpu_log_read(dpu, stderr); }
    exit(EXIT_FAILURE);
  }

  struct BenchmarkResult result;
  struct dpu_set_t dpu;
  DPU_FOREACH(dpuSet, dpu) {
    checkDpu(dpu_copy_from(dpu, "result", 0, &result, sizeof(result)),
             "dpu_copy_from(result)");
  }

  return result;
}

static int parseInt(const char *value, const char *name, int minimum,
                    int maximum) {
  char *end = NULL;
  long parsed = strtol(value, &end, 10);
  if (value[0] == '\0' || *end != '\0' || parsed < minimum ||
      parsed > maximum) {
    fprintf(stderr, "Invalid %s: %s (expected %d..%d)\n", name, value, minimum,
            maximum);
    exit(EXIT_FAILURE);
  }
  return (int)parsed;
}

static int parseThresholds(const char *value, int *thresholds) {
  char buffer[256];
  snprintf(buffer, sizeof(buffer), "%s", value);

  int count = 0;
  for (char *token = strtok(buffer, ","); token != NULL;
       token = strtok(NULL, ",")) {
    if (count == MAX_THRESHOLDS) {
      fprintf(stderr, "Too many thresholds (max %d)\n", MAX_THRESHOLDS);
      exit(EXIT_FAILURE);
    }
    thresholds[count++] = parseInt(token, "threshold", 1, 64);
  }

  if (count == 0) {
    fprintf(stderr, "No thresholds given\n");
    exit(EXIT_FAILURE);
  }
  return count;
}

static int compareU64(const void *a, const void *b) {
  uint64_t x = *(const uint64_t *)a;
  uint64_t y = *(const uint64_t *)b;
  return (x > y) - (x < y);
}

static void usage(const char *program) {
  fprintf(stderr,
          "Usage: %s [--thresholds LIST] [--repetitions N] [--rounds N]\n"
          "          [--batch N] [--size BYTES] [--local-percent P]\n"
          "          [--remote-pattern neighbor|scatter|hot-owner]\n"
          "          [--skip-validation] [--raw-output PATH]\n"
          "          [--summary-output PATH]\n"
          "Defaults: --thresholds %s --repetitions %d --rounds 8 --batch 64\n"
          "          --size 32 --local-percent 50 --remote-pattern neighbor\n",
          program, DEFAULT_THRESHOLDS, DEFAULT_REPETITIONS);
}

int main(int argc, char **argv) {
  struct BenchmarkConfig config = {
      .mallocableRegionSize = 2 * 1024 * 1024,
      .pageSize = 4 * 1024,
      .bufferSize = 1 * 1024,
      .allocSize = 32,
      .batchSize = 64,
      .numRounds = 8,
      .localFreePercent = 50,
      .remotePattern = 0,
  };

  int thresholds[MAX_THRESHOLDS];
  int numThresholds = parseThresholds(DEFAULT_THRESHOLDS, thresholds);
  int repetitions = DEFAULT_REPETITIONS;
  int skipValidation = 0;
  const char *rawOutput = DEFAULT_RAW_OUTPUT;
  const char *summaryOutput = DEFAULT_SUMMARY_OUTPUT;

  for (int i = 1; i < argc; i++) {
    int hasValue = i + 1 < argc;

    if (strcmp(argv[i], "--thresholds") == 0 && hasValue) {
      numThresholds = parseThresholds(argv[++i], thresholds);
    } else if (strcmp(argv[i], "--repetitions") == 0 && hasValue) {
      repetitions = parseInt(argv[++i], "repetitions", 1, 1000);
    } else if (strcmp(argv[i], "--rounds") == 0 && hasValue) {
      config.numRounds = parseInt(argv[++i], "rounds", 1, 100000);
    } else if (strcmp(argv[i], "--batch") == 0 && hasValue) {
      config.batchSize = parseInt(argv[++i], "batch", 1, 128);
    } else if (strcmp(argv[i], "--size") == 0 && hasValue) {
      config.allocSize = parseInt(argv[++i], "size", 8, 2048);
    } else if (strcmp(argv[i], "--local-percent") == 0 && hasValue) {
      config.localFreePercent = parseInt(argv[++i], "local-percent", 0, 100);
    } else if (strcmp(argv[i], "--remote-pattern") == 0 && hasValue) {
      const char *pattern = argv[++i];
      if (strcmp(pattern, "neighbor") == 0) {
        config.remotePattern = 0;
      } else if (strcmp(pattern, "scatter") == 0) {
        config.remotePattern = 1;
      } else if (strcmp(pattern, "hot-owner") == 0) {
        config.remotePattern = 2;
      } else {
        fprintf(stderr, "Invalid remote-pattern: %s\n", pattern);
        return EXIT_FAILURE;
      }
    } else if (strcmp(argv[i], "--skip-validation") == 0) {
      skipValidation = 1;
    } else if (strcmp(argv[i], "--raw-output") == 0 && hasValue) {
      rawOutput = argv[++i];
    } else if (strcmp(argv[i], "--summary-output") == 0 && hasValue) {
      summaryOutput = argv[++i];
    } else {
      usage(argv[0]);
      return EXIT_FAILURE;
    }
  }

  FILE *rawFile = fopen(rawOutput, "w");
  FILE *summaryFile = fopen(summaryOutput, "w");
  if (rawFile == NULL || summaryFile == NULL) {
    fprintf(stderr, "Cannot open CSV output: %s\n", strerror(errno));
    return EXIT_FAILURE;
  }

  fprintf(rawFile,
          "threshold,repetition,total_cycles,alloc_cycles,free_cycles,"
          "drain_cycles,local_frees,alien_enqueues,alien_flushes,"
          "flushed_pointers,owner_lock_acquires,cache_searches,buddy_frees,"
          "pages\n");
  fprintf(summaryFile,
          "threshold,repetitions,median_total_cycles,mean_total_cycles,"
          "stddev_total_cycles,min_total_cycles,max_total_cycles,"
          "mean_alloc_cycles,mean_free_cycles,mean_drain_cycles,"
          "alien_flushes,owner_lock_acquires,cache_searches,pages,"
          "validation\n");

  const char *patternNames[] = {"neighbor", "scatter", "hot-owner"};
  printf("config: size=%d batch=%d rounds=%d local=%d%% remote=%s "
         "tasklets=16 repetitions=%d\n",
         config.allocSize, config.batchSize, config.numRounds,
         config.localFreePercent, patternNames[config.remotePattern],
         repetitions);
  printf("%9s %12s %12s %12s %12s %8s %10s %10s %6s %s\n", "threshold",
         "median_cyc", "alloc_cyc", "free_cyc", "drain_cyc", "flushes",
         "lock_acq", "searches", "pages", "validation");

  struct dpu_set_t dpuSet;
  checkDpu(dpu_alloc(1, "backend=simulator", &dpuSet), "dpu_alloc");

  uint64_t *samples = malloc((size_t)repetitions * sizeof(uint64_t));
  int exitCode = EXIT_SUCCESS;

  for (int t = 0; t < numThresholds; t++) {
    int threshold = thresholds[t];

    // NOTE: validation writes and checks a tag in every allocation, so it
    // runs separately from the timed repetitions
    const char *validation = "skipped";
    if (!skipValidation) {
      struct BenchmarkResult check =
          runBenchmark(dpuSet, &config, threshold, 1);

      if (check.validationErrors == 0 && check.leakedChunks == 0) {
        validation = "pass";
      } else {
        validation = "FAIL";
        exitCode = EXIT_FAILURE;
        fprintf(stderr,
                "threshold=%d validation failed: %llu tag errors, %llu "
                "leaked chunks\n",
                threshold, (unsigned long long)check.validationErrors,
                (unsigned long long)check.leakedChunks);
      }
    }

    struct BenchmarkResult last;
    double sumTotal = 0, sumAlloc = 0, sumFree = 0, sumDrain = 0;

    for (int r = 0; r < repetitions; r++) {
      last = runBenchmark(dpuSet, &config, threshold, 0);
      samples[r] = last.totalCycles;
      sumTotal += last.totalCycles;
      sumAlloc += last.allocCycles;
      sumFree += last.freeCycles;
      sumDrain += last.drainCycles;

      fprintf(rawFile,
              "%d,%d,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,"
              "%llu\n",
              threshold, r + 1, (unsigned long long)last.totalCycles,
              (unsigned long long)last.allocCycles,
              (unsigned long long)last.freeCycles,
              (unsigned long long)last.drainCycles,
              (unsigned long long)last.localFrees,
              (unsigned long long)last.alienEnqueues,
              (unsigned long long)last.alienFlushes,
              (unsigned long long)last.flushedPointers,
              (unsigned long long)last.ownerLockAcquires,
              (unsigned long long)last.cacheSearches,
              (unsigned long long)last.buddyFrees,
              (unsigned long long)last.numPages);
    }

    double mean = sumTotal / repetitions;
    double squaredDifferenceSum = 0;
    for (int r = 0; r < repetitions; r++) {
      squaredDifferenceSum += (samples[r] - mean) * (samples[r] - mean);
    }
    double stddev =
        repetitions > 1 ? sqrt(squaredDifferenceSum / (repetitions - 1)) : 0;

    qsort(samples, repetitions, sizeof(uint64_t), compareU64);
    double median = repetitions % 2
                        ? samples[repetitions / 2]
                        : (samples[repetitions / 2 - 1] +
                           samples[repetitions / 2]) /
                              2.0;

    fprintf(summaryFile,
            "%d,%d,%.1f,%.1f,%.1f,%llu,%llu,%.1f,%.1f,%.1f,%llu,%llu,%llu,"
            "%llu,%s\n",
            threshold, repetitions, median, mean, stddev,
            (unsigned long long)samples[0],
            (unsigned long long)samples[repetitions - 1],
            sumAlloc / repetitions, sumFree / repetitions,
            sumDrain / repetitions, (unsigned long long)last.alienFlushes,
            (unsigned long long)last.ownerLockAcquires,
            (unsigned long long)last.cacheSearches,
            (unsigned long long)last.numPages, validation);

    printf("%9d %12.0f %12.0f %12.0f %12.0f %8llu %10llu %10llu %6llu %s\n",
           threshold, median, sumAlloc / repetitions, sumFree / repetitions,
           sumDrain / repetitions, (unsigned long long)last.alienFlushes,
           (unsigned long long)last.ownerLockAcquires,
           (unsigned long long)last.cacheSearches,
           (unsigned long long)last.numPages, validation);
    fflush(stdout);
  }

  free(samples);
  checkDpu(dpu_free(dpuSet), "dpu_free");
  fclose(rawFile);
  fclose(summaryFile);

  printf("CSV written to %s and %s\n", rawOutput, summaryOutput);
  return exitCode;
}
