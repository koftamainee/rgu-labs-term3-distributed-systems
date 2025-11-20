#include <ctype.h>
#include <fcntl.h>
#include <mqueue.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../include/53.h"

#define SERVER_QUEUE "/farmer_main_queue"

#define ESCAPE_SEQUENCE "\x1B"

int main(int argc, char* argv[]) {
  if (argc != 2) {
    fprintf(stderr, "Usage: %s <command_file>\n", argv[0]);
    return 1;
  }

  FILE* file = fopen(argv[1], "r");
  if (!file) {
    perror("fopen");
    return 1;
  }

  char client_queue_name[64];
  snprintf(client_queue_name, sizeof(client_queue_name), "/client_%d", getpid());

  struct mq_attr attr = {0};
  attr.mq_flags = 0;
  attr.mq_maxmsg = 10;
  attr.mq_msgsize = sizeof(message_t);
  attr.mq_curmsgs = 0;

  mqd_t reply_q = mq_open(client_queue_name, O_CREAT | O_RDONLY, 0666, &attr);
  if (reply_q == (mqd_t)-1) {
    perror("mq_open reply");
    return 1;
  }

  mqd_t server_q = mq_open(SERVER_QUEUE, O_WRONLY);
  if (server_q == (mqd_t)-1) {
    perror("mq_open server");
    return 1;
  }

  char buffer[MAX_CMD_LEN];
  int buf_index = 0;
  int c;

  message_t msg;
  msg.client_pid = getpid();
  msg.status = 0;

  while ((c = fgetc(file)) != EOF) {
    if (isspace(c)) {
      continue;
    }
    if (c == ';') {
      buffer[buf_index] = '\0';
      if (strlen(buffer) > 0) {
        strncpy(msg.command, buffer, MAX_CMD_LEN - 1);
        msg.command[MAX_CMD_LEN - 1] = '\0';
        mq_send(server_q, (char*)&msg, sizeof(msg), 0);

        message_t reply;
        if (mq_receive(reply_q, (char*)&reply, sizeof(reply), NULL) != -1) {
          printf("Command: %s\n", msg.command);
          printf("Server reply: \"%s\"\n", reply.command);
          if (reply.status != 0) {
            printf("Exit status: %d\n", reply.status);
            buf_index = 0;
            break;
          }
        }
      }
      buf_index = 0;
    } else {
      if (buf_index < MAX_CMD_LEN - 1) {
        buffer[buf_index++] = c;
      }
    }
  }

  strncpy(msg.command, ESCAPE_SEQUENCE, MAX_CMD_LEN - 1);

  mq_send(server_q, (char*)&msg, sizeof(msg), 0);

  fclose(file);

  if (buf_index > 0) {
    fprintf(stderr, "Error: last command missing terminating ';'\n");
    mq_close(server_q);
    mq_close(reply_q);
    mq_unlink(client_queue_name);
    return 1;
  }

  mq_close(server_q);
  mq_close(reply_q);
  mq_unlink(client_queue_name);

  return 0;
}
