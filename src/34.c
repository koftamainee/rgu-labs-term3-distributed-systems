#include <fcntl.h>
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

char *fifo_name(const char *base, int idx);

void cleanup_fifos(char **paths, int n, const char *report, const char *ready) {
  if (paths) {
    for (int i = 0; i < n; ++i) {
      if (paths[i]) {
        unlink(paths[i]);
        free(paths[i]);
      }
    }
    free(paths);
  }
  if (report) {
    unlink(report);
  }
  if (ready) {
    unlink(ready);
  }
}

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

  pid_t parent_pid = getpid();
  char base_fifo[128];
  snprintf(base_fifo, sizeof(base_fifo), "/tmp/joseph_%d_fifo", parent_pid);
  char report_fifo[128];
  snprintf(report_fifo, sizeof(report_fifo), "/tmp/joseph_%d_report",
           parent_pid);
  char ready_fifo[128];
  snprintf(ready_fifo, sizeof(ready_fifo), "/tmp/joseph_%d_ready", parent_pid);

  unlink(report_fifo);
  unlink(ready_fifo);

  if (mkfifo(report_fifo, 0666) != 0) {
    perror("mkfifo report");
    return 1;
  }
  if (mkfifo(ready_fifo, 0666) != 0) {
    unlink(report_fifo);
    perror("mkfifo ready");
    return 1;
  }

  char **fifo_paths = (char **)malloc(sizeof(char *) * process_count);
  if (fifo_paths == NULL) {
    unlink(report_fifo);
    unlink(ready_fifo);
    perror("malloc fifo_paths");
    return 1;
  }

  for (int i = 0; i < process_count; i++) {
    fifo_paths[i] = fifo_name(base_fifo, i);
    if (fifo_paths[i] == NULL) {
      for (int j = 0; j < i; j++) {
        free(fifo_paths[j]);
        unlink(fifo_paths[j]);
      }
      free(fifo_paths);
      unlink(report_fifo);
      unlink(ready_fifo);
      perror("malloc ring");
      return 1;
    }
    if (mkfifo(fifo_paths[i], 0666) != 0) {
      for (int j = 0; j < i; j++) {
        free(fifo_paths[j]);
        unlink(fifo_paths[j]);
      }
      free(fifo_paths);
      unlink(report_fifo);
      unlink(ready_fifo);
      perror("malloc ring");
      return 1;
    }
  }

  for (int i = 0; i < process_count; i++) {
    pid_t pid = fork();
    if (pid < 0) {
      cleanup_fifos(fifo_paths, process_count, report_fifo, ready_fifo);
      perror("fork");
      exit(1);
    }

    if (pid == 0) {
      int idx = i;
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
        printf("%d recivied message: %d\n", getpid(), msg.count);
        // msg.count--;
        if (msg.count == 0) {
          printf("%d get count at 0. Exiting.\n", getpid());
          close(fd_in);
          close(fd_out);
          exit(0);
        }

        int w = write(fd_out, &msg, sizeof(msg));
        if (w != sizeof(msg)) {
          perror("write fd_out");
          close(fd_in);
          close(fd_out);
          exit(1);
        }

        printf("%d sent message to the next target. Exiting\n", getpid());

        exit(0);
      }
    }
  }

  int fd_ready = open(ready_fifo, O_RDONLY);
  if (fd_ready < 0) {
    cleanup_fifos(fifo_paths, process_count, report_fifo, ready_fifo);
    perror("open fifo fd_ready write");
    return 1;
  }
  int ready_flag = 0;
  read(fd_ready, &ready_flag, 1);

  /// main parent code

  int fd_first_message = open(fifo_paths[0], O_WRONLY | O_NONBLOCK);
  if (fd_first_message < 0) {
    perror("open fifo for first message");

    cleanup_fifos(fifo_paths, process_count, report_fifo, ready_fifo);
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
    cleanup_fifos(fifo_paths, process_count, report_fifo, ready_fifo);
    return 1;
  }
  close(fd_first_message);

  printf("Parent sent first message\n");

  int status;
  pid_t pid;
  while ((pid = wait(&status)) > 0) {
    if (WIFEXITED(status)) {
      printf("Child %d exited with status %d\n", pid, WEXITSTATUS(status));
    } else if (WIFSIGNALED(status)) {
      printf("Child %d killed by signal %d\n", pid, WTERMSIG(status));
    }
  }

  cleanup_fifos(fifo_paths, process_count, report_fifo, ready_fifo);
  return 0;
}

char *fifo_name(const char *base, int idx) {
  char *s = malloc(256);
  if (s == NULL) {
    return NULL;
  }
  snprintf(s, 256, "%s_%d", base, idx);
  return s;
}
