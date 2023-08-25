#include <stdio.h>
#include <unistd.h>

int main() {
  char buf;
  while (read(STDIN_FILENO, &buf, 1) > 0) {
    putc(buf, stdout);
  }
}
