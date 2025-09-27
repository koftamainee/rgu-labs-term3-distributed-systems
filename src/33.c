#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define TERMINATION_MARK -1
#define TARGET_COUNT 50

int gcd(int a, int b);
int child_process(int read_fd, int write_fd);
int parent_process(int read_fd, int write_fd);

int main() {
  int parent_to_child[2];
  int child_to_parent[2];

  if (pipe(parent_to_child) != 0 || pipe(child_to_parent) != 0) {
    perror("Error creating pipes");
    return 1;
  }

  pid_t pid = fork();
  if (pid < 0) {
    perror("Error on fork");
    return 1;
  }

  if (pid == 0) {
    close(parent_to_child[1]);
    close(child_to_parent[0]);
    int res = child_process(parent_to_child[0], child_to_parent[1]);
    close(parent_to_child[0]);
    close(child_to_parent[1]);
    exit(res);
  } else {
    close(parent_to_child[0]);
    close(child_to_parent[1]);
    int res = parent_process(child_to_parent[0], parent_to_child[1]);
    if (res != 0) {
      kill(pid, SIGTERM);
    }
    int child_res = 0;
    waitpid(pid, &child_res, 0);
    close(parent_to_child[1]);
    close(child_to_parent[0]);
    return WEXITSTATUS(child_res);
  }
}

int gcd(int a, int b) {
  while (b != 0) {
    int t = b;
    b = a % b;
    a = t;
  }
  return a;
}

int child_process(int read_fd, int write_fd) {
  int occurrences = 0;

  while (1) {
    int num;
    if (read(read_fd, &num, sizeof(num)) != sizeof(num)) {
      perror("Child error reading number");
      return 1;
    }

    if (occurrences == TARGET_COUNT) {
      int term = TERMINATION_MARK;
      if (write(write_fd, &term, sizeof(term)) != sizeof(term)) {
        perror("Child error sending termination mark");
        return 1;
      }
      printf("[Child] Sent termination mark. Exiting.\n");
      break;
    }

    int coprimes[num];
    int coprime_count = 0;
    for (int i = 1; i < num; i++) {
      if (gcd(i, num) == 1) {
        coprimes[coprime_count++] = i;
      }
    }

    if (coprime_count == num - 1) {
      occurrences++;
      printf("[Child] Found prime number. Current occurrences: %d\n",
             occurrences);
    }

    if (write(write_fd, &coprime_count, sizeof(coprime_count)) !=
        sizeof(coprime_count)) {
      perror("Child error writing length");
      return 1;
    }
    for (int i = 0; i < coprime_count; i++) {
      if (write(write_fd, &coprimes[i], sizeof(coprimes[i])) !=
          sizeof(coprimes[i])) {
        perror("Child error writing number");
        return 1;
      }
    }
    printf("[Child] Sent coprime numbers to parent\n");
  }

  return 0;
}

int parent_process(int read_fd, int write_fd) {
  srand(time(NULL));

  while (1) {
    int random_number = rand() % 100 + 1;
    if (write(write_fd, &random_number, sizeof(random_number)) !=
        sizeof(random_number)) {
      perror("Parent error writing number");
      return 1;
    }
    printf("[Parent] Sent number %d to child\n", random_number);

    int len;
    if (read(read_fd, &len, sizeof(len)) != sizeof(len)) {
      perror("Parent error reading length");
      return 1;
    }

    if (len == TERMINATION_MARK) {
      printf("[Parent] Received termination mark. Exiting.\n");
      break;
    }

    printf("[Parent] Numbers from child: {");
    for (int i = 0; i < len; i++) {
      int num;
      if (read(read_fd, &num, sizeof(num)) != sizeof(num)) {
        perror("Parent error reading number");
        return 1;
      }
      printf("%d", num);
      if (i < len - 1) printf(", ");
    }
    printf("}\n\n");
  }

  return 0;
}
