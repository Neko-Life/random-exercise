#include <assert.h>
#include <deque>
#include <string>

std::deque<short> str_to_vs(const std::string &str) {
  std::deque<short> result;

  for (const char &c : str) {
    result.push_back(c - '0');
  }

  return result;
}

void print_vs(const std::deque<short> &vs) {
  for (const short s : vs) {
    fprintf(stderr, "%d ", s);
  }

  puts("");
}

int sub_vs(std::deque<short> &vs) {
  const size_t vs_len = vs.size();

  if (vs_len == 0)
    return 0;

  size_t sub_idx = 1;
  size_t last_idx = vs_len - sub_idx;
  short last_digit = 0;
  while ((last_digit = vs.at(last_idx)) < 1) {
    sub_idx++;

    if (vs_len == 1)
      return 0;

    last_idx = vs_len - sub_idx;
  }

  vs.at(last_idx) = last_digit - 1;

  while (sub_idx > 1) {
    sub_idx--;
    last_idx = vs_len - sub_idx;

    vs.at(last_idx) = 9;
  }

  if (vs.front() == 0) {
    vs.erase(vs.begin());
  }

  return 1;
}

int add_vs(std::deque<short> &vs) {
  size_t vs_len = vs.size();

  size_t sub_idx = 1;
  size_t last_idx = vs_len - sub_idx;
  short last_digit = 9;
  while ((last_digit = vs.at(last_idx)) == 9) {
    sub_idx++;

    if (last_idx == 0) {
      vs.push_front(0);
      vs_len = vs.size();
    }

    last_idx = vs_len - sub_idx;
  }

  vs.at(last_idx) = last_digit + 1;

  while (sub_idx > 1) {
    sub_idx--;
    last_idx = vs_len - sub_idx;

    vs.at(last_idx) = 0;
  }

  return 1;
}

std::string add(const std::string &a, const std::string &b) {
  std::deque<short> a_v = str_to_vs(a);
  std::deque<short> b_v = str_to_vs(b);

  // print_vs(a_v);
  // print_vs(b_v);

  while (sub_vs(b_v)) {
    add_vs(a_v);
  }

  std::string result = "";

  for (const short s : a_v) {
    result += std::to_string(s);
  }

  return result;
}

int main(const int argc, const char *argv[]) {
  const std::string result = add(argv[1], argv[2]);

  printf("result: %s\n", result.c_str());
  /* assert(result == "5000"); */
  return 0;
}
