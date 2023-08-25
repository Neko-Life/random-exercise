#include <iostream>
#include <string>
#include <thread>

#define BUFFER_SIZE 4096

#define OUT_CMD "-"
/* #define OUT_CMD "tcp://localhost:8080/listen" */
#define MUSIC_FILE "track.opus"

static bool running = true;

float volume = 100;
FILE *out;

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

  FILE *input =
      popen((std::string("opusdec ") + MUSIC_FILE " -").c_str(), "r");

  if (!input) {
    fprintf(stderr, "popen\n");
    return 1;
  }

  out = popen(getCmd().c_str(), "w");

  if (!out) {
    fprintf(stderr, "popen\n");
    pclose(input);
    return 1;
  }

  float current_volume = volume;
  size_t read_size = 0;
  char buffer[BUFFER_SIZE];
  while ((read_size = fread(buffer, 1, BUFFER_SIZE, input))) {
    size_t written_size = 0;
    while ((written_size += fwrite(buffer + written_size, 1,
                                   read_size - written_size, out) < read_size))
      ;

    if (volume != current_volume) {
      pclose(out);
      out = nullptr;

      out = popen(getCmd().c_str(), "w");

      current_volume = volume;
    }
  }

  pclose(input);
  pclose(out);
  fprintf(stderr, "closed\n");

  running = false;

  return 0;
}
