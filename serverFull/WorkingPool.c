#include "WorkingPool.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <stdatomic.h>

#define BUFFER 64

Task taskQueue[BUFFER];
int taskCount = 0;
pthread_mutex_t mutexQueue;
pthread_cond_t condQueue;
pthread_cond_t condSubmit;
int running = 1;
int taskDone = 0;
int totalResult = 0;
int taskTaken = 0;



void executeTask(Task *task) {
	printf("started task\n");
	task->taskFunction(task->client_fd);
	__atomic_fetch_add(&taskDone, 1, __ATOMIC_SEQ_CST);


}

void submitTask(Task task) {
	printf("got task\n");
	pthread_mutex_lock(&mutexQueue);

	while (taskCount != taskTaken && taskCount % BUFFER == 0)
	{
		pthread_cond_wait(&condSubmit, &mutexQueue);
	}

	taskQueue[taskCount % BUFFER] = task;
	taskCount++;

	pthread_mutex_unlock(&mutexQueue);

	pthread_cond_signal(&condQueue);


}

void* startThread(void* args) {
	(void)args;
	printf("got thread\n");
	while (running) {
		Task task;

		pthread_mutex_lock(&mutexQueue);
		while (taskCount == taskTaken && running)
		{
			pthread_cond_wait(&condQueue, &mutexQueue);
		}
		if (!running)
		{
			pthread_mutex_unlock(&mutexQueue);
			break;
		}

		task = taskQueue[taskTaken % BUFFER];
		taskTaken++;

		if (taskTaken % BUFFER == 0)
		{
			pthread_cond_signal(&condSubmit);
		}

		pthread_mutex_unlock(&mutexQueue);
		executeTask(&task);

	}
	return NULL;
}