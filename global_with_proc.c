#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

/* shared global variable */
int g_count = 0;

int main(void) {
  pid_t pid = fork();

  if (pid > 0) {
    /* parent only code
     ******************/
    g_count++;
    wait(NULL);
    printf("[%d] g_count: %d\n", getpid(), g_count);
  } else if (pid == 0) {
    /* child only code
     *****************/
    g_count++;
  } else {
    /* error handling (in parent) */
    perror("fork");
    exit(1);
  }

  return 0;
}
