#ifndef WORKERPOOL_H
#define WORKERPOOL_H
#include <stddef.h>

typedef struct worker_pool worker_pool_t;
typedef struct tasks task_node;
typedef void (*workerPoolTask)(void *arg);

worker_pool_t *worker_pool_create(size_t num);
void worker_pool_destroy(worker_pool_t *pool);

void worker_pool_add_work(worker_pool_t *pool, workerPoolTask task, void *arg);




#endif