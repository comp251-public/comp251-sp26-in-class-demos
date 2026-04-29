/*
 * CORRECT THE RACE CONDITION IN THIS PROGRAM
 *
 * STEP 1: IDENTIFY THE CRITICAL SECTIONS
 * STEP 2: ADD MUTEX LOCKS AROUND CRITICIAL SECTIONS TO SYNCHRONIZE THREADS
 *
 */

#include <pthread.h>
#include <stdio.h>

#define NUM_ITERATIONS 100000 // number of operations for each thread

// shared Venmo balance

int venmo_balance = 1000; // initial balance $1000

void *deposit(void *arg) {
  for (int i = 0; i < NUM_ITERATIONS; ++i) {
    venmo_balance += 100; // deposit $100
  }
  return NULL;
}

void *withdraw(void *arg) {
  for (int i = 0; i < NUM_ITERATIONS; ++i) {
    venmo_balance -= 100; // withdraw $100
  }
  return NULL;
}

int main() {
  pthread_t depositer, withdrawer;

  pthread_create(&depositer, NULL, deposit, NULL);
  pthread_create(&withdrawer, NULL, withdraw, NULL);

  pthread_join(depositer, NULL);
  pthread_join(withdrawer, NULL);

  printf("Final Venmo balance: $%d\n", venmo_balance);

  return 0;
}
