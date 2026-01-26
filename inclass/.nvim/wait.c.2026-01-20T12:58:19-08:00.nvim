#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main() {
  pid_t pid = fork();
  if (pid!= 0) {
    int wstatus = 0;
      if (waitpid(pid, &wstats, 0) == -1) {
        exit(EXIT_FAILURE);
      } else {
        printf("Child exited\n");
      }
  } else {
    if (execlp("ls", "ls", "-a", "-l", NULL) == -1) {
      perror("execelp");
      exit(EXIT_FAILURE);
    }
  }
  return 0;
}             
