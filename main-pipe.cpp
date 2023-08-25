#include <iostream>
#include <string>
#include <thread>
#include <unistd.h>

#define BUFFER_SIZE 4096

#define OUT_CMD "-"
/* #define OUT_CMD "tcp://localhost:8080/listen" */
#define MUSIC_FILE "track.opus"

static bool running = true;

float volume = 100;
int pipefd[2];
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

int main() {
  std::thread t(listener);
  t.detach();

  FILE *input = popen((std::string("opusdec ") + MUSIC_FILE " -").c_str(), "r");

  if (!input) {
    perror("popen");
    return 1;
  }

  if (pipe(pipefd) == -1) {
    perror("pipe");
    pclose(input);
    return 1;
  }

  int readfd = pipefd[0];
  int writefd = pipefd[1];

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
      /* puts("din\n"); */
      _exit(status);
    }
    close(pipefd[0]);

    status = dup2(pipefd[1], STDOUT_FILENO);
    if (status == -1) {
      close(pipefd[1]); /* Reader will see EOF */
      close(pipefd[0]); /* Close unused read end */
      /* puts("dout\n"); */
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

  /* out = popen(getCmd().c_str(), "w"); */

  /* if (!out) { */
  /*   fprintf(stderr, "popen\n"); */
  /*   pclose(input); */
  /*   return 1; */
  /* } */

  float current_volume = volume;
  size_t read_size = 0;
  char buffer[BUFFER_SIZE];
  while ((read_size = fread(buffer, 1, BUFFER_SIZE, input))) {
    size_t written_size = 0;
    while ((written_size += write(writefd, buffer + written_size,
                                  read_size - written_size) < read_size))
      ;

    if (volume != current_volume) {
      /* pclose(out); */
      /* out = nullptr; */

      /* out = popen(getCmd().c_str(), "w"); */

      current_volume = volume;
    }
  }

  pclose(input);
  /* pclose(out); */
  fprintf(stderr, "closed\n");

  running = false;

  return 0;
}
