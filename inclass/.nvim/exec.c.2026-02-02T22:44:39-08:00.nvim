#include <stdio.h>
#include <unistd.h>

int main(void) {
  printf("Start PID=%d, parent PID=%d\n", getpid(), getppid());
  int fpid = fork();
  if (fpid != 0) {
    execlp("ls", "ls", "-a", (char *)NULL);
  } else {
    execlp("ls", "ls, "-a", "-|", "-h", (char *)NULL);
  }
  return 0;
}
