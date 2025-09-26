#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

int main() {
  int pipefd[2];
  char buf[BUFSIZ];
  int res = 0;

  if (pipe(pipefd) != 0) {
    perror("pipe");
    return EXIT_FAILURE;
  }

  pid_t pid = fork();
  if (pid < 0) {
    perror("fork");
    return EXIT_FAILURE;
  }

  if (pid == 0) {
    close(pipefd[1]);
    ssize_t bytes_read = read(pipefd[0], buf, sizeof(buf));
    if (bytes_read > 0) {
      printf("read from pipe: '%s'\n", buf);
      close(pipefd[0]);
      exit(EXIT_SUCCESS);
    } else {
      perror("read");
      close(pipefd[0]);
      exit(EXIT_FAILURE);
    }
  }

  if (pid > 0) {
    close(pipefd[0]);
    const char *msg = "some_string_from_parent";
    size_t bytes_to_write_count = strlen(msg) + 1;

    ssize_t bytes_written = write(pipefd[1], msg, bytes_to_write_count);
    if (bytes_written != (ssize_t)bytes_to_write_count) {
      perror("write");
      close(pipefd[1]);
      return EXIT_FAILURE;
    }

    close(pipefd[1]);
    wait(&res);
    return WEXITSTATUS(res);
  }
}
