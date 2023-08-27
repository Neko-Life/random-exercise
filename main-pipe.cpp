/*
    Great exercise, now i know how i will implement effects for Musicat
    !TODO: explore more on usage of different effects on ffmpeg
*/
#include <assert.h>
#include <fcntl.h>
#include <iostream>
#include <limits.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <string>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

#define BUFFER_SIZE processing_buffer_size

#define OUT_CMD "pipe:1"
/* #define OUT_CMD "tcp://localhost:8080/listen" */
#define MUSIC_FILE "track.opus"

// careful adjusting this, need to make sure it won't
// block when written at once
static const size_t processing_buffer_size = BUFSIZ * 8;

static bool running = true;

static float volume = 100;
static int ppipefd[2];
static int cpipefd[2];
static pid_t cpid;
static pid_t rpid;

static void listener() {
  float volstr;

  // listen to volume changes
  while (running && std::cin >> volstr) {
    if (volstr > (float)0)
      volume = volstr;
  }
}

static int run_reader(int prreadfd, int crwritefd) {
  // request kernel to kill little self when parent dies
  if (prctl(PR_SET_PDEATHSIG, SIGTERM) == -1) {
    perror("child reader");
    _exit(EXIT_FAILURE);
  }

  close(prreadfd); /* Close unused read end */

  int status;
  status = dup2(crwritefd, STDOUT_FILENO);
  close(crwritefd);
  if (status == -1) {
    perror("reader dout");
    _exit(EXIT_FAILURE);
  }

  // redirect ffmpeg stderr to /dev/null
  int dnull = open("/dev/null", O_WRONLY);
  dup2(dnull, STDERR_FILENO);
  close(dnull);

  execlp("ffmpeg", "ffmpeg", "-v", "debug", "-i", MUSIC_FILE, "-flush_packets",
         "1", "-f", "s16le", "-ac", "2", "-ar", "48000",
         /*"-preset", "ultrafast",*/ OUT_CMD, (char *)NULL);

  perror("reader");
  _exit(EXIT_FAILURE);
}

// p*fd = fd for parent
// c*fd = fd for child
static int run_child(int pwritefd, int creadfd, int preadfd, int cwritefd) {
  // request kernel to kill little self when parent dies
  if (prctl(PR_SET_PDEATHSIG, SIGTERM) == -1) {
    perror("child ffmpeg");
    _exit(EXIT_FAILURE);
  }

  close(pwritefd); /* Close unused write end */
  close(preadfd);  /* Close unused read end */

  // redirect ffmpeg stdin and stdout to prepared pipes
  // exit if unable to redirect
  int status;

  status = dup2(creadfd, STDIN_FILENO);
  close(creadfd);
  if (status == -1) {
    perror("ffmpeg din");
    _exit(EXIT_FAILURE);
  }

  status = dup2(cwritefd, STDOUT_FILENO);
  close(cwritefd);
  if (status == -1) {
    perror("ffmpeg dout");
    _exit(EXIT_FAILURE);
  }

  // redirect ffmpeg stderr to /dev/null
  /* int dnull = open("/dev/null", O_WRONLY); */
  /* dup2(dnull, STDERR_FILENO); */
  /* close(dnull); */

  execlp("ffmpeg", "ffmpeg", "-v", "debug", "-f", "s16le", "-ac", "2", "-ar",
         "48000", "-i", "pipe:0", "-af",
         (std::string("volume=") + std::to_string(volume / (float)100)).c_str(),
         "-flush_packets", "1", "-f", "u8", "-ac", "2", "-ar", "48000",
         /*"-preset", "ultrafast",*/ OUT_CMD, (char *)NULL);

  perror("ffmpeg");
  _exit(EXIT_FAILURE);
}

int main() {
  std::thread t(listener);
  t.detach();

  // decode opus track
  int rpipefd[2];
  if (pipe(rpipefd) == -1) {
    perror("rpipe");
    return EXIT_FAILURE;
  }
  int prreadfd = rpipefd[0];
  int crwritefd = rpipefd[1];

  rpid = fork();
  if (rpid == -1) {
    perror("rfork");
    return EXIT_FAILURE;
  }

  if (rpid == 0) { /* Child reads from pipe */
    return run_reader(prreadfd, crwritefd);
  }

  close(crwritefd);
  crwritefd = -1;

  FILE *input = fdopen(prreadfd, "r");

  // prepare required pipes for bidirectional interprocess communication
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

  // create a child
  cpid = fork();
  if (cpid == -1) {
    perror("fork");
    return EXIT_FAILURE;
  }

  if (cpid == 0) { /* Child reads from pipe */
    return run_child(pwritefd, creadfd, preadfd, cwritefd);
  }

  close(creadfd);  /* Close unused read end */
  close(cwritefd); /* Close unused write end */

  creadfd = -1;
  cwritefd = -1;

  // prepare required data for polling
  struct pollfd prfds[1], pwfds[1];
  prfds[0].events = POLLIN;
  pwfds[0].events = POLLOUT;

  prfds[0].fd = preadfd;
  pwfds[0].fd = pwritefd;

  // main loop
  float current_volume = volume;
  size_t read_size = 0;

  char buffer[BUFFER_SIZE];
  bool read_input = true;
  size_t written_size = 0;

  while (true) {
    if (read_input) {
      read_size = fread(buffer, 1, BUFFER_SIZE, input);
      if (!(read_size > 0))
        break;
    }

    // we don't know how much the resulting buffer is, so the best reliable way
    // with no optimization is to keep polling and write/read byte by byte.
    // polling will only tells whether an fd can be operated on, we won't
    // know if the write/read will be blocked in the middle of operation
    // (waiting for the exact requested amount of space/data to be available)
    // if we were to do more than a byte at a time

    // we can do chunk writing here as as long as the output keeps being read,
    // the buffer will keep being consumed by ffmpeg, as long as it's not too
    // big
    int write_has_event = poll(pwfds, 1, 0);
    bool write_ready = (write_has_event > 0) && (pwfds[0].revents & POLLOUT);
    while (write_ready &&
           ((written_size += write(pwritefd, buffer + written_size,
                                   read_size - written_size)) < read_size)) {
      write_has_event = poll(pwfds, 1, 0);
      write_ready = (write_has_event > 0) && (pwfds[0].revents & POLLOUT);
    }; // keep writing until buffer entirely written

    if (written_size < read_size)
      read_input = false;
    else {
      read_input = true;
      written_size = 0;
    }

    // poll ffmpeg stdout
    int read_has_event = poll(prfds, 1, 0);
    bool read_ready = (read_has_event > 0) && (prfds[0].revents & POLLIN);
    size_t input_read_size = 0;

    if (read_ready) {
      int read_count = 0;

      uint8_t out_buffer;
      while (read_ready &&
             ((input_read_size = read(preadfd, &out_buffer, 1)) > 0)) {
        fwrite(&out_buffer, 1, input_read_size, stdout);

        read_has_event = poll(prfds, 1, 0);
        read_ready = (read_has_event > 0) && (prfds[0].revents & POLLIN);
      }
    }

    // recreate ffmpeg process to update filter chain
    if (volume != current_volume) {
      // close write fd, we can now safely read until
      // eof without worrying about polling and blocked read
      close(pwritefd);

      uint8_t rest_buffer[BUFFER_SIZE];
      // read the rest of data before closing current instance
      while ((input_read_size = read(preadfd, rest_buffer, BUFFER_SIZE)) > 0) {
        fwrite(buffer, 1, input_read_size, stdout);
      }

      // close read fd
      close(preadfd);

      pwritefd = -1;
      preadfd = -1;

      // wait for child to finish transferring data
      int status;
      waitpid(cpid, &status, 0);
      fprintf(stderr, "child status: %d\n", status);

      // kill child
      kill(cpid, SIGTERM);

      // wait for child until it completely died to prevent it becoming zombie
      waitpid(cpid, &status, 0);
      fprintf(stderr, "killed child status: %d\n", status);
      assert(waitpid(cpid, &status, 0) == -1);

      // do the same setup routine as startup
      if (pipe(ppipefd) == -1) {
        perror("ppipe");
        break;
      }
      preadfd = ppipefd[0];
      cwritefd = ppipefd[1];

      if (pipe(cpipefd) == -1) {
        // fds will be closed on exit
        perror("cpipe");
        break;
      }
      creadfd = cpipefd[0];
      pwritefd = cpipefd[1];

      cpid = fork();
      if (cpid == -1) {
        // fds will be closed on exit
        perror("fork");
        break;
      }

      if (cpid == 0) { /* Child reads from pipe */
        return run_child(pwritefd, creadfd, preadfd, cwritefd);
      }

      close(creadfd);  /* Close unused read end */
      close(cwritefd); /* Close unused write end */

      creadfd = -1;
      cwritefd = -1;

      // update fd to poll
      prfds[0].fd = preadfd;
      pwfds[0].fd = pwritefd;

      // mark changes done
      current_volume = volume;
    }
  }

  // exiting, clean up
  fclose(input);
  input = NULL;

  if (creadfd > -1)
    close(creadfd);
  if (pwritefd > -1)
    close(pwritefd);
  if (cwritefd > -1)
    close(cwritefd);
  if (preadfd > -1)
    close(preadfd);

  creadfd = -1;
  pwritefd = -1;
  cwritefd = -1;
  preadfd = -1;

  fprintf(stderr, "fds closed\n");

  // kill child
  kill(cpid, SIGTERM);
  kill(rpid, SIGTERM);

  // wait for childs to make sure they're dead and
  // prevent them to become zombies
  int status;
  fprintf(stderr, "waiting for child\n");
  waitpid(cpid, &status, 0); /* Wait for child */
  fprintf(stderr, "killed cchild status: %d\n", status);
  waitpid(rpid, &status, 0); /* Wait for child */
  fprintf(stderr, "killed rchild status: %d\n", status);
  assert(waitpid(cpid, &status, 0) == -1);
  assert(waitpid(rpid, &status, 0) == -1);

  running = false;

  return 0;
}
