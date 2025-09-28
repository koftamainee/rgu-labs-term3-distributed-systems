#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <wait.h>

#define MAX_N 1024

typedef struct {
  int count;
  int num_alive;
  int alive[MAX_N];
} message_t;

char **create_fifos(int process_count, char **report_fifo, char **ready_fifo);
void destroy_fifos(char **fifo_paths, int process_count,
                   const char *report_fifo, const char *ready_fifo);
int child_process(int idx, int process_count, char **fifo_paths,
                  const char *ready_fifo);
int parent_process(int process_count, int step, char **fifo_paths,
                   const char *report_fifo, const char *ready_fifo, int *pids);

int main(int argc, char *argv[]) {
  if (argc != 3) {
    fprintf(stderr, "Invalid usage\n");
    return 1;
  }

  int process_count = atoi(argv[1]);
  int step = atoi(argv[2]);
  if (process_count < 0 || step <= 1 || step >= process_count) {
    fprintf(stderr, "Invalid arguments\n");
    return 1;
  }

  int pids[process_count];

  char *report_fifo = NULL;
  char *ready_fifo = NULL;
  char **fifo_paths = create_fifos(process_count, &report_fifo, &ready_fifo);
  if (fifo_paths == NULL) {
    return 1;
  }

  for (int i = 0; i < process_count; i++) {
    pid_t pid = fork();
    if (pid < 0) {
      destroy_fifos(fifo_paths, process_count, report_fifo, ready_fifo);
      for (int j = 0; j < i; j++) {
        kill(pids[j], SIGTERM);
      }
      perror("fork");
      return 1;
    }

    if (pid > 0) {
      pids[i] = pid;
    }

    if (pid == 0) {
      return child_process(i, process_count, fifo_paths, ready_fifo);
    }
  }

  return parent_process(process_count, step, fifo_paths, report_fifo,
                        ready_fifo, pids);
}

char **create_fifos(int process_count, char **report_fifo, char **ready_fifo) {
  pid_t parent_pid = getpid();

  char base_fifo[128];
  snprintf(base_fifo, sizeof(base_fifo), "/tmp/joseph_%d_fifo", parent_pid);

  *report_fifo = (char *)malloc(128);
  if (!*report_fifo) {
    perror("malloc report_fifo");
    return NULL;
  }
  snprintf(*report_fifo, 128, "/tmp/joseph_%d_report", parent_pid);

  *ready_fifo = (char *)malloc(128);
  if (!*ready_fifo) {
    free(*report_fifo);
    perror("malloc ready_fifo");
    return NULL;
  }
  snprintf(*ready_fifo, 128, "/tmp/joseph_%d_ready", parent_pid);

  unlink(*report_fifo);
  unlink(*ready_fifo);

  if (mkfifo(*report_fifo, 0666) != 0) {
    perror("mkfifo report");
    free(*report_fifo);
    free(*ready_fifo);
    return NULL;
  }

  if (mkfifo(*ready_fifo, 0666) != 0) {
    unlink(*report_fifo);
    perror("mkfifo ready");
    free(*report_fifo);
    free(*ready_fifo);
    return NULL;
  }

  char **fifo_paths = (char **)malloc(sizeof(char *) * process_count);
  if (fifo_paths == NULL) {
    unlink(*report_fifo);
    unlink(*ready_fifo);
    free(*report_fifo);
    free(*ready_fifo);
    perror("malloc fifo_paths");
    return NULL;
  }

  for (int i = 0; i < process_count; i++) {
    fifo_paths[i] = (char *)malloc(256);
    if (!fifo_paths[i]) {
      for (int j = 0; j < i; j++) {
        free(fifo_paths[j]);
        unlink(fifo_paths[j]);
      }
      free(fifo_paths);
      unlink(*report_fifo);
      unlink(*ready_fifo);
      free(*report_fifo);
      free(*ready_fifo);
      perror("malloc fifo name");
      return NULL;
    }

    snprintf(fifo_paths[i], 256, "%s_%d", base_fifo, i);

    if (mkfifo(fifo_paths[i], 0666) != 0) {
      for (int j = 0; j <= i; j++) {
        free(fifo_paths[j]);
        unlink(fifo_paths[j]);
      }
      free(fifo_paths);
      unlink(*report_fifo);
      unlink(*ready_fifo);
      free(*report_fifo);
      free(*ready_fifo);
      perror("mkfifo ring");
      return NULL;
    }
  }

  return fifo_paths;
}

void destroy_fifos(char **fifo_paths, int process_count,
                   const char *report_fifo, const char *ready_fifo) {
  if (fifo_paths) {
    for (int i = 0; i < process_count; ++i) {
      if (fifo_paths[i]) {
        unlink(fifo_paths[i]);
        free(fifo_paths[i]);
      }
    }
    free(fifo_paths);
  }

  if (report_fifo) {
    unlink(report_fifo);
    free((void *)report_fifo);
  }

  if (ready_fifo) {
    unlink(ready_fifo);
    free((void *)ready_fifo);
  }
}

int child_process(int idx, int process_count, char **fifo_paths,
                  const char *ready_fifo) {
  int fd_in = open(fifo_paths[idx], O_RDONLY | O_NONBLOCK);
  if (fd_in < 0) {
    perror("child open fd_in");
    exit(1);
  }

  printf("%d opened input pipe at %s\n", getpid(), fifo_paths[idx]);

  if (idx == process_count - 1) {
    int fd_ready = open(ready_fifo, O_WRONLY);
    if (fd_ready >= 0) {
      char ready_byte = 1;
      write(fd_ready, &ready_byte, 1);
      close(fd_ready);
      printf("%d sent ready message to the parent\n", getpid());
    }
  }

  printf("%d is ready.\n", getpid());

  int fd_out = open(fifo_paths[(idx + 1) % process_count], O_WRONLY);
  if (fd_out < 0) {
    perror("open fd_out");
    close(fd_in);
    exit(1);
  }

  while (1) {
    message_t msg;
    int r = read(fd_in, &msg, sizeof(msg));
    if (r != sizeof(msg)) {
      continue;
    }

    printf("%d received message: %d\n", getpid(), msg.count);
    msg.count--;
    if (msg.count == 0) {
      printf("%d get count at 0. Exiting.\n", getpid());
      // TODO: process elimination logic
    }

    int w = write(fd_out, &msg, sizeof(msg));
    if (w != sizeof(msg)) {
      perror("write fd_out");
      close(fd_in);
      close(fd_out);
      exit(1);
    }

    printf("%d sent message to next target.\n", getpid());

    exit(0);
  }
}

int parent_process(int process_count, int step, char **fifo_paths,
                   const char *report_fifo, const char *ready_fifo, int *pids) {
  int fd_ready = open(ready_fifo, O_RDONLY);
  if (fd_ready < 0) {
    destroy_fifos(fifo_paths, process_count, report_fifo, ready_fifo);
    perror("open fifo ready");
    return 1;
  }

  int ready_flag = 0;
  read(fd_ready, &ready_flag, 1);

  int fd_first_message = open(fifo_paths[0], O_WRONLY | O_NONBLOCK);
  if (fd_first_message < 0) {
    perror("open fifo for first message");
    for (int i = 0; i < process_count; i++) {
      kill(pids[i], SIGTERM);
    }
    destroy_fifos(fifo_paths, process_count, report_fifo, ready_fifo);
    return 1;
  }

  printf("%d opened %s for first message\n", getpid(), fifo_paths[0]);

  message_t msg;
  msg.num_alive = process_count;
  msg.count = step;
  for (int i = 0; i < process_count; i++) {
    msg.alive[i] = i;
  }

  int w = write(fd_first_message, &msg, sizeof(msg));
  if (w != sizeof(msg)) {
    perror("parent write first message");
    destroy_fifos(fifo_paths, process_count, report_fifo, ready_fifo);
    return 1;
  }

  close(fd_first_message);
  printf("Parent sent first message\n");

  int status;
  pid_t pid;
  int terminate = 0;
  while ((pid = wait(&status)) > 0) {
    if (WIFEXITED(status)) {
      printf("Child %d exited with status %d\n", pid, WEXITSTATUS(status));
      if (WEXITSTATUS(status) != 0) {
        terminate = 1;
      }
    } else if (WIFSIGNALED(status)) {
      printf("Child %d killed by signal %d\n", pid, WTERMSIG(status));
      terminate = 1;
    }

    if (terminate) {
      terminate = 0;
      printf("One of the children ended with error. Aborting...\n");
      for (int i = 0; i < process_count; i++) {
        kill(pids[i], SIGTERM);
      }
      break;
    }
  }

  destroy_fifos(fifo_paths, process_count, report_fifo, ready_fifo);
  return terminate;
}
