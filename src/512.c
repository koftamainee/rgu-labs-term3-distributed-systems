#include <fcntl.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define N_SEMS 3

int main() {
  sem_t* sems[N_SEMS];
  char sem_names[N_SEMS][16];

  for (int i = 0; i < N_SEMS; i++) {
    snprintf(sem_names[i], sizeof(sem_names[i]), "/sem%d", i);
    sems[i] = sem_open(sem_names[i], O_CREAT, 0666, 1);
    if (sems[i] == SEM_FAILED) {
      perror("sem_open");
      exit(1);
    }
  }

  printf("Locking all semaphores...\n");
  for (int i = 0; i < N_SEMS; i++) {
    sem_wait(sems[i]);
    printf("Semaphore %d locked\n", i);
  }

  sleep(2);

  printf("Unlocking all semaphores...\n");
  for (int i = 0; i < N_SEMS; i++) {
    sem_post(sems[i]);
    printf("Semaphore %d unlocked\n", i);
  }

  for (int i = 0; i < N_SEMS; i++) {
    sem_close(sems[i]);
    sem_unlink(sem_names[i]);
  }

  return 0;
}
