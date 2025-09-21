#define _XOPEN_SOURCE 500
#include <ftw.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../include/hash_table.h"

typedef struct {
  dev_t dev;
  ino_t ino;
} file_key;

static size_t g_recmin = 0;
static size_t g_recmax = 0;
static hash_table *g_seen = NULL;

static int keys_comparer(const void *a, const void *b);
static size_t hash_file_key(const void *key, size_t key_size, size_t capacity);

static const char *get_extension(const char *filename);

static int walker(const char *fpath, const struct stat *sb, int typeflag,
                  struct FTW *ftwbuf);

int main(int argc, char *argv[]) {
  if (argc < 4) {
    fprintf(stderr, "Incorrect usage\n");
    return INVALID_CLI_ARGUMENT;
  }

  size_t target_dirs_count = 0;
  err_t err = 0;
  int i = 0;
  char *endptr;

  g_recmin = strtoull(argv[1], &endptr, 10);

  if (*endptr != '\0') {
    fprintf(stderr, "Invalid recmin: %s\n", argv[1]);
    return INVALID_CLI_ARGUMENT;
  }

  g_recmax = strtoull(argv[2], &endptr, 10);
  if (*endptr != '\0') {
    fprintf(stderr, "Invalid recmax: %s\n", argv[2]);
    return INVALID_CLI_ARGUMENT;
  }

  if (g_recmin > g_recmax) {
    fprintf(stderr, "recmin cannot be greater than recmax\n");
    return INVALID_CLI_ARGUMENT;
  }

  err = hash_table_init(&g_seen, keys_comparer, hash_file_key, sizeof(file_key),
                        sizeof(int), free);
  if (err != 0) {
    fprintf(stderr, "Failed to init hash table: %d\n", err);
    return EXIT_FAILURE;
  }

  printf("╭────────┬─────────────────────┬───────┬─────────────╮\n");
  printf("│ #      │ Name                │ Ext   │ Addr(blocks)│\n");
  printf("├────────┼─────────────────────┼───────┼─────────────┤\n");

  for (i = 3; i < argc; i++) {
    if (nftw(argv[i], walker, 20, FTW_PHYS) == -1) {
      hash_table_free(g_seen);

      printf("╰────────┴─────────────────────┴───────┴─────────────╯\n");

      fprintf(stderr, "nftw failed\n");
      return EXIT_FAILURE;
    }
  }

  printf("╰────────┴─────────────────────┴───────┴─────────────╯\n");

  hash_table_free(g_seen);
}

static int keys_comparer(const void *a, const void *b) {
  const file_key *ka = a;
  const file_key *kb = b;
  return ka->dev == kb->dev && ka->ino == kb->ino;
}

static size_t hash_file_key(const void *key, size_t key_size, size_t capacity) {
  const file_key *k = key;
  uint64_t v = ((uint64_t)k->dev << sizeof(int) * 8) ^ (uint64_t)k->ino;
  return (size_t)v % capacity;
}

static const char *get_extension(const char *filename) {
  if (filename == NULL) {
    return NULL;
  }

  const char *last_dot = strrchr(filename, '.');
  if (last_dot == NULL || last_dot == filename) {
    return "";  // no extension
  }

  return last_dot + 1;
}

static int walker(const char *fpath, const struct stat *sb, int typeflag,
                  struct FTW *ftwbuf) {
  if (typeflag != FTW_F) {
    return 0;
  }

  if ((size_t)ftwbuf->level < g_recmin || (size_t)ftwbuf->level > g_recmax) {
    return 0;
  }

  file_key key = {.dev = sb->st_dev, .ino = sb->st_ino};
  void *dummy = NULL;
  int to_insert = 1;
  static int file_counter = 0;

  if (hash_table_get(g_seen, &key, &dummy) == 0) {
    return 0;  // already seen
  }

  if (hash_table_set(g_seen, &key, &to_insert) != 0) {
    fprintf(stderr, "WARNING: hash_table_set failed\n");
    return 0;  // dont abort nftw loop, just print out
  }

  const char *filename = fpath + ftwbuf->base;
  const char *ext = get_extension(filename);

  if (*ext == '\0') {
    ext = ".";
  }

  printf("│ %-6d │ %-19.19s │ %-5.5s │ %11" PRIu64 " │\n", file_counter++,
         filename, ext, (uint64_t)sb->st_blocks);

  return 0;
}
