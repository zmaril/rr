/* -*- Mode: C; tab-width: 8; c-basic-offset: 2; indent-tabs-mode: nil; -*- */

#include "rrutil.h"

static pthread_mutex_t mutex;

static void* run_thread(void* p) {
  test_assert(0 == pthread_mutex_lock(&mutex));
  return NULL;
}

int main(int argc, char* argv[]) {
  pthread_mutexattr_t attr;
  pthread_t thread;

  pthread_mutexattr_init(&attr);
  pthread_mutexattr_setrobust(&attr, PTHREAD_MUTEX_ROBUST);
  pthread_mutex_init(&mutex, &attr);

  pthread_create(&thread, NULL, run_thread, NULL);
  pthread_join(thread, NULL);

  test_assert(EOWNERDEAD == pthread_mutex_lock(&mutex));
  pthread_mutex_consistent(&mutex);
  test_assert(0 == pthread_mutex_unlock(&mutex));
  test_assert(0 == pthread_mutex_lock(&mutex));

  atomic_puts("EXIT-SUCCESS");
  return 0;
}