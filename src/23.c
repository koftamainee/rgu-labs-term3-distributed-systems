// search_tree.c
#define _GNU_SOURCE
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define ERR_USAGE 1
#define ERR_OPEN_PATHS 2
#define ERR_NO_PATHS 3
#define ERR_PIPE_FAIL 4
#define ERR_FORK_FAIL 5
#define ERR_CHILD_ALLOC 6
#define ERR_FILE_OPEN 7
#define ERR_FILE_READ 8
#define ERR_NULL_PTR 9

#define MAX_LINE 4096
#define MAX_PATH 4096
#define MAX_PROCS_CAP 50

void print_error(int code) {
  switch (code) {
    case 0:
      printf("No error\n");
      break;
    case ERR_USAGE:
      printf("Usage: <file_with_paths> <search_string>\n");
      break;
    case ERR_OPEN_PATHS:
      printf("Failed to open file with paths\n");
      break;
    case ERR_NO_PATHS:
      printf("No file paths found in paths file\n");
      break;
    case ERR_PIPE_FAIL:
      printf("pipe() failed\n");
      break;
    case ERR_FORK_FAIL:
      printf("fork() failed\n");
      break;
    case ERR_CHILD_ALLOC:
      printf("Memory allocation failed in child process\n");
      break;
    case ERR_FILE_OPEN:
      printf("Failed to open one of the files\n");
      break;
    case ERR_FILE_READ:
      printf("Failed to read one of the files\n");
      break;
    case ERR_NULL_PTR:
      printf("Null pointer passed to function\n");
      break;
    default:
      printf("Unknown error code %d\n", code);
      break;
  }
}

static int count_substr_in_buffer(const char *haystack, const char *needle,
                                  long *out_count) {
  if (!haystack || !needle || !out_count) return ERR_NULL_PTR;
  if (!*needle) {
    *out_count = 0;
    return 0;
  }

  long cnt = 0;
  const char *p = haystack;
  while ((p = strstr(p, needle)) != NULL) {
    cnt++;
    p += 1;
  }
  *out_count = cnt;
  return 0;
}

static int child_count_file(const char *filepath, const char *needle,
                            int write_fd) {
  if (!filepath || !needle) return ERR_NULL_PTR;

  FILE *f = fopen(filepath, "r");
  if (!f) {
    int err = ERR_FILE_OPEN;
    write(write_fd, &err, sizeof(err));
    return ERR_FILE_OPEN;
  }

  fseek(f, 0, SEEK_END);
  long len = ftell(f);
  fseek(f, 0, SEEK_SET);

  char *buf = malloc(len + 1);
  if (!buf) {
    fclose(f);
    int err = ERR_CHILD_ALLOC;
    write(write_fd, &err, sizeof(err));
    return ERR_CHILD_ALLOC;
  }

  size_t r = fread(buf, 1, len, f);
  if (r < (size_t)len) {
    free(buf);
    fclose(f);
    int err = ERR_FILE_READ;
    write(write_fd, &err, sizeof(err));
    return ERR_FILE_READ;
  }
  buf[len] = '\0';
  fclose(f);

  long cnt;
  int res = count_substr_in_buffer(buf, needle, &cnt);
  free(buf);

  if (res != 0) {
    write(write_fd, &res, sizeof(res));
    return res;
  }

  int ok = 0;
  write(write_fd, &ok, sizeof(ok));
  write(write_fd, &cnt, sizeof(cnt));
  return 0;
}

static void spawn_balanced_tree_node(int idx, int nodes_count) {
  if (idx >= nodes_count) return;
  int child_indices[2] = {2 * idx + 1, 2 * idx + 2};
  for (int k = 0; k < 2; ++k) {
    int c = child_indices[k];
    if (c >= nodes_count) continue;
    pid_t pid = fork();
    if (pid == 0) {
      printf("[pid %d] node %d started\n", getpid(), c);
      fflush(stdout);
      spawn_balanced_tree_node(c, nodes_count);
      printf("[pid %d] node %d exiting\n", getpid(), c);
      fflush(stdout);
      _exit(0);
    } else if (pid > 0) {
      int status;
      waitpid(pid, &status, 0);
    }
  }
}

int main(int argc, char **argv) {
  if (argc != 3) {
    print_error(ERR_USAGE);
    return ERR_USAGE;
  }
  const char *paths_file = argv[1];
  const char *needle = argv[2];
  size_t needle_len = strlen(needle);

  FILE *pf = fopen(paths_file, "r");
  if (!pf) {
    print_error(ERR_OPEN_PATHS);
    return ERR_OPEN_PATHS;
  }

  char line[MAX_LINE];
  char **paths = NULL;
  size_t paths_cnt = 0;
  while (fgets(line, sizeof(line), pf)) {
    char *end = line + strlen(line) - 1;
    while (end >= line &&
           (*end == '\n' || *end == '\r' || *end == ' ' || *end == '\t')) {
      *end = '\0';
      end--;
    }
    if (strlen(line) == 0) continue;
    paths = realloc(paths, sizeof(char *) * (paths_cnt + 1));
    if (!paths) {
      fclose(pf);
      print_error(ERR_CHILD_ALLOC);
      return ERR_CHILD_ALLOC;
    }
    paths[paths_cnt] = strdup(line);
    if (!paths[paths_cnt]) {
      fclose(pf);
      print_error(ERR_CHILD_ALLOC);
      return ERR_CHILD_ALLOC;
    }
    paths_cnt++;
  }
  fclose(pf);

  if (paths_cnt == 0) {
    print_error(ERR_NO_PATHS);
    return ERR_NO_PATHS;
  }

  int (*pipes)[2] = malloc(sizeof(int[2]) * paths_cnt);
  pid_t *kids = malloc(sizeof(pid_t) * paths_cnt);
  if (!pipes || !kids) {
    print_error(ERR_CHILD_ALLOC);
    return ERR_CHILD_ALLOC;
  }

  for (size_t i = 0; i < paths_cnt; ++i) {
    if (pipe(pipes[i]) < 0) {
      print_error(ERR_PIPE_FAIL);
      return ERR_PIPE_FAIL;
    }
    pid_t pid = fork();
    if (pid < 0) {
      print_error(ERR_FORK_FAIL);
      return ERR_FORK_FAIL;
    } else if (pid == 0) {
      close(pipes[i][0]);
      child_count_file(paths[i], needle, pipes[i][1]);
      _exit(0);
    } else {
      close(pipes[i][1]);
      kids[i] = pid;
    }
  }

  long *counts = malloc(sizeof(long) * paths_cnt);
  int *errs = malloc(sizeof(int) * paths_cnt);
  if (!counts || !errs) {
    print_error(ERR_CHILD_ALLOC);
    return ERR_CHILD_ALLOC;
  }

  int total_hits = 0;
  for (size_t i = 0; i < paths_cnt; ++i) {
    int err;
    long cnt;
    ssize_t r1 = read(pipes[i][0], &err, sizeof(err));
    if (r1 != sizeof(err)) err = ERR_FILE_READ;
    if (err == 0) {
      ssize_t r2 = read(pipes[i][0], &cnt, sizeof(cnt));
      if (r2 != sizeof(cnt)) cnt = -1;
    } else {
      cnt = -1;
    }
    close(pipes[i][0]);
    int status;
    waitpid(kids[i], &status, 0);
    errs[i] = err;
    counts[i] = cnt;
    if (cnt > 0) total_hits += cnt;
  }

  if (total_hits > 0) {
    printf("Search results:\n");
    for (size_t i = 0; i < paths_cnt; ++i) {
      if (errs[i] == 0 && counts[i] > 0) {
        printf("%s : %ld\n", paths[i], counts[i]);
      } else if (errs[i] != 0) {
        print_error(errs[i]);
      }
    }
  } else {
    printf("String '%s' not found in any file.\n", needle);
    int desired = (int)needle_len;
    int nodes_to_spawn = desired > 0 ? desired : 1;
    if (nodes_to_spawn > MAX_PROCS_CAP) {
      printf("Limited to %d processes for safety.\n", MAX_PROCS_CAP);
      nodes_to_spawn = MAX_PROCS_CAP;
    }
    printf("[pid %d] root node 0\n", getpid());
    spawn_balanced_tree_node(0, nodes_to_spawn);
    printf("Process tree finished.\n");
  }

  for (size_t i = 0; i < paths_cnt; ++i) free(paths[i]);
  free(paths);
  free(pipes);
  free(kids);
  free(counts);
  free(errs);

  return 0;
}
