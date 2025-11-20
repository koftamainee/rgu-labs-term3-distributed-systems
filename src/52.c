#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

typedef enum { empty, men, women } state_t;

static state_t state = empty;
static int inside = 0;
static int N = 0;

sem_t mutex;
sem_t slots;

#define NUM_THREADS 20

typedef struct {
  state_t gender;
  const char* label;
} thread_arg_t;

void* thread(void* arg);

void man_wants_to_enter();
void woman_wants_to_enter();
void man_leaves();
void woman_leaves();

void person_wants_to_enter(state_t gender);
void person_leaves(state_t gender);

void check_sem(int ret, const char* msg) {
  if (ret != 0) {
    perror(msg);
    exit(EXIT_FAILURE);
  }
}

int main(int argc, char* argv[]) {
  if (argc < 2) {
    fprintf(stderr, "Usage: %s <N>\n", argv[0]);
    return 1;
  }

  N = atoi(argv[1]);
  if (N <= 0) {
    fprintf(stderr, "N must be positive.\n");
    return 1;
  }

  check_sem(sem_init(&mutex, 0, 1), "sem_init mutex");
  check_sem(sem_init(&slots, 0, N), "sem_init slots");

  pthread_t threads[NUM_THREADS];
  thread_arg_t args[NUM_THREADS];

  for (int i = 0; i < NUM_THREADS / 2; i++) {
    args[i].gender = men;
    args[i].label = "Man";
    if (pthread_create(&threads[i], NULL, thread, &args[i]) != 0) {
      perror("pthread_create men_thread");
      exit(EXIT_FAILURE);
    }
  }

  for (int i = NUM_THREADS / 2; i < NUM_THREADS; i++) {
    args[i].gender = women;
    args[i].label = "Woman";
    if (pthread_create(&threads[i], NULL, thread, &args[i]) != 0) {
      perror("pthread_create women_thread");
      exit(EXIT_FAILURE);
    }
  }

  for (int i = 0; i < NUM_THREADS; i++) {
    if (pthread_join(threads[i], NULL) != 0) {
      perror("pthread_join");
      exit(EXIT_FAILURE);
    }
  }

  check_sem(sem_destroy(&mutex), "sem_destroy mutex");
  check_sem(sem_destroy(&slots), "sem_destroy slots");

  return 0;
}

void* thread(void* arg) {
  thread_arg_t* t = (thread_arg_t*)arg;

  if (t->gender == men)
    man_wants_to_enter();
  else
    woman_wants_to_enter();

  usleep(500000);

  if (t->gender == men)
    man_leaves();
  else
    woman_leaves();

  return NULL;
}

void man_wants_to_enter() { person_wants_to_enter(men); }
void woman_wants_to_enter() { person_wants_to_enter(women); }
void man_leaves() { person_leaves(men); }
void woman_leaves() { person_leaves(women); }

void person_wants_to_enter(state_t gender) {
  int entered = 0;
  while (!entered) {
    check_sem(sem_wait(&mutex), "sem_wait mutex enter check");
    if (state == empty || state == gender) {
      state = gender;
      check_sem(sem_post(&mutex), "sem_post mutex enter set");
      check_sem(sem_wait(&slots), "sem_wait slots");

      check_sem(sem_wait(&mutex), "sem_wait mutex inside increment");
      inside++;
      printf("%s entered. Inside: %d\n", (gender == men ? "Man" : "Woman"), inside);
      entered = 1;
      check_sem(sem_post(&mutex), "sem_post mutex enter done");
    } else {
      check_sem(sem_post(&mutex), "sem_post mutex enter failed");
    }

    if (!entered) usleep(1000);
  }
}

void person_leaves(state_t gender) {
  check_sem(sem_wait(&mutex), "sem_wait mutex leave");
  inside--;
  printf("%s left. Inside: %d\n", (gender == men ? "Man" : "Woman"), inside);
  if (inside == 0) state = empty;
  check_sem(sem_post(&mutex), "sem_post mutex leave");
  check_sem(sem_post(&slots), "sem_post slots leave");
}
