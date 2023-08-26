#include <assert.h>
#include <fcntl.h>
#include <iostream>
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

int run_child(int pwritefd, int creadfd) {
  if (prctl(PR_SET_PDEATHSIG, SIGINT) == -1) {
    perror("child");
    exit(1);
  }

  close(pwritefd); /* Reader will see EOF */

  int status;
  /* fprintf(stderr, "creadfd: %d\n", creadfd); */
  /* fprintf(stderr, "cwritefd: %d\n", cwritefd); */

  status = dup2(creadfd, STDIN_FILENO);
  close(creadfd);

  if (status == -1) {
    perror("child");
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
    run_child(pwritefd, creadfd);
  } else {          /* Parent writes argv[1] to pipe */
    close(creadfd); /* Close unused read end */

    /* fcntl(pwritefd, F_SETFL, fcntl(pwritefd, F_GETFL, 0) | O_NONBLOCK); */
    /* fcntl(preadfd, F_SETFL, fcntl(preadfd, F_GETFL, 0) | O_NONBLOCK); */

    FILE *pwritefile = fdopen(pwritefd, "w");

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

      /* while ((read_size = fread(buffer, 1, BUFFER_SIZE, preadfile)) > 0) */
      /*   fwrite(buffer, 1, read_size, stdout); */

      if (volume != current_volume) {
        /* pclose(out); */
        /* out = nullptr; */
        fclose(pwritefile); /* Reader will see EOF */
        pwritefile = NULL;

        int status;
        waitpid(cpid, &status, 0); /* Wait for child */

        fprintf(stderr, "child status: %d\n", status);

        kill(cpid, SIGINT);

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
          perror("fork");
          break;
        }

        if (cpid == 0) { /* Child reads from pipe */
          return run_child(pwritefd, creadfd);
        }

        close(creadfd); /* Close unused read end */
        pwritefile = fdopen(pwritefd, "w");

        assert(fwrite(buffer, 1, read_size, pwritefile) == read_size);

        current_volume = volume;
      }
    }

    if (pwritefile)
      fclose(pwritefile); /* Reader will see EOF */
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
  wait(NULL); /* Wait for child */

  running = false;

  return 0;
}
