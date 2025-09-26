#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#define MAX_STR_LEN 10

int main(int argc, char *argv[]) {
  if (argc != 2) {
    fprintf(stderr, "Client: Specify named pipe path\n");
    return 1;
  }

  srand(time(NULL));

  signal(SIGPIPE, SIG_IGN);

  int fd = open(argv[1], O_WRONLY);
  if (fd < 0) {
    perror("Client: open fifo file to write");
    return 1;
  }

  char buf[BUFSIZ];

  while (1) {
    size_t len = rand() % MAX_STR_LEN + 1;
    for (int i = 0; i < len - 1; i++) {
      buf[i] = 'a' + rand() % 26;
    }
    buf[len - 1] = '\0';

    printf("Client: sending '%s' (len = %zu)\n", buf, len - 1);

    ssize_t bytes_written = write(fd, buf, len);
    if (bytes_written < 0) {
      if (errno == EPIPE) {
        printf("Client: server closed FIFO. Exiting...\n");
        break;
      } else {
        perror("write");
        break;
      }
    }
    usleep(100000);
  }
  close(fd);
  return 0;
}
