#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>

int main() {
  pid_t pid = getpid();
  pid_t ppid = getppid();
  pid_t pgid = getpgrp();
  uid_t ruid = getuid();
  uid_t euid = geteuid();
  gid_t rgid = getgid();
  gid_t egid = getegid();

  printf("Current process ID (PID): %d\n", pid);
  printf("Parent process ID (PPID): %d\n", ppid);
  printf("Process group ID (PGID): %d\n", pgid);
  printf("Real user ID (RUID): %d\n", ruid);
  printf("Effective user ID (EUID): %d\n", euid);
  printf("Real group ID (RGID): %d\n", rgid);
  printf("Effective group ID (EGID): %d\n", egid);

  return 0;
}
