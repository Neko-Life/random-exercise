#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main() {
  execlp("echo", "echo", "Hello", "World!");
  puts("!\n");
  exit(EXIT_FAILURE);
}
