#include "async.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

void* heavy_work(void* arg) {
    int n = *(int*)arg;
    printf("[worker] got input: %d\n", n);
    sleep(4);
    int* res = malloc(sizeof(int));
    *res = n * 42;
    return res;
}

void on_done(void* ctx, void* result) {
    int value = *(int*)result;
    printf("[callback] Result = %d\n", value);
}

int main() {
    async_init(3);

    int a = 5, b = 10, c = 15, d = 20;
    async_submit_with_callback(heavy_work, &a, sizeof(a), on_done, NULL);
    async_submit_with_callback(heavy_work, &b, sizeof(b), on_done, NULL);
    async_submit_with_callback(heavy_work, &c, sizeof(c), on_done, NULL);
    async_submit_with_callback(heavy_work, &d, sizeof(d), on_done, NULL);

    async_run();
    async_shutdown();
    return 0;
}
