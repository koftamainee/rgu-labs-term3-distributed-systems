#include <fcntl.h>
#include <mqueue.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <wait.h>

#define MQ_NAME "/42_mq"
#define MSG_SIZE 512
#define ESCAPE_SEQUENCE "\x1B"

typedef struct {
  int id;
  char name[64];
  float values[5];
  int values_count;
} custom_t;

typedef struct {
  char data[MSG_SIZE];
} message_t;

void serialize_custom(const custom_t* c, char* buffer) {
  int pos =
      snprintf(buffer, MSG_SIZE, "%d;%s;%d", c->id, c->name, c->values_count);
  for (int i = 0; i < c->values_count; i++) {
    pos += snprintf(buffer + pos, MSG_SIZE - pos, ";%f", c->values[i]);
  }
}

void deserialize_custom(const char* buffer, custom_t* c) {
  char temp[MSG_SIZE];
  strncpy(temp, buffer, MSG_SIZE);
  temp[MSG_SIZE - 1] = '\0';

  char* token = strtok(temp, ";");
  c->id = atoi(token);

  token = strtok(NULL, ";");
  strncpy(c->name, token, sizeof(c->name));
  c->name[sizeof(c->name) - 1] = '\0';

  token = strtok(NULL, ";");
  c->values_count = atoi(token);

  for (int i = 0; i < c->values_count; i++) {
    token = strtok(NULL, ";");
    if (token) {
      c->values[i] = atof(token);
    } else {
      c->values[i] = 0.0f;
    }
  }
}

void handle_string(const char* msg) { printf("[Reader] String: %s\n", msg); }

void handle_int(const char* msg) { printf("[Reader] Int: %d\n", atoi(msg)); }

void handle_custom(const char* msg) {
  custom_t c;
  deserialize_custom(msg, &c);
  printf("[Reader] Custom id=%d, name=%s, values=[", c.id, c.name);
  for (int i = 0; i < c.values_count; i++) {
    printf("%f", c.values[i]);
    if (i < c.values_count - 1) {
      printf(", ");
    }
  }
  printf("]\n");
}

int main() {
  pid_t pid;
  mqd_t mq;
  struct mq_attr attr = {0};

  attr.mq_flags = 0;
  attr.mq_maxmsg = 10;
  attr.mq_msgsize = sizeof(message_t);

  mq = mq_open(MQ_NAME, O_CREAT | O_RDWR, 0666, &attr);
  if (mq == (mqd_t)-1) {
    perror("mq_open");
    exit(1);
  }

  pid = fork();
  if (pid < 0) {
    perror("fork");
    mq_close(mq);
    mq_unlink(MQ_NAME);
    exit(1);
  }

  if (pid == 0) {
    mq = mq_open(MQ_NAME, O_RDONLY);
    if (mq == (mqd_t)-1) {
      perror("reader mq_open");
      exit(1);
    }

    while (1) {
      message_t msg;
      unsigned int prio;
      ssize_t bytes = mq_receive(mq, (char*)&msg, sizeof(msg), &prio);
      if (bytes < 0) {
        perror("mq_receive");
        break;
      }

      if (strcmp(msg.data, ESCAPE_SEQUENCE) == 0) {
        printf("[Reader] Got escape sequence. Terminating...\n");
        break;
      }

      switch (prio) {
        case 1: {
          handle_string(msg.data);
          break;
        }
        case 2: {
          handle_int(msg.data);
          break;
        }
        case 3: {
          handle_custom(msg.data);
          break;
        }
        default: {
          printf("[Reader] Unknown priority %u\n", prio);
          break;
        }
      }
    }

    mq_close(mq);
    exit(0);
  } else {
    // Writer
    mq = mq_open(MQ_NAME, O_WRONLY);
    if (mq == (mqd_t)-1) {
      perror("writer mq_open");
      kill(pid, SIGTERM);
      mq_unlink(MQ_NAME);
      exit(1);
    }

    message_t msgs[5];

    snprintf(msgs[0].data, MSG_SIZE, "Hello string");
    snprintf(msgs[1].data, MSG_SIZE, "%d", 42);
    custom_t c = {123, "Alice", {1.1f, 2.2f, 3.3f}, 3};
    serialize_custom(&c, msgs[2].data);
    snprintf(msgs[3].data, MSG_SIZE, "Another string");
    snprintf(msgs[4].data, MSG_SIZE, ESCAPE_SEQUENCE);

    unsigned int priorities[5] = {1, 2, 3, 1, 1};

    for (int i = 0; i < 5; i++) {
      if (mq_send(mq, (char*)&msgs[i], sizeof(msgs[i]), priorities[i]) < 0) {
        perror("mq_send");
      } else {
        if (i < 4) {
          printf("[Writer] Sent with prio %u: %s\n", priorities[i],
                 msgs[i].data);
        } else {
          printf("[Writer] Sent escape sequence. Terminating...\n");
        }
      }
      usleep(100);
    }

    wait(NULL);
    mq_close(mq);
    mq_unlink(MQ_NAME);
  }

  return 0;
}
