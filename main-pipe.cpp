#include <assert.h>
#include <fcntl.h>
#include <iostream>
#include <poll.h>
#include <string>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

#define BUFFER_SIZE 4096

#define OUT_CMD "-"
/* #define OUT_CMD "tcp://localhost:8080/listen" */
#define MUSIC_FILE "track.opus"

static bool running = true;

float volume = 100;
int ppipefd[2];
int cpipefd[2];
pid_t cpid;

void listener() {
  float volstr;

  while (running && std::cin >> volstr) {
    if (volstr > (float)0)
      volume = volstr;
  }
}

std::string getCmd() {
  return (std::string("ffmpeg -f s16le -i - ") + "-af \"volume=" +
          std::to_string(volume / (float)100) + "\"" + " -f s16le " OUT_CMD);
}

int run_child(int pwritefd, int creadfd, int preadfd, int cwritefd) {
  if (prctl(PR_SET_PDEATHSIG, SIGINT) == -1) {
    perror("child");
    exit(1);
  }

  close(pwritefd); /* Reader will see EOF */
  close(preadfd);

  int status;
  /* fprintf(stderr, "creadfd: %d\n", creadfd); */
  /* fprintf(stderr, "cwritefd: %d\n", cwritefd); */

  status = dup2(creadfd, STDIN_FILENO);
  close(creadfd);
  if (status == -1) {
    perror("din");
    _exit(status);
  }

  status = dup2(cwritefd, STDOUT_FILENO);
  close(cwritefd);
  if (status == -1) {
    perror("dout");
    _exit(status);
  }

  if (status == -1) {
    _exit(status);
  }

  int dnull = open("/dev/null", O_WRONLY);
  dup2(dnull, STDERR_FILENO);
  close(dnull);

  execlp("ffmpeg", "ffmpeg", "-f", "s16le", "-i", "-", "-af",
         (std::string("volume=") + std::to_string(volume / (float)100)).c_str(),
         "-f", "s16le", "-bufsize", "64K", /*"-preset", "ultrafast",*/ OUT_CMD,
         (char *)NULL);

  perror("!");
  _exit(EXIT_FAILURE);
}

int main() {
  std::thread t(listener);
  t.detach();

  FILE *input = popen(
      (std::string("opusdec ") + MUSIC_FILE " - 2>/dev/null").c_str(), "r");

  if (!input) {
    fprintf(stderr, "popen\n");
    return EXIT_FAILURE;
  }

  if (pipe(ppipefd) == -1) {
    perror("ppipe");
    return EXIT_FAILURE;
  }
  int preadfd = ppipefd[0];
  int cwritefd = ppipefd[1];

  if (pipe(cpipefd) == -1) {
    perror("cpipe");
    return EXIT_FAILURE;
  }
  int creadfd = cpipefd[0];
  int pwritefd = cpipefd[1];

  cpid = fork();
  if (cpid == -1) {
    perror("fork");
    return EXIT_FAILURE;
  }

  if (cpid == 0) { /* Child reads from pipe */
    run_child(pwritefd, creadfd, preadfd, cwritefd);
  } else {           /* Parent writes argv[1] to pipe */
    close(creadfd);  /* Close unused read end */
    close(cwritefd); /* Close unused write end */

    /* fcntl(pwritefd, F_SETFL, fcntl(pwritefd, F_GETFL, 0) | O_NONBLOCK); */
    /* fcntl(preadfd, F_SETFL, fcntl(preadfd, F_GETFL, 0) | O_NONBLOCK); */

    nfds_t nfds = 1;
    struct pollfd pfds[1];
    pfds[0].events = POLLIN;

    pfds[0].fd = preadfd;

    // associate fds with FILEs
    FILE *pwritefile = fdopen(pwritefd, "w");
    FILE *preadfile = fdopen(preadfd, "r");

    size_t total_written_size = 0;
    float current_volume = volume;
    size_t read_size = 0;
    char buffer[BUFFER_SIZE];
    while ((read_size = fread(buffer, 1, BUFFER_SIZE, input))) {
      assert(fwrite(buffer, 1, read_size, pwritefile) == read_size);

      /* fflush(pwritefile); */

      /* size_t written_size = 0; */
      /* while ((written_size += */
      /*         fwrite(buffer + written_size, 1, read_size - written_size, */
      /*                pwritefile) < read_size)) */
      /*   ; */
      /* total_written_size += written_size; */

      int has_event = poll(pfds, 1, 0);

      if ((has_event > 0) && (pfds[0].revents & POLLIN)) {
        while ((read_size = fread(buffer, 1, BUFFER_SIZE, preadfile)) > 0) {
          fwrite(buffer, 1, read_size, stdout);

          has_event = poll(pfds, 1, 0);
          if (!((has_event > 0) && (pfds[0].revents & POLLIN)))
            break;
        }
      }

      if (volume != current_volume) {
        /* pclose(out); */
        /* out = nullptr; */
        fclose(pwritefile); /* Reader will see EOF */
        pwritefile = NULL;
        fclose(preadfile); /* Reader will see EOF */
        preadfile = NULL;

        int status;
        waitpid(cpid, &status, 0); /* Wait for child */

        fprintf(stderr, "child status: %d\n", status);

        // kill child
        kill(cpid, SIGINT);

        if (pipe(ppipefd) == -1) {
          perror("ppipe");
          break;
        }
        preadfd = ppipefd[0];
        cwritefd = ppipefd[1];

        if (pipe(cpipefd) == -1) {
          perror("cpipe");
          break;
        }
        creadfd = cpipefd[0];
        pwritefd = cpipefd[1];

        cpid = fork();
        if (cpid == -1) {
          close(creadfd);
          close(pwritefd);
          close(cwritefd);
          close(preadfd);
          perror("fork");
          break;
        }

        if (cpid == 0) { /* Child reads from pipe */
          return run_child(pwritefd, creadfd, preadfd, cwritefd);
        }

        close(creadfd);  /* Close unused read end */
        close(cwritefd); /* Close unused read end */
        pwritefile = fdopen(pwritefd, "w");
        preadfile = fdopen(preadfd, "r");

        /* assert(fwrite(buffer, 1, read_size, pwritefile) == read_size); */

        current_volume = volume;
      }
    }

    if (pwritefile)
      fclose(pwritefile); /* Reader will see EOF */

    if (preadfile)
      fclose(preadfile);
    /* close(pwritefd); */
  }

  /* out = popen(getCmd().c_str(), "w"); */

  /* if (!out) { */
  /*   fprintf(stderr, "popen\n"); */
  /*   pclose(input); */
  /*   return 1; */
  /* } */

  pclose(input);
  /* pclose(out); */
  fprintf(stderr, "closed\n");
  fprintf(stderr, "waiting for child\n");

  // kill child
  kill(cpid, SIGINT);
  int status;
  waitpid(cpid, &status, 0); /* Wait for child */
  fprintf(stderr, "child status: %d\n", status);

  running = false;

  return 0;
}
