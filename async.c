#include "async.h"
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

// Future-based async implementation

typedef struct future {
    pthread_mutex_t lock;
    pthread_cond_t cond;
    int done;
    void* result;
} future_t;

static future_t* future_create() {
    future_t* f = malloc(sizeof(future_t));
    pthread_mutex_init(&f->lock, NULL);
    pthread_cond_init(&f->cond, NULL);
    f->done = 0;
    f->result = NULL;
    return f;
}

static void future_destroy(future_t* f) {
    pthread_mutex_destroy(&f->lock);
    pthread_cond_destroy(&f->cond);
    free(f);
}

static int future_is_ready(future_t* f) {
    pthread_mutex_lock(&f->lock);
    int r = f->done;
    pthread_mutex_unlock(&f->lock);
    return r;
}

static void* future_get(future_t* f) {
    pthread_mutex_lock(&f->lock);
    while (!f->done)
        pthread_cond_wait(&f->cond, &f->lock);
    void* r = f->result;
    pthread_mutex_unlock(&f->lock);
    return r;
}

// Job queue

typedef struct {
    async_fn_t fn;
    void* arg;
    future_t* future;
} job_payload_t;

#define MAX_JOBS 128
static job_payload_t* queue[MAX_JOBS];
static int head = 0, tail = 0;
static pthread_mutex_t qlock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t qcond = PTHREAD_COND_INITIALIZER;

static int shutdown_requested = 0;

static void enqueue(job_payload_t* job) {
    pthread_mutex_lock(&qlock);
    int next_tail = (tail + 1) % MAX_JOBS;
    if (next_tail == head) {
        fprintf(stderr, "[async] Job queue overflow\n");
        pthread_mutex_unlock(&qlock);
        return;
    }
    queue[tail] = job;
    tail = next_tail;
    pthread_cond_signal(&qcond);
    pthread_mutex_unlock(&qlock);
}

static job_payload_t* dequeue() {
    pthread_mutex_lock(&qlock);
    while (head == tail && !shutdown_requested)
        pthread_cond_wait(&qcond, &qlock);

    if (shutdown_requested && head == tail) {
        pthread_mutex_unlock(&qlock);
        return NULL;
    }

    job_payload_t* job = queue[head];
    head = (head + 1) % MAX_JOBS;
    pthread_mutex_unlock(&qlock);
    return job;
}

static void* worker(void* _) {
    while (1) {
        job_payload_t* job = dequeue();
        if (!job) break;  // shutdown
        void* result = job->fn(job->arg);
        pthread_mutex_lock(&job->future->lock);
        job->future->result = result;
        job->future->done = 1;
        pthread_cond_signal(&job->future->cond);
        pthread_mutex_unlock(&job->future->lock);
        free(job->arg);
        free(job);
    }
    return NULL;
}

void async_init(int count) {
    for (int i = 0; i < count; ++i) {
        pthread_t t;
        pthread_create(&t, NULL, worker, NULL);
        pthread_detach(t);
    }
}

void async_shutdown() {
    pthread_mutex_lock(&qlock);
    shutdown_requested = 1;
    pthread_cond_broadcast(&qcond);
    pthread_mutex_unlock(&qlock);
}

// Job runtime

struct async_job {
    future_t* future;
    int done;
    async_callback_t callback;
    void* callback_ctx;
};

#define MAX_ACTIVE 256
static async_job_t* active_jobs[MAX_ACTIVE];
static int active_count = 0;

async_job_t* async_submit_with_callback(async_fn_t fn, void* arg, size_t size,
                                        async_callback_t cb, void* cb_ctx) {
    async_job_t* job = calloc(1, sizeof(async_job_t));
    job->future = future_create();
    job->callback = cb;
    job->callback_ctx = cb_ctx;

    void* arg_copy = malloc(size);
    memcpy(arg_copy, arg, size);

    job_payload_t* payload = malloc(sizeof(job_payload_t));
    payload->fn = fn;
    payload->arg = arg_copy;
    payload->future = job->future;

    enqueue(payload);

    if (active_count < MAX_ACTIVE) {
        active_jobs[active_count++] = job;
    } else {
        fprintf(stderr, "[async] Too many active jobs\n");
    }

    return job;
}

async_job_t* async_submit(async_fn_t fn, void* arg, size_t size) {
    return async_submit_with_callback(fn, arg, size, NULL, NULL);
}

void async_run() {
    while (active_count > 0) {
        for (int i = 0; i < active_count; ++i) {
            async_job_t* job = active_jobs[i];
            if (job->done) continue;

            if (future_is_ready(job->future)) {
                void* result = future_get(job->future);
                if (job->callback) {
                    job->callback(job->callback_ctx, result);
                }
                future_destroy(job->future);
                free(result);
                job->done = 1;
            }
        }

        int j = 0;
        for (int i = 0; i < active_count; ++i) {
            if (!active_jobs[i]->done) {
                active_jobs[j++] = active_jobs[i];
            } else {
                free(active_jobs[i]);
            }
        }
        active_count = j;

        usleep(1000);
    }
}
