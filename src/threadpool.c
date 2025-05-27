#include "threadpool.h"

#include <assert.h>

pthread_mutex_t queue_mutex;
package dequeue(queue *q);

void *worker_start(void *handle)
{
    worker_handle *h = handle;
    package input;
    input.data_p = NULL;
    input.func = NULL;

    while (*(h->running)) {
        input = dequeue(h->task_queue);
        if (input.func == NULL) {
            sched_yield();
            continue;
        }
        input.func(input.data_p);
        (*(h->completed_count_p))++;
    }
    return handle;
}

void queue_init(queue *q)
{
    q->left = NULL;
    q->right = NULL;
    pthread_mutex_init(&queue_mutex, NULL);
}

thread_pool *create_pool(int thread_count)
{
    thread_pool *pool = NULL;
    queue *tasks = NULL;
    pthread_t *tids = NULL;

    pool = calloc(1, sizeof(thread_pool));
    tasks = calloc(1, sizeof(queue));
    tids = calloc(thread_count, sizeof(pthread_t));

    if (!pool || !tasks || !tids) {
        goto err;
    }

    pool->tasks = tasks;
    pool->tids = tids;
    pool->thread_count = thread_count;
    pool->running = true;

    queue_init(pool->tasks);

    worker_handle *h = calloc(1, sizeof(worker_handle));
    if (!h) {
        goto err;
    }
    pool->w_handle = h;
    h->task_queue = pool->tasks;
    h->running = &(pool->running);
    h->completed_count_p = &(pool->tasks->completed_count);
    for (int i = 0; i < thread_count; i++) {
        if (pthread_create(&(pool->tids[i]), 0, worker_start, h) != 0) {
            goto err;
        }
    }

    return pool;

err:

    if (pool) {
        pool->running = false;
        if (pool->tids) {
            for (int i = 0; pool->tids[i] != 0; i++) {
                pthread_join(pool->tids[i], NULL);
            }
        }
    }
    free(pool);
    free(tasks);
    free(tids);

    return NULL;
}

void destroy_queue(queue *q)
{
    while (dequeue(q).data_p) {
        continue;
    }
    free(q);
}

void destroy_pool(thread_pool *pool)
{
    pool->running = false;
    for (int i = 0; i < pool->thread_count; i++) {
        pthread_join(pool->tids[i], NULL);
    }
    free(pool->tids);
    destroy_queue(pool->tasks);
    free(pool->w_handle);
    free(pool);
}

// atomic
package dequeue(queue *q)
{
    pthread_mutex_lock(&queue_mutex);
    assert(q);
    package result;
    result.data_p = NULL;
    result.func = NULL;
    if (q->left) {
        result = q->left->packg;

        node *tmp = q->left;
        q->left = q->left->next;
        free(tmp);
    }
    if (q->left == NULL) {
        q->right = NULL;
    }
    pthread_mutex_unlock(&queue_mutex);
    return result;
}

// atomic
bool enqueue(queue *q, package p)
{
    pthread_mutex_lock(&queue_mutex);
    assert(q);
    assert(p.data_p);

    node *new_node = calloc(1, sizeof(node));
    if (!new_node) {
        pthread_mutex_unlock(&queue_mutex);
        return false;
    }
    new_node->packg = p;

    if (q->right == NULL) {
        q->left = q->right = new_node;
    } else {
        q->right->next = new_node;
        q->right = q->right->next;
    }

    pthread_mutex_unlock(&queue_mutex);
    return true;
}
