#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
  int pipefd[2];
  char buf;
  pid_t cpid;

  if (argc != 2) {
    fprintf(stderr, "Usage: %s <string>\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  if (pipe(pipefd) == -1) {
    perror("pipe");
    exit(EXIT_FAILURE);
  }

  cpid = fork();
  if (cpid == -1) {
    perror("fork");
    exit(EXIT_FAILURE);
  }

  if (cpid == 0) { /* Child reads from pipe */
    int status;

    status = dup2(pipefd[0], STDIN_FILENO);
    if (status == -1) {
      close(pipefd[1]); /* Reader will see EOF */
      close(pipefd[0]); /* Close unused read end */
      puts("din\n");
      _exit(status);
    }
    close(pipefd[0]);

    status = dup2(pipefd[1], STDOUT_FILENO);
    if (status == -1) {
      close(pipefd[1]); /* Reader will see EOF */
      close(pipefd[0]); /* Close unused read end */
      puts("dout\n");
      _exit(status);
    }
    close(pipefd[0]); /* Close unused read end */

    execlp("./myecho", "./myecho");

    puts("!\n");
    _exit(EXIT_FAILURE);
  } else { /* Parent writes argv[1] to pipe */
    write(pipefd[1], argv[1], strlen(argv[1]));
    close(pipefd[1]); /* Reader will see EOF */

    while (read(pipefd[0], &buf, 1) > 0)
      putc(buf, stdout);

    close(pipefd[0]); /* Close unused read end */

    wait(NULL); /* Wait for child */
    exit(EXIT_SUCCESS);
  }
}
