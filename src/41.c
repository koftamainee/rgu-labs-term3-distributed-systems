#include <fcntl.h>
#include <mqueue.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <wait.h>

#define MQ_NAME "/41_mq"
#define MSG_SIZE 256

int main() {
  pid_t pid;
  mqd_t mq;
  struct mq_attr attr;

  attr.mq_flags = 0;
  attr.mq_maxmsg = 10;
  attr.mq_msgsize = MSG_SIZE;
  attr.mq_curmsgs = 0;

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
    char buffer[MSG_SIZE];
    ssize_t bytes_read;

    mq = mq_open(MQ_NAME, O_RDONLY);
    if (mq == (mqd_t)-1) {
      perror("child mq_open");
      exit(1);
    }

    bytes_read = mq_receive(mq, buffer, MSG_SIZE, NULL);
    if (bytes_read < 0) {
      perror("mq_receive");
      mq_close(mq);
      exit(1);
    }

    buffer[bytes_read] = '\0';
    printf("[Child %d] Received message: %s\n", getpid(), buffer);

    mq_close(mq);
    exit(0);
  } else {
    const char* msg = "Hello from parent!";
    if (mq_send(mq, msg, strlen(msg), 0) < 0) {
      perror("mq_send");
      mq_close(mq);
      mq_unlink(MQ_NAME);
      exit(1);
    }

    printf("[Parent %d] Sent message: %s\n", getpid(), msg);

    wait(NULL);
    mq_close(mq);
    mq_unlink(MQ_NAME);
  }

  return 0;
}
