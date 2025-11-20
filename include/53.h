#pragma once
#include <fcntl.h>
#define MAX_CMD_LEN 256
#define MAX_CLIENTS 16

typedef struct {
  pid_t client_pid;
  int status;
  char command[MAX_CMD_LEN];
} message_t;
