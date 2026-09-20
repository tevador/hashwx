/* Copyright (c) 2020-2026 tevador <tevador@gmail.com> */
/* See LICENSE for licensing information */

#include "test_utils.h"
#include "platform.h"
#include "siphash_rng.h"

#include <hashwx.h>
#include <limits.h>
#include <inttypes.h>
#if defined(HASHWX_THREADS)
#include <threads.h>
#else
typedef int thrd_t;
#endif

#define WORKER_OK 0
#define WORKER_ERROR_ALLOC 1
#define WORKER_ERROR_NOSUPP 2

typedef struct worker_job {
    int id;
    thrd_t thread;
    hashwx_ctx* ctx;
    hashwx_type flags;
    int64_t total_hashes;
    uint64_t best_hash;
    uint64_t threshold;
    uint64_t hash_sum;
    int start;
    int step;
    int end;
    int nonces;
    int res;
} worker_job;

static const siphash_key worker_key = {
    .k0 = 0xb443266e0c61253a,
    .k1 = 0x85cfeef0bcbdb1e9
};

static int worker(void* args) {
    worker_job* job = (worker_job*)args;
    job->total_hashes = 0;
    job->best_hash = UINT64_MAX;
    job->hash_sum = 0;
    job->ctx = hashwx_alloc(job->flags);
    if (job->ctx == NULL) {
        return WORKER_ERROR_ALLOC;
    }
    if (job->ctx == HASHWX_NOTSUPP) {
        return WORKER_ERROR_NOSUPP;
    }
    for (int seed = job->start; seed < job->end; seed += job->step) {
        siphash_rng gen;
        hashwx_rng_init(&gen, &worker_key, seed);
#ifdef PLATFORM_LE
        hashwx_make(job->ctx, (const uint8_t*)&gen.state);
#else
        uint64_t temp_seed[4];
        temp_seed[0] = platform_load64(&gen.state[0]);
        temp_seed[1] = platform_load64(&gen.state[1]);
        temp_seed[2] = platform_load64(&gen.state[2]);
        temp_seed[3] = platform_load64(&gen.state[3]);
        hashwx_make(job->ctx, (const uint8_t*)&temp_seed);
#endif
        for (int nonce = 0; nonce < job->nonces; ++nonce) {
            uint64_t hashval = hashwx_exec(job->ctx, nonce);
            job->hash_sum ^= hashval;
            if (hashval < job->best_hash) {
                job->best_hash = hashval;
            }
            if (hashval < job->threshold) {
                printf("[thread %2i] Hash (%5i, %5i) below threshold: %016" PRIx64 "\n",
                    job->id,
                    seed,
                    nonce,
                    hashval);
            }
        }
        job->total_hashes += job->nonces;
    }
    hashwx_free(job->ctx);
    return WORKER_OK;
}

int main(int argc, char** argv) {
    int nonces, seeds, start, diff, threads;
    bool interpret;
    read_int_option("--diff", argc, argv, &diff, INT_MAX);
    read_int_option("--start", argc, argv, &start, 0);
    read_int_option("--seeds", argc, argv, &seeds, 3000);
    read_int_option("--nonces", argc, argv, &nonces, 463);
    read_int_option("--threads", argc, argv, &threads, 1);
    read_option("--interpret", argc, argv, &interpret);
#if !defined(HASHWX_THREADS)
    if (threads > 1) {
        printf("Error: Your compiler doesn't support C11 threads.\n");
        return 1;
    }
#endif
    hashwx_type flags = HASHWX_INTERPRETED;
    if (!interpret) {
        flags = HASHWX_COMPILED;
    }
    uint64_t best_hash = UINT64_MAX;
    uint64_t diff_ex = (uint64_t)diff * 1000ULL;
    uint64_t threshold = UINT64_MAX / diff_ex;
    int seeds_end = seeds + start;
    int64_t total_hashes = 0;
    printf("Interpret: %i, Target diff.: %" PRIu64 ", Threads: %i\n", interpret, diff_ex, threads);
    printf("Testing seeds %i-%i with %i nonces each ...\n", start, seeds_end - 1, nonces);
    double time_start, time_end;
    worker_job* jobs = malloc(sizeof(worker_job) * threads);
    if (jobs == NULL) {
        printf("Error: memory allocation failure\n");
        return 1;
    }
    for (int thd = 0; thd < threads; ++thd) {
        jobs[thd].flags = flags;
        jobs[thd].id = thd;
        jobs[thd].start = start + thd;
        jobs[thd].step = threads;
        jobs[thd].end = seeds_end;
        jobs[thd].nonces = nonces;
        jobs[thd].threshold = threshold;
    }
    time_start = platform_wall_clock();
    if (threads > 1) {
#if defined(HASHWX_THREADS)
        for (int thd = 0; thd < threads; ++thd) {
            int res = thrd_create(&jobs[thd].thread , &worker, &jobs[thd]);
            if (res != thrd_success) {
                printf("Error: thread_create failed\n");
                return 1;
            }
        }
        for (int thd = 0; thd < threads; ++thd) {
            thrd_join(jobs[thd].thread, &jobs[thd].res);
        }
#endif
    }
    else {
        jobs[0].res = worker(jobs);
    }
    time_end = platform_wall_clock();
    int ret_val = 0;
    for (int thd = 0; thd < threads; ++thd) {
        if (jobs[thd].res == WORKER_ERROR_ALLOC) {
            printf("Error: memory allocation failure\n");
            ret_val = 1;
            goto cleanup;
        }
        if (jobs[thd].res == WORKER_ERROR_NOSUPP) {
            printf("Error: not supported. Try with --interpret\n");
            ret_val = 1;
            goto cleanup;
        }
    }
    uint64_t hash_sum = 0;
    for (int thd = 0; thd < threads; ++thd) {
        total_hashes += jobs[thd].total_hashes;
        hash_sum ^= jobs[thd].hash_sum;
        if (jobs[thd].best_hash < best_hash) {
            best_hash = jobs[thd].best_hash;
        }
    }
    double elapsed = time_end - time_start;
    printf("Total hashes: %" PRIi64 "\n", total_hashes);
    printf("%f hashes/sec.\n", total_hashes / elapsed);
    printf("%f seeds/sec.\n", seeds / elapsed);
    printf("Best hash: %016" PRIx64, best_hash);
    printf(" (diff: %" PRIu64 ")\n", UINT64_MAX / best_hash);
    printf("Hash sum: %016" PRIx64 "\n", hash_sum);
cleanup:
    free(jobs);
    return ret_val;
}
