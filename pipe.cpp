#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
  int ppipefd[2];
  int cpipefd[2];
  char buf;
  pid_t cpid;

  if (argc != 2) {
    fprintf(stderr, "Usage: %s <string>\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  if (pipe(ppipefd) == -1) {
    perror("ppipe");
    exit(EXIT_FAILURE);
  }

  int preadfd = ppipefd[0];
  int cwritefd = ppipefd[1];

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
    close(preadfd);
    close(pwritefd); /* Reader will see EOF */

    int status;
    /* fprintf(stderr, "creadfd: %d\n", creadfd); */
    /* fprintf(stderr, "cwritefd: %d\n", cwritefd); */

    status = dup2(creadfd, STDIN_FILENO);
    close(creadfd);
    status = dup2(cwritefd, STDOUT_FILENO);
    close(cwritefd); /* Reader will see EOF */

    if (status == -1) {
      perror("child");
      _exit(status);
    }

    /* puts("running myecho\n"); */
    execlp("./myecho", "./myecho", NULL);

    perror("!");
    _exit(EXIT_FAILURE);
  } else {           /* Parent writes argv[1] to pipe */
    close(cwritefd); /* Reader will see EOF */
    close(creadfd);  /* Close unused read end */

    write(pwritefd, argv[1], strlen(argv[1]));
    write(pwritefd, "\n", 1);
    close(pwritefd); /* Reader will see EOF */

    while (read(preadfd, &buf, 1) > 0)
      putc(buf, stdout);
    close(preadfd); /* Close unused read end */

    printf("waiting for child\n");
    wait(NULL); /* Wait for child */
    exit(EXIT_SUCCESS);
  }
}
