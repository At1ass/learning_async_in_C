#include "async.h"
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

struct future {
    pthread_mutex_t lock;
    pthread_cond_t cond;
    int done;
    void* result;
    future_continuation_t continuation;
    void* continuation_ctx;
    int continuation_called;
};

#define MAX_PENDING 512
static future_t* pending_futures[MAX_PENDING];
static int pending_count = 0;

static void pending_add(future_t* f) {
    if (pending_count < MAX_PENDING) {
        pending_futures[pending_count++] = f;
    } else {
        fprintf(stderr, "[async] Too many pending futures\n");
    }
}

static future_t* future_create() {
    future_t* f = malloc(sizeof(future_t));
    pthread_mutex_init(&f->lock, NULL);
    pthread_cond_init(&f->cond, NULL);
    f->done = 0;
    f->result = NULL;
    f->continuation = NULL;
    f->continuation_ctx = NULL;
    f->continuation_called = 0;
    return f;
}

static void future_set_result(future_t* f, void* result) {
    pthread_mutex_lock(&f->lock);
    f->result = result;
    f->done = 1;
    pthread_cond_signal(&f->cond);

    if (f->continuation && !f->continuation_called) {
        f->continuation_called = 1;
        future_continuation_t cb = f->continuation;
        void* ctx = f->continuation_ctx;
        pthread_mutex_unlock(&f->lock);
        cb(ctx, result);
        return;
    }

    pthread_mutex_unlock(&f->lock);
}

int future_is_ready(future_t* f) {
    pthread_mutex_lock(&f->lock);
    int r = f->done;
    pthread_mutex_unlock(&f->lock);
    return r;
}

void* future_get(future_t* f) {
    pthread_mutex_lock(&f->lock);
    while (!f->done)
        pthread_cond_wait(&f->cond, &f->lock);
    void* r = f->result;
    pthread_mutex_unlock(&f->lock);
    return r;
}

void future_then(future_t* f, future_continuation_t cb, void* ctx) {
    pthread_mutex_lock(&f->lock);
    if (f->done) {
        if (!f->continuation_called) {
            f->continuation_called = 1;
            void* result = f->result;
            pthread_mutex_unlock(&f->lock);
            cb(ctx, result);
        } else {
            pthread_mutex_unlock(&f->lock);
        }
        return;
    }

    f->continuation = cb;
    f->continuation_ctx = ctx;
    pending_add(f);
    pthread_mutex_unlock(&f->lock);
}

void future_destroy(future_t* f) {
    pthread_mutex_destroy(&f->lock);
    pthread_cond_destroy(&f->cond);
    free(f);
}

// ==== Worker Pool & Queue ====

typedef struct {
    async_fn_t fn;
    void* arg;
    future_t* future;
} job_payload_t;

#define MAX_JOBS 512
static job_payload_t* queue[MAX_JOBS];
static int head = 0, tail = 0;
static pthread_mutex_t qlock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t qcond = PTHREAD_COND_INITIALIZER;
static int shutdown_requested = 0;

static void enqueue(job_payload_t* job) {
    pthread_mutex_lock(&qlock);
    int next = (tail + 1) % MAX_JOBS;
    if (next == head) {
        fprintf(stderr, "[async] Job queue overflow\n");
        pthread_mutex_unlock(&qlock);
        return;
    }
    queue[tail] = job;
    tail = next;
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
        if (!job) break;
        void* result = job->fn(job->arg);
        future_set_result(job->future, result);
        free(job->arg);
        free(job);
    }
    return NULL;
}

void async_init() {
    long cpus = sysconf(_SC_NPROCESSORS_ONLN);
    if (cpus < 1) cpus = 2;
    for (long i = 0; i < cpus; ++i) {
        pthread_t t;
        pthread_create(&t, NULL, worker, NULL);
        pthread_detach(t);
    }
    printf("[async] Initialized with %ld worker threads\n", cpus);
}

void async_shutdown() {
    pthread_mutex_lock(&qlock);
    shutdown_requested = 1;
    pthread_cond_broadcast(&qcond);
    pthread_mutex_unlock(&qlock);
}

// ==== Task submission ====

future_t* async_submit(async_fn_t fn, void* arg, size_t size) {
    future_t* f = future_create();
    void* arg_copy = malloc(size);
    if (!arg_copy) {
        fprintf(stderr, "[async] malloc failed\n");
        future_destroy(f);
        return NULL;
    }
    memcpy(arg_copy, arg, size);

    job_payload_t* payload = malloc(sizeof(job_payload_t));
    payload->fn = fn;
    payload->arg = arg_copy;
    payload->future = f;

    enqueue(payload);
    return f;
}

// ==== Manual run loop ====

void async_run() {
    int still_active = 1;
    while (still_active) {
        still_active = 0;

        for (int i = 0; i < pending_count; ++i) {
            future_t* f = pending_futures[i];
            pthread_mutex_lock(&f->lock);
            if (f->done && !f->continuation_called && f->continuation) {
                f->continuation_called = 1;
                future_continuation_t cb = f->continuation;
                void* ctx = f->continuation_ctx;
                void* result = f->result;
                pthread_mutex_unlock(&f->lock);
                cb(ctx, result);
            } else {
                if (!f->done) still_active = 1;
                pthread_mutex_unlock(&f->lock);
            }
        }

        usleep(1000); // 1 ms
    }
}
