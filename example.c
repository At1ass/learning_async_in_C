#include "async.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

void* heavy_work(void* arg) {
    int n = *(int*)arg;
    printf("[worker] Received input: %d\n", n);
    sleep(2);
    int* result = malloc(sizeof(int));
    *result = n * 42;
    return result;
}

int main() {
    async_init(2);  // worker threads

    int a = 5, b = 10, c = 15;

    async_job_t* job1 = async_submit(heavy_work, &a);
    async_job_t* job2 = async_submit(heavy_work, &b);
    async_job_t* job3 = async_submit(heavy_work, &c);

    for (int tick = 0; tick < 10; ++tick) {
        printf("Tick %d:\n", tick);

        const char* msg1 = async_poll(job1);
        const char* msg2 = async_poll(job2);
        const char* msg3 = async_poll(job3);

        if (msg1) printf("  job1: %s\n", msg1);
        if (msg2) printf("  job2: %s\n", msg2);
        if (msg3) printf("  job3: %s\n", msg3);

        sleep(1);
    }

    async_destroy(job1);
    async_destroy(job2);
    async_destroy(job3);

    return 0;
}
