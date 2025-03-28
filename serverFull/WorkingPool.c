#include "WorkingPool.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <stdatomic.h>
#include <stddef.h>

struct tasks {
	workerPoolTask task;
	void *arg;
	task_node *next;
};

struct worker_pool {
	pthread_t *threads;
	size_t num_threads;

	task_node *task_head;
	task_node *task_tail;

	pthread_mutex_t queue_mutex;
	pthread_cond_t task_available;
};

int running = 1;


void* startThread(void* args) {
	worker_pool_t *pool = (worker_pool_t *)args;
	printf("got thread\n");
	while (running) {
		pthread_mutex_lock(&pool->queue_mutex);
		while (pool->task_head == NULL && running)
		{
			printf("waiting\n");
			pthread_cond_wait(&pool->task_available, &pool->queue_mutex);
		}
		if (!running)
		{
			pthread_mutex_unlock(&pool->queue_mutex);
			break;
		}

		task_node *task = pool->task_head;
		if (task->next != NULL)
		{
			pool->task_head = task->next;
		}
		else
		{
			pool->task_head = NULL;
			pool->task_tail = NULL;
		}

		pthread_mutex_unlock(&pool->queue_mutex);
		if (task) {
			task->task(task->arg);
			free(task);
		}



	}
	return NULL;
}

worker_pool_t *worker_pool_create(size_t num)
{
	worker_pool_t *pool = malloc(sizeof(worker_pool_t));
	if (pool == NULL)
	{
		perror("malloc failed");
		return NULL;
	}

	pool->num_threads = num;
	pool->threads = malloc(sizeof(pthread_t) * pool->num_threads);
	if (pool->threads == NULL)
	{
		perror("malloc failed");
		free(pool);
		return NULL;
	}
	if (pthread_mutex_init(&pool->queue_mutex, NULL) != 0) {
		fprintf(stderr, "Mutex init failed\n");
		free(pool->threads);
		free(pool);
		return NULL;
	}

	if (pthread_cond_init(&pool->task_available, NULL) != 0) {
		fprintf(stderr, "Cond var init failed\n");
		pthread_mutex_destroy(&pool->queue_mutex);
		free(pool->threads);
		free(pool);
		return NULL;
	}
	pool->task_head = NULL;
	pool->task_tail = NULL;

	for (size_t i = 0; i < pool->num_threads; i++)
	{
		if (pthread_create(&pool->threads[i], NULL, &startThread, (void *)pool) != 0) {
			perror("Failed to create a thread");

			for (size_t j = 0; j < i; j++) {
				pthread_cancel(pool->threads[j]);
				pthread_join(pool->threads[j], NULL);
			}

			pthread_cond_destroy(&pool->task_available);
			pthread_mutex_destroy(&pool->queue_mutex);
			free(pool->threads);
			free(pool);
			return NULL;
		}
		printf("created a thread\n");
	}

	return pool;

}

void worker_pool_add_work(worker_pool_t *pool, workerPoolTask task, void *arg)
{
	printf("started to add work\n");
	task_node *new_task = malloc(sizeof(task_node));
	if (new_task == NULL)
	{
		perror("malloc failed");
		return;
	}

	new_task->task = task;
	new_task->arg = arg;
	new_task->next = NULL;

	pthread_mutex_lock(&pool->queue_mutex);

	if (pool->task_head == NULL)
	{
		pool->task_head = new_task;
		pool->task_tail = new_task;
	}
	else
	{
		pool->task_tail->next = new_task;
		pool->task_tail = new_task;
	}

	pthread_cond_signal(&pool->task_available);
	pthread_mutex_unlock(&pool->queue_mutex);
	printf("added work\n");
}

void worker_pool_destroy(worker_pool_t *pool)
{
	running = 0;
	pthread_cond_broadcast(&pool->task_available);
	for (size_t i = 0; i < pool->num_threads; i++)
	{
		if (pthread_join(pool->threads[i], NULL) != 0)
		{
			perror ("Failed to join a thread");
		}
	}
	free(pool->threads);
	task_node *ptr = pool->task_head;
	task_node *next;
	while (ptr != NULL)
	{
		next = ptr->next;
		free(ptr);
		ptr = next;
	}
	pthread_mutex_destroy(&pool->queue_mutex);
	pthread_cond_destroy(&pool->task_available);
	free(pool);
}
