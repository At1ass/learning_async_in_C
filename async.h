#ifndef ASYNC_H
#define ASYNC_H

#include <stddef.h>

typedef struct async_job async_job_t;
typedef void* (*async_fn_t)(void* arg);
typedef void (*async_callback_t)(void* user_data, void* result);

void async_init(int worker_count);
async_job_t* async_submit(async_fn_t fn, void* arg, size_t arg_size);
async_job_t* async_submit_with_callback(async_fn_t fn, void* arg, size_t arg_size,
                                        async_callback_t cb, void* cb_ctx);
void async_run();
void async_shutdown();

#endif
