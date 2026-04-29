#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

#define N_THREADS 2
#define N_LOCKS 2
#define TIMEOUT 1

typedef struct {
  pthread_t thread;
  int id;
} thread_info_t;

typedef void *(*thread_func_t)(void *);

pthread_mutex_t locks[2];

void do_task(thread_info_t *thread_info) {
  printf("[thread %d] started task...\n", thread_info->id);
  fflush(stdout);

  sleep(TIMEOUT);

  printf("[thread %d] completed task...\n", thread_info->id);
  fflush(stdout);
}

void acquire_lock(int id, thread_info_t *thread_info) {
  printf("[thread %d] attempting to acquire lock %d...\n", thread_info->id, id);
  fflush(stdout);

  pthread_mutex_lock(&locks[id]);

  printf("[thread %d] acquired lock %d...\n", thread_info->id, id);
  fflush(stdout);
}

void release_lock(int id, thread_info_t *thread_info) {
  printf("[thread %d] releasing lock %d...\n", thread_info->id, id);
  fflush(stdout);

  pthread_mutex_unlock(&locks[id]);
}

void *worker1(void *thread_info) {
  while (1) {
    acquire_lock(1, thread_info);
    acquire_lock(2, thread_info);

    do_task(thread_info);

    release_lock(2, thread_info);
    release_lock(1, thread_info);
  }

  return NULL;
}

void *worker2(void *thread_info) {
  while (1) {
    acquire_lock(2, thread_info);
    acquire_lock(1, thread_info);

    do_task(thread_info);

    release_lock(1, thread_info);
    release_lock(2, thread_info);
  }

  return NULL;
}

int main() {
  thread_info_t thread_info[N_THREADS];
  thread_func_t thread_funcs[N_THREADS] = {worker1, worker2};

  int i;

  /* initialize mutexes */
  for (i = 0; i < N_LOCKS; i++) {
    pthread_mutex_init(&locks[i], NULL);
  }

  /* create threads */
  for (i = 0; i < N_THREADS; i++) {
    thread_info[i].id = i;
    pthread_create(&thread_info[i].thread, NULL, thread_funcs[i],
                   &thread_info[i]);
  }

  /* wait for all threads to complete */
  for (i = 0; i < N_THREADS; i++) {
    pthread_join(thread_info[i].thread, NULL);
  }

  return 0;
}
