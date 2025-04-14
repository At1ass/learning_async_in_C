#ifndef ASYNC_H
#define ASYNC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

typedef struct future future_t;
typedef void* (*async_fn_t)(void* arg);
typedef void (*future_continuation_t)(void* ctx, void* result);

void async_init();
void async_shutdown();
future_t* async_submit(async_fn_t fn, void* arg, size_t arg_size);
void future_then(future_t* f, future_continuation_t cb, void* ctx);
int future_is_ready(future_t* f);
void* future_get(future_t* f);
void async_run();
void future_destroy(future_t* f);

#ifdef __cplusplus
}
#endif

#endif // ASYNC_H
