/*
    Great exercise, now i know how i will implement effects for Musicat
    !TODO: explore more on usage of different effects on ffmpeg
*/
#include <assert.h>
#include <fcntl.h>
#include <iostream>
#include <poll.h>
#include <stdio.h>
#include <string>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

static const size_t processing_buffer_size = BUFSIZ * 5;

#define BUFFER_SIZE processing_buffer_size
#define MINIMUM_WRITE 128000

#define OUT_CMD "pipe:1"
/* #define OUT_CMD "tcp://localhost:8080/listen" */
#define MUSIC_FILE "track.opus"

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
  /* int dnull = open("/dev/null", O_WRONLY); */
  /* dup2(dnull, STDERR_FILENO); */
  /* close(dnull); */

  execlp("ffmpeg", "ffmpeg", "-i", MUSIC_FILE, "-f", "s16le",
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
  int dnull = open("/dev/null", O_WRONLY);
  dup2(dnull, STDERR_FILENO);
  close(dnull);

  std::string block_size_str("16");

  execlp("ffmpeg", "ffmpeg", "-f", "s16le", "-i", "pipe:0", "-blocksize",
         block_size_str.c_str(), "-af",
         (std::string("volume=") + std::to_string(volume / (float)100)).c_str(),
         "-f", "s16le",
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

  // prepare required data for polling ffmpeg stdout
  nfds_t nfds = 1;
  struct pollfd pfds[1];
  pfds[0].events = POLLIN;

  pfds[0].fd = preadfd;

  // associate fds with FILEs for better write and read handling
  FILE *pwritefile = fdopen(pwritefd, "w");
  FILE *preadfile = fdopen(preadfd, "r");

  // main loop
  int write_attempt = 0;
  float current_volume = volume;
  size_t read_size = 0;
  size_t minimum_write = 0;
  char buffer[BUFFER_SIZE];

  while ((read_size = fread(buffer, 1, BUFFER_SIZE, input)) > 0) {
    fprintf(stderr, "attempt to write: %d\n", ++write_attempt);
    size_t written_size = 0;
    while ((written_size += fwrite(buffer + written_size, 1,
                                   read_size - written_size, pwritefile)) <
           read_size)
      ; // keep writing until buffer entirely written

    minimum_write += written_size;
    fprintf(stderr, "minimum write: %ld\n", minimum_write);

    // poll ffmpeg stdout
    int has_event = poll(pfds, 1, 0);
    const bool read_ready = (has_event > 0) && (pfds[0].revents & POLLIN);

    // check if this might be the last data we can read
    if (read_size < BUFFER_SIZE) {
      fprintf(stderr, "Last straw of data, closing pipe\n");
      // close pipe to send eof to child
      fclose(pwritefile);
      pwritefile = NULL;
    }

    // if minimum_write was hit, meaning the fd was ready before
    // the call to poll, poll is reporting for event, not reporting
    // if data is waiting to read or not
    if (read_ready || (minimum_write >= MINIMUM_WRITE)) {
      while ((read_size = fread(buffer, 1, BUFFER_SIZE, preadfile)) > 0) {
        fwrite(buffer, 1, read_size, stdout);

        // check if this might be the last data we can read
        if (read_size < BUFFER_SIZE)
          break;

        // poll again to see if there's activity after read
        has_event = poll(pfds, 1, 0);
        if ((has_event > 0) && (pfds[0].revents & POLLIN))
          continue;
        else
          break;
      }

      // reset state after successful write and read
      write_attempt = 0;
      minimum_write = 0;
    }

    // recreate ffmpeg process to update filter chain
    if (volume != current_volume) {
      // close opened files
      fclose(pwritefile);
      pwritefile = NULL;
      fclose(preadfile);
      preadfile = NULL;

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
        perror("cpipe");
        break;
      }
      creadfd = cpipefd[0];
      pwritefd = cpipefd[1];

      cpid = fork();
      if (cpid == -1) {
        // error clean up
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
      close(cwritefd); /* Close unused write end */

      // update fd to poll
      pfds[0].fd = preadfd;

      pwritefile = fdopen(pwritefd, "w");
      preadfile = fdopen(preadfd, "r");

      // mark changes done
      current_volume = volume;
    }
  }

  // no more data to read from input, clean up
  fclose(input);
  input = NULL;
  fprintf(stderr, "input closed\n");

  if (pwritefile)
    fclose(pwritefile);

  if (preadfile)
    fclose(preadfile);

  // kill child
  kill(cpid, SIGTERM);
  kill(rpid, SIGTERM);

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
