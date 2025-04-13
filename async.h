#ifndef ASYNC_H
#define ASYNC_H

typedef struct async_job async_job_t;
typedef void* (*async_fn_t)(void* arg);

void async_init(int worker_count);
async_job_t* async_submit(async_fn_t fn, void* arg);
const char* async_poll(async_job_t* job);
int async_ready(async_job_t* job);
void* async_result(async_job_t* job);
void async_destroy(async_job_t* job);

#endif
