#pragma once
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdlib.h>

typedef struct package
{
    void *data_p;
    void (*func)(void *);
} package;

typedef struct node
{
    package packg;
    struct node *next;
} node;

// queue of pointers to data and function
typedef struct queue
{
    node *left;
    node *right;
    atomic_int queued_count;
    atomic_int completed_count;

} queue;

typedef struct worker_handle
{
    atomic_int *completed_count_p; // pointer to the varible containing the number of completed tasks
    atomic_bool *running;          // pointer the variable containing if the threads should terminate
    queue *task_queue;
} worker_handle;

typedef struct thread_pool
{
    queue *tasks; // linked list
    int thread_count;
    pthread_t *tids; // fixed width array
    atomic_bool running;
    worker_handle *w_handle;

} thread_pool;

bool enqueue(queue *q, package p);
thread_pool *create_pool(int thread_count);
void destroy_pool(thread_pool *pool);
