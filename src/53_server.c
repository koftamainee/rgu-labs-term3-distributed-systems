#include <fcntl.h>
#include <mqueue.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../include/53.h"

#define SERVER_QUEUE "/farmer_main_queue"
#define MAX_CLIENTS 16

#define ESCAPE_SEQUENCE "\x1B"

typedef struct {
  pid_t client_pid;
  int left[4];   // 0=farmer,1=wolf,2=goat,3=cabbage
  int right[4];  // same
} session_t;

session_t sessions[MAX_CLIENTS];

int find_session(pid_t pid) {
  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (sessions[i].client_pid == pid) return i;
  }
  return -1;
}

int create_session(pid_t pid) {
  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (sessions[i].client_pid == -1) {
      sessions[i].client_pid = pid;
      sessions[i].left[0] = 1;  // farmer
      sessions[i].left[1] = 1;  // wolf
      sessions[i].left[2] = 1;  // goat
      sessions[i].left[3] = 1;  // cabbage
      sessions[i].right[0] = sessions[i].right[1] = sessions[i].right[2] = sessions[i].right[3] = 0;
      return i;
    }
  }
  return -1;
}

void format_state(session_t* s, char* out);

int main() {
  struct mq_attr attr = {0};
  attr.mq_flags = 0;
  attr.mq_maxmsg = 10;
  attr.mq_msgsize = sizeof(message_t);
  attr.mq_curmsgs = 0;

  for (int i = 0; i < MAX_CLIENTS; i++) {
    sessions[i].client_pid = -1;
  }

  mqd_t server_q = mq_open(SERVER_QUEUE, O_CREAT | O_RDONLY, 0666, &attr);
  if (server_q == (mqd_t)-1) {
    perror("mq_open");
    return 1;
  }

  printf("Server running...\n");

  message_t msg;
  msg.status = 0;
  while (1) {
    if (mq_receive(server_q, (char*)&msg, sizeof(msg), NULL) == -1) {
      perror("mq_receive");
      continue;
    }

    int idx = find_session(msg.client_pid);
    if (idx == -1) {
      printf("Creating new session with cliend PID: %d\n", msg.client_pid);
      idx = create_session(msg.client_pid);
    }
    if (idx == -1) {
      fprintf(stderr, "Max clients reached\n");
      continue;
    }

    if (strcmp(msg.command, ESCAPE_SEQUENCE) == 0) {
      sessions[idx].client_pid = -1;
      printf("Termination Session with client PID: %d\n", msg.client_pid);
      continue;
    }

    printf("Session PID %d command: %s\n", msg.client_pid, msg.command);

    session_t* s = &sessions[idx];

    int boat = s->left[0] ? 0 : 1;  // 0 = left, 1 = right
    int carry = -1;                 // -1 == empty

    for (int i = 1; i < 4; i++) {
      if (s->left[i] == 2 || s->right[i] == 2) {
        carry = i;
        break;
      }
    }

    if (strcmp(msg.command, "takegoat") == 0 || strcmp(msg.command, "takewolf") == 0 ||
        strcmp(msg.command, "takecabbage") == 0) {
      int item = (msg.command[4] == 'g') ? 2 : (msg.command[4] == 'w') ? 1 : 3;

      if (carry != -1) {
        msg.status = 1;
        strcpy(msg.command, "Boat not empty");
      } else if ((boat == 0 && s->left[item] == 1) || (boat == 1 && s->right[item] == 1)) {
        carry = item;
        if (boat == 0)
          s->left[item] = 2;
        else
          s->right[item] = 2;

        format_state(s, msg.command);
      } else {
        msg.status = 2;
        strcpy(msg.command, "Item not on this side");
      }

    }

    // ---------- put ----------
    else if (strcmp(msg.command, "put") == 0) {
      if (carry == -1) {
        msg.status = 3;
        strcpy(msg.command, "Boat empty");
      } else {
        if (boat == 0) {
          s->left[carry] = 1;
          s->right[carry] = 0;
        } else {
          s->right[carry] = 1;
          s->left[carry] = 0;
        }

        format_state(s, msg.command);
      }

    }

    // ---------- move ----------
    else if (strcmp(msg.command, "move") == 0) {
      if (boat == 0) {
        s->left[0] = 0;
        s->right[0] = 1;
      } else {
        s->right[0] = 0;
        s->left[0] = 1;
      }

      boat = s->left[0] ? 0 : 1;

      int lf = s->left[0];
      int rf = s->right[0];

      if (!lf && s->left[1] == 1 && s->left[2] == 1) {
        msg.status = 4;
        strcpy(msg.command, "Wolf eats goat left");
      } else if (!lf && s->left[2] == 1 && s->left[3] == 1) {
        msg.status = 5;
        strcpy(msg.command, "Goat eats cabbage left");
      } else if (!rf && s->right[1] == 1 && s->right[2] == 1) {
        msg.status = 6;
        strcpy(msg.command, "Wolf eats goat right");
      } else if (!rf && s->right[2] == 1 && s->right[3] == 1) {
        msg.status = 7;
        strcpy(msg.command, "Goat eats cabbage right");
      } else {
        format_state(s, msg.command);
      }

    }

    // ---------- unknown ----------
    else {
      msg.status = -1;
      strcpy(msg.command, "Invalid command");
    }

    char client_queue_name[64];
    snprintf(client_queue_name, sizeof(client_queue_name), "/client_%d", msg.client_pid);
    mqd_t client_q = mq_open(client_queue_name, O_WRONLY);
    if (client_q != (mqd_t)-1) {
      mq_send(client_q, (char*)&msg, sizeof(msg), 0);
      mq_close(client_q);
    }
  }

  mq_close(server_q);
  mq_unlink(SERVER_QUEUE);
  return 0;
}

void format_state(session_t* s, char* out) {
  char L[32] = {0}, R[32] = {0}, B[16] = {0};
  int boat = s->left[0] ? 0 : 1;

  for (int i = 0; i < 4; i++) {
    if (s->left[i] == 1) {
      char c = "FWGC"[i];
      size_t n = strlen(L);
      L[n] = c;
      L[n + 1] = ' ';
    }
    if (s->right[i] == 1) {
      char c = "FWGC"[i];
      size_t n = strlen(R);
      R[n] = c;
      R[n + 1] = ' ';
    }
    if (s->left[i] == 2 || s->right[i] == 2) {
      char c = "FWGC"[i];
      size_t n = strlen(B);
      B[n] = c;
      B[n + 1] = ' ';
    }
  }

  sprintf(out,
          "Left: %s, "
          "Right: %s, "
          "Boat: %s, "
          "BoatSide: %s",
          L, R, B, boat == 0 ? "left" : "right");
}
