#include "async.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

void* compute(void* arg) {
    int n = *(int*)arg;
    printf("[worker] Started: %d\n", n);
    sleep(10);
    int* res = malloc(sizeof(int));
    *res = n * 42;
    return res;
}

void on_done(void* ctx, void* result) {
    int v = *(int*)result;
    printf("[callback] Got result: %d\n", v);
    free(result);
}

int main() {
    async_init();

    future_t* futures[20];
    for (int i = 0; i < 20; ++i) {
        futures[i] = async_submit(compute, &i, sizeof(i));
        future_then(futures[i], on_done, NULL);
    }

    async_run();      // ждём все коллбэки
    async_shutdown(); // завершить воркеры

    for (int i = 0; i < 20; ++i) {
        future_destroy(futures[i]);
    }

    return 0;
}
