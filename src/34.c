#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAX_N 1024

typedef struct {
  int count;
  int num_alive;
  int alive[MAX_N];
} message_t;

char *fifo_name(const char *base, int idx);

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
      perror("mkfifo ring");
    }
  }

  for (int i = 0; i < process_count; i++) {
    pid_t pid = fork();
    if (pid < 0) {
      // TODO help
      exit(1);
    }

    if (pid == 0) {
      sleep(1);
      int index = i;
      int fd_in = open(fifo_paths[index], O_RDONLY);
      if (fd_in < 0) {
        // TODO
        exit(1);
      }
    }
  }
}
char *fifo_name(const char *base, int idx) {
  char *s = malloc(256);
  if (s == NULL) {
    return NULL;
  }
  snprintf(s, 256, "%s_%d", base, idx);
  return s;
}
