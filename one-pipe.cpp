#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
  int cpipefd[2];
  char buf;
  pid_t cpid;

  if (argc != 2) {
    fprintf(stderr, "Usage: %s <string>\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  if (pipe(cpipefd) == -1) {
    perror("cpipe");
    exit(EXIT_FAILURE);
  }

  int creadfd = cpipefd[0];
  int pwritefd = cpipefd[1];

  cpid = fork();
  if (cpid == -1) {
    perror("fork");
    exit(EXIT_FAILURE);
  }

  if (cpid == 0) { /* Child reads from pipe */
    close(pwritefd); /* Reader will see EOF */

    int status;
    /* fprintf(stderr, "creadfd: %d\n", creadfd); */
    /* fprintf(stderr, "cwritefd: %d\n", cwritefd); */

    status = dup2(creadfd, STDIN_FILENO);
    if (status == -1) {
      close(creadfd);  /* Close unused read end */
      puts("din\n");
      _exit(status);
    }

    close(creadfd);

    /* puts("running myecho\n"); */
    execlp("./myecho", "./myecho", NULL);

    puts("!\n");
    _exit(EXIT_FAILURE);
  } else { /* Parent writes argv[1] to pipe */
    close(creadfd); /* Close unused read end */

    write(pwritefd, argv[1], strlen(argv[1]));

    close(pwritefd); /* Reader will see EOF */

    wait(NULL); /* Wait for child */
    exit(EXIT_SUCCESS);
  }
}
