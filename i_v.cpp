#include <assert.h>
#include <deque>
#include <string>
#include <vector>

// #define v_container std::vector
#define v_container std::deque

v_container<short> str_to_vs(const std::string &str) {
  v_container<short> result;

  for (const char &c : str) {
    result.push_back(c - '0');
  }

  return result;
}

void print_vs(const v_container<short> &vs) {
  for (const short s : vs) {
    fprintf(stderr, "%d ", s);
  }

  puts("");
}

int sub_vs(v_container<short> &vs) {
  const size_t vs_len = vs.size();

  if (vs_len == 0)
    return 0;

  size_t sub_idx = 1;
  size_t last_idx = vs_len - sub_idx;
  short last_digit = 0;
  while ((last_digit = vs[last_idx]) < 1) {
    if (last_idx == 0)
      return 0;

    sub_idx++;

    last_idx = vs_len - sub_idx;
  }

  vs[last_idx] = last_digit - 1;

  while (sub_idx > 1) {
    sub_idx--;
    last_idx = vs_len - sub_idx;

    vs[last_idx] = 9;
  }

  // if (vs.front() == 0) {
  //   vs.erase(vs.begin());
  // }

  return 1;
}

int add_vs(v_container<short> &vs) {
  size_t vs_len = vs.size();

  size_t sub_idx = 1;
  size_t last_idx = vs_len - sub_idx;
  short last_digit = 9;
  while ((last_digit = vs[last_idx]) == 9) {
    sub_idx++;

    if (last_idx == 0) {
      vs.push_front(0);
      vs_len = vs.size();
    }

    last_idx = vs_len - sub_idx;
  }

  vs[last_idx] = last_digit + 1;

  while (sub_idx > 1) {
    sub_idx--;
    last_idx = vs_len - sub_idx;

    vs[last_idx] = 0;
  }

  return 1;
}

// both str length should be equal
bool is_bigger_str(const std::string &a, const std::string &b) {
  for (size_t i = 0; i < a.length(); i++) {
    const char &ac = a[i];
    const char &bc = b[i];

    const int ai = ac - '0';
    const int bi = bc - '0';

    if (ai > bi)
      return true;
    else if (ai < bi)
      return false;
  }

  // both str are equal
  return false;
}

std::string add(const std::string &a, const std::string &b) {
  // check whether a and b should be swapped, for better performance, the
  // smaller gets added to the bigger
  bool swap = false;
  if (b.length() > a.length()) {
    swap = true;
  }

  // both string length are the same, compare per digit
  else if (is_bigger_str(b, a)) {
    swap = true;
  }

  v_container<short> a_v = str_to_vs(swap ? b : a);
  v_container<short> b_v = str_to_vs(swap ? a : b);

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
