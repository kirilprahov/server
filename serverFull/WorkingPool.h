#ifndef WORKERPOOL_H
#define WORKERPOOL_H

typedef struct Task {
    void (*taskFunction)(int);
    int client_fd;
} Task;

void* startThread(void* args);
void submitTask(Task task);
void executeTask(Task *task);

#endif