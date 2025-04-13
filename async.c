#include "async.h"
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

// Future Implementation

typedef struct future {
    pthread_mutex_t lock;
    pthread_cond_t cond;
    int done;
    void* result;
} future_t;

typedef struct {
    async_fn_t fn;
    void* arg;
    future_t* future;
} job_payload_t;

#define MAX_JOBS 64
static job_payload_t* queue[MAX_JOBS];
static int head = 0, tail = 0;
static pthread_mutex_t qlock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t qcond = PTHREAD_COND_INITIALIZER;

static void enqueue(job_payload_t* job) {
    pthread_mutex_lock(&qlock);
    queue[tail] = job;
    tail = (tail + 1) % MAX_JOBS;
    pthread_cond_signal(&qcond);
    pthread_mutex_unlock(&qlock);
}

static job_payload_t* dequeue() {
    pthread_mutex_lock(&qlock);
    while (head == tail)
        pthread_cond_wait(&qcond, &qlock);
    job_payload_t* job = queue[head];
    head = (head + 1) % MAX_JOBS;
    pthread_mutex_unlock(&qlock);
    return job;
}

static void* worker(void* _) {
    while (1) {
        job_payload_t* job = dequeue();
        void* result = job->fn(job->arg);
        pthread_mutex_lock(&job->future->lock);
        job->future->result = result;
        job->future->done = 1;
        pthread_cond_signal(&job->future->cond);
        pthread_mutex_unlock(&job->future->lock);
        free(job);
    }
    return NULL;
}

void async_init(int worker_count) {
    for (int i = 0; i < worker_count; ++i) {
        pthread_t t;
        pthread_create(&t, NULL, worker, NULL);
        pthread_detach(t);
    }
}

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

// Coroutine API

#define DECLARE() int state
#define INIT(self) (self)->state = 0
#define BEGIN(self) switch ((self)->state) { case 0:
#define YIELD(self, val) do { (self)->state = __LINE__; return val; case __LINE__:; } while (0)
#define END }

// async_job API

struct async_job {
    DECLARE();
    future_t* f;
    void* result;
    int started;
    int done;
    char buf[64];
    async_fn_t fn;
    void* arg;
};

async_job_t* async_submit(async_fn_t fn, void* arg) {
    async_job_t* job = calloc(1, sizeof(async_job_t));
    job->fn = fn;
    job->arg = arg;
    INIT(job);
    return job;
}

const char* async_poll(async_job_t* job) {
    if (job->done) return NULL;

    BEGIN(job);

    if (!job->started) {
        future_t* f = future_create();
        job_payload_t* payload = malloc(sizeof(job_payload_t));
        payload->fn = job->fn;

        void* arg_copy = malloc(sizeof(int));
        memcpy(arg_copy, job->arg, sizeof(int));
        payload->arg = arg_copy;

        payload->future = f;
        job->f = f;
        job->started = 1;
        enqueue(payload);
        YIELD(job, "Started async job...");
    }

    while (!future_is_ready(job->f)) {
        YIELD(job, "Waiting...");
    }

    job->result = future_get(job->f);
    snprintf(job->buf, sizeof(job->buf), "Result = %d", *(int*)job->result);
    YIELD(job, job->buf);

    job->done = 1;
    future_destroy(job->f);
    free(job->result);

    END;
    return NULL;
}

int async_ready(async_job_t* job) {
    return job->done;
}

void* async_result(async_job_t* job) {
    return job->result;
}

void async_destroy(async_job_t* job) {
    free(job);
}
