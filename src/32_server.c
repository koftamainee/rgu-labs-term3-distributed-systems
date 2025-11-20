#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
  if (argc != 2) {
    fprintf(stderr, "Server: Specify pipe_path\n");
    return 1;
  }
  // unlink(argv[1]);

  struct stat s;
  if (stat(argv[1], &s) == 0) {
    perror("file exists");
    // s.st_mode check if fifo
    return 1;
  }

  if (mkfifo(argv[1], 0600) < 0) {
    perror("mkfifo");
    return 1;
  }
  printf("Server: FIFO created on %s. Waiting writters...\n", argv[1]);

  int fd = open(argv[1], O_RDONLY);
  if (fd < 0) {
    perror("Server: open fifo file to read");
    unlink(argv[1]);
    return 1;
  }

  int counts[BUFSIZ] = {0};
  char buf[BUFSIZ];
  ssize_t bytes_read = 0;

  while (1) {
    bytes_read = read(fd, buf, BUFSIZ);
    buf[bytes_read] = '\0';

    size_t len = strlen(buf);
    if (len < BUFSIZ) {
      counts[len]++;
      printf(
          "Server: Recivied '%s' (len = %zu), total len occurences: "
          "%d.\n",
          buf, len, counts[len]);
      if (counts[len] >= 5) {
        printf("Server: Length %zu encountered 5 times. Quiting...\n", len);
        break;
      }
    }
  }
  close(fd);
  unlink(argv[1]);
  return 0;
}
