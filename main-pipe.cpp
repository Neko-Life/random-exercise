/*
    Great exercise, now i know how i will implement effects for Musicat
    Very great exercise!! So much fun learning how this works!
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

// BUFFER_SIZE%READ_CHUNK_SIZE should be 0 to guarantee
// complete read write cycle, knowing chunk size
// reduce cpu usage greatly as reading byte by byte
// is REALLY cpu intensive
#define BUFFER_SIZE processing_buffer_size
// found out ffmpeg default write chunk to stdout
// this can also be interpreted as BUFSIZ/2
#define READ_CHUNK_SIZE 4096

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

  execlp("ffmpeg", "ffmpeg", "-v", "debug", "-i", MUSIC_FILE, "-f", "s16le",
         "-ac", "2", "-ar", "48000",
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
  // commented for debugging purpose
  /* int dnull = open("/dev/null", O_WRONLY); */
  /* dup2(dnull, STDERR_FILENO); */
  /* close(dnull); */

  execlp("ffmpeg", "ffmpeg", "-v", "debug", "-f", "s16le", "-ac", "2", "-ar",
         "48000", "-i", "pipe:0", "-af",
         (std::string("volume=") + std::to_string(volume / (float)100)).c_str(),
         "-f", "s16le", "-ac", "2", "-ar", "48000",
         /*"-preset", "ultrafast",*/ OUT_CMD, (char *)NULL);

  perror("ffmpeg");
  _exit(EXIT_FAILURE);
}

int main() {
  // this is BAD!! Never create a thread before doing any fork!
  // Rather always fork than mixing multi-threading with forking
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
    //
    // as safe chunk read size is known we no longer need to read byte by byte

    // we can do chunk writing here as as long as the output keeps being read,
    // the buffer will keep being consumed by ffmpeg, as long as it's not too
    // big it won't block to wait for buffer space
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

      // with known chunk size that always guarantee correct read size that
      // empties ffmpeg buffer, poll will report correctly that the pipe
      // is really empty so we know exactly when to stop reading, this is
      // where it's problematic when BUFFER_SIZE%READ_CHUNK_SIZE doesn't equal 0
      uint8_t out_buffer[BUFFER_SIZE];
      while (read_ready &&
             ((input_read_size += read(preadfd, out_buffer + input_read_size,
                                       READ_CHUNK_SIZE)) > 0)) {

        // write only when buffer is full
        if (input_read_size == BUFFER_SIZE) {
          size_t written_out = 0;
          while ((written_out += fwrite(out_buffer + written_out, 1,
                                        input_read_size - written_out,
                                        stdout)) < input_read_size)
            ;
          fprintf(stderr, "Written to stdout: %ld\n", input_read_size);
          input_read_size = 0;
        }

        // ALWAYS do polling to decide whether to read again
        read_has_event = poll(prfds, 1, 0);
        read_ready = (read_has_event > 0) && (prfds[0].revents & POLLIN);
      }

      // empties the last buffer that usually size less than BUFFER_SIZE
      // which usually left with 0 remainder when divided by
      // READ_CHUNK_SIZE, otherwise can means ffmpeg have chunked audio packets
      // with its specific size as its output when specifying some other
      // containerized format
      fprintf(stderr, "Current last written to stdout: %ld\n", input_read_size);
      if (input_read_size > 0)
        fwrite(out_buffer, 1, input_read_size, stdout);
    }

    // recreate ffmpeg process to update filter chain
    if (volume != current_volume) {
      // close write fd, we can now safely read until
      // eof without worrying about polling and blocked read
      close(pwritefd);

      // always read the rest of data after closing ffmpeg stdin before closing
      // its stdout
      uint8_t rest_buffer[BUFFER_SIZE];
      while ((input_read_size = read(preadfd, rest_buffer, BUFFER_SIZE)) > 0) {
        fprintf(stderr, "Written pre-reloading chain: %ld\n", input_read_size);
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

      // we don't need to kill here as ffmpeg will exit
      // when the write end (its stdin) of pipe is closed
      // kill child
      // kill(cpid, SIGTERM);

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
        // fds will be closed on program exit
        close(cwritefd);
        perror("cpipe");
        break;
      }
      creadfd = cpipefd[0];
      pwritefd = cpipefd[1];

      cpid = fork();
      if (cpid == -1) {
        // fds will be closed on exit
        // i know we should close
        // child's pipes here
        close(cwritefd);
        close(creadfd);
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
  // close reader stdout
  fclose(input);
  input = NULL;

  // close ffmpeg stdin
  // acknowledge that its stdout still have
  // some data to read which should be read
  // immediately after closing its stdin
  close(pwritefd);
  pwritefd = -1;

  size_t last_read_size = 0;
  uint8_t rest_buffer[BUFFER_SIZE];
  // read the rest of data before closing ffmpeg stdout
  while ((last_read_size = read(preadfd, rest_buffer, BUFFER_SIZE)) > 0) {
    fwrite(buffer, 1, last_read_size, stdout);
  }

  // data have been read entirely
  // close it
  close(preadfd);
  preadfd = -1;

  fprintf(stderr, "fds closed\n");

  // we don't NEED to do this!
  // the whole program should exit correctly now without broken pipe error
  // yayy!!
  // kill child
  // kill(cpid, SIGTERM);
  // kill(rpid, SIGTERM);

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
