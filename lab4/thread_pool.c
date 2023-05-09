#include "thread_pool.h"

#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define TASK_WAITING_PUSH 0
#define TASK_WAITING_THREAD 1
#define TASK_RUNNING 2
#define TASK_FINISHED 3
#define TASK_JOINED 4

struct thread_task {
  thread_task_f function;
  void *arg;
  int task_state;
  pthread_t thread_worker;
  void *result;

  pthread_cond_t *task_cond;
  pthread_mutex_t *task_mutex;
};

struct thread_pool {
  pthread_t *threads;

  int max_thread_count;
  int active_thread_count;
  int busy_thread_count;
  struct thread_task **task_queue;
  int task_count;
  bool is_shutdown;
  pthread_mutex_t *mutex;
  pthread_cond_t *task_cond;
  pthread_condattr_t *attr;
};

typedef struct {
  struct thread_pool *pool;
  int thread_id;
} thread_args_t;

void *worker_thread(void *arg) {
  thread_args_t *args = (thread_args_t *)arg;
  struct thread_pool *pool = args->pool;

  while (true) {
    pthread_mutex_lock(pool->mutex);

    while (!pool->task_count && !pool->is_shutdown) {
      pthread_cond_wait(pool->task_cond, pool->mutex);
    }

    if (pool->is_shutdown) {
      free(args);

      pthread_mutex_unlock(pool->mutex);
      pthread_exit(NULL);
    }

    struct thread_task *task = pool->task_queue[--pool->task_count];
    ++pool->busy_thread_count;

    pthread_mutex_lock(task->task_mutex);
    task->thread_worker = pool->threads[args->thread_id];

    pthread_mutex_unlock(pool->mutex);
    task->task_state = TASK_RUNNING;
    thread_task_f function = task->function;
    void *arg = task->arg;

    pthread_mutex_unlock(task->task_mutex);

    void *result = function(arg);
    pthread_mutex_lock(task->task_mutex);
    pthread_mutex_lock(pool->mutex);

    task->result = result;
    task->task_state = TASK_FINISHED;

    pthread_cond_signal(task->task_cond);
    pthread_mutex_unlock(task->task_mutex);

    pool->busy_thread_count--;
    pthread_mutex_unlock(pool->mutex);
  }
}

int thread_pool_new(int max_thread_count, struct thread_pool **pool) {
  if (max_thread_count > TPOOL_MAX_THREADS || max_thread_count <= 0)
    return TPOOL_ERR_INVALID_ARGUMENT;

  struct thread_pool *new_pool = malloc(sizeof(struct thread_pool));
  if (!new_pool) return -1;

  new_pool->max_thread_count = max_thread_count;
  new_pool->active_thread_count = 0;
  new_pool->busy_thread_count = 0;
  new_pool->is_shutdown = false;
  new_pool->task_queue = malloc(sizeof(struct thread_task *) * TPOOL_MAX_TASKS);

  if (!new_pool->task_queue) {
    free(new_pool);
    return -1;
  }

  new_pool->mutex = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
  if (pthread_mutex_init(new_pool->mutex, NULL) != 0) {
    free(new_pool->task_queue);
    free(new_pool->threads);
    free(new_pool);
    return -1;
  }

  new_pool->task_cond = (pthread_cond_t *)malloc(sizeof(pthread_cond_t));
  new_pool->attr = (pthread_condattr_t *)malloc(sizeof(pthread_condattr_t));

  pthread_condattr_init(new_pool->attr);
  pthread_condattr_setclock(new_pool->attr, CLOCK_MONOTONIC);
  pthread_cond_init(new_pool->task_cond, new_pool->attr);

  new_pool->task_count = 0;
  new_pool->threads = malloc(sizeof(pthread_t) * max_thread_count);

  if (!new_pool->threads) {
    free(new_pool->task_queue);
    free(new_pool);
    return -1;
  }

  *pool = new_pool;
  return 0;
}

int thread_pool_thread_count(const struct thread_pool *pool) {
  pthread_mutex_lock(pool->mutex);
  int active_threads = pool->active_thread_count;
  pthread_mutex_unlock(pool->mutex);
  return active_threads;
}

int thread_pool_delete(struct thread_pool *pool) {
  pthread_mutex_lock(pool->mutex);

  if (pool->task_count > 0 || pool->busy_thread_count > 0) {
    pthread_mutex_unlock(pool->mutex);
    return TPOOL_ERR_HAS_TASKS;
  }

  pool->is_shutdown = true;

  pthread_cond_broadcast(pool->task_cond);
  pthread_mutex_unlock(pool->mutex);

  for (int i = 0; i < pool->active_thread_count; i++) {
    pthread_join(pool->threads[i], NULL);
  }

  free(pool->task_queue);
  free(pool->threads);

  pthread_mutex_destroy(pool->mutex);
  free(pool->mutex);

  pthread_cond_destroy(pool->task_cond);
  free(pool->task_cond);

  pthread_condattr_destroy(pool->attr);
  free(pool->attr);

  free(pool);
  return 0;
}

int thread_pool_push_task(struct thread_pool *pool, struct thread_task *task) {
  pthread_mutex_lock(pool->mutex);
  if (pool->task_count >= TPOOL_MAX_TASKS) {
    pthread_mutex_unlock(pool->mutex);
    return TPOOL_ERR_TOO_MANY_TASKS;
  }

  pthread_mutex_lock(task->task_mutex);

  pool->task_queue[pool->task_count++] = task;
  task->task_state = TASK_WAITING_THREAD;

  if (pool->busy_thread_count == pool->active_thread_count &&
      pool->active_thread_count < pool->max_thread_count) {
    thread_args_t *args = malloc(sizeof(thread_args_t));
    args->thread_id = pool->active_thread_count;
    args->pool = pool;
    pthread_create(&(pool->threads[pool->active_thread_count]), NULL,
                   worker_thread, (void *)args);
    pool->active_thread_count++;
  }

  pthread_mutex_unlock(pool->mutex);
  pthread_mutex_unlock(task->task_mutex);
  pthread_cond_signal(pool->task_cond);

  return 0;
}

int thread_task_new(struct thread_task **task, thread_task_f function,
                    void *arg) {
  *task = (struct thread_task *)malloc(sizeof(struct thread_task));

  (*task)->task_mutex = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
  pthread_mutex_init((*task)->task_mutex, NULL);

  (*task)->task_cond = (pthread_cond_t *)malloc(sizeof(pthread_cond_t));
  pthread_cond_init((*task)->task_cond, NULL);

  pthread_mutex_lock((*task)->task_mutex);
  (*task)->function = function;
  (*task)->arg = arg;
  (*task)->task_state = TASK_WAITING_PUSH;
  pthread_mutex_unlock((*task)->task_mutex);
  return 0;
}

bool thread_task_is_finished(const struct thread_task *task) {
  if (!task) return TPOOL_ERR_INVALID_ARGUMENT;
  return task->task_state == TASK_FINISHED;
}

bool thread_task_is_running(const struct thread_task *task) {
  if (!task) return TPOOL_ERR_INVALID_ARGUMENT;
  return task->task_state == TASK_RUNNING;
}

int thread_task_join(struct thread_task *task, void **result) {
  if (!task) return TPOOL_ERR_INVALID_ARGUMENT;

  pthread_mutex_lock(task->task_mutex);

  if (task->task_state == TASK_WAITING_PUSH) {
    pthread_mutex_unlock(task->task_mutex);
    return TPOOL_ERR_TASK_NOT_PUSHED;
  }

  while (task->task_state != TASK_FINISHED)
    pthread_cond_wait(task->task_cond, task->task_mutex);

  *result = task->result;
  task->task_state = TASK_JOINED;

  pthread_mutex_unlock(task->task_mutex);
  return 0;
}

int thread_task_delete(struct thread_task *task) {
  if (!task) return TPOOL_ERR_INVALID_ARGUMENT;

  pthread_mutex_lock(task->task_mutex);

  if (task->task_state > TASK_WAITING_PUSH && task->task_state < TASK_JOINED) {
    pthread_mutex_unlock(task->task_mutex);
    return TPOOL_ERR_TASK_IN_POOL;
  }

  pthread_mutex_unlock(task->task_mutex);

  pthread_cond_destroy(task->task_cond);
  free(task->task_cond);

  pthread_mutex_destroy(task->task_mutex);
  free(task->task_mutex);

  free(task);
  return 0;
}
