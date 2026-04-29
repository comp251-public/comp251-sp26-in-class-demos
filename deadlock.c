/* CORRECT THIS CODE! */

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

pthread_mutex_t m;
int counter = 0;

void *worker(void *arg) {
  long tid = *(long *)arg; // read thread id from passed-in address

  for (int i = 0; i < 10000; i++) {
    pthread_mutex_lock(&m);

    /* thread 1 only updates first 5000 */
    if (tid == 1 && i > 5000) {
      break;
    }

    printf("thread %ld: counter=%d\n", tid, counter);
    counter++;

    pthread_mutex_unlock(&m);
  }

  return NULL;
}

int main(void) {
  pthread_t t0, t1;

  // thread id variables whose addresses we pass to pthread_create
  long tid0 = 0;
  long tid1 = 1;

  if (pthread_mutex_init(&m, NULL) != 0) {
    fprintf(stderr, "pthread_mutex_init failed\n");
    return 1;
  }

  if (pthread_create(&t0, NULL, worker, &tid0) != 0 ||
      pthread_create(&t1, NULL, worker, &tid1) != 0) {
    perror("pthread_create");
    pthread_mutex_destroy(&m);
    return 1;
  }

  pthread_join(t0, NULL);
  pthread_join(t1, NULL);

  pthread_mutex_destroy(&m);
  return 0;
}
