#include <sstream>
#include <string>

std::string hex_to_str(long h) {
  std::stringstream s;
  s << std::hex << h;
  return s.str();
}

int main() {
    fprintf(stderr, "%s\n", hex_to_str(0x10).c_str());
    fprintf(stderr, "%s\n", hex_to_str(0x00).c_str());
    fprintf(stderr, "%s\n", hex_to_str(0x26).c_str());
    return 0;
}
