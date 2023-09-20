#include <assert.h>
#include <deque>
#include <stdexcept>
#include <stdint.h>
#include <string>
#include <vector>

// #define USE_DEQUE

#ifndef USE_DEQUE

#define v_container std::vector

#else

#define v_container std::deque
#define CAPABILITY_PUSH_FRONT

#endif

// try to have signed type here,
// we would want to have subtraction too
// later on!
#ifndef INTEGER_TYPE_MAX

#define INTEGER_TYPE_MAX INT64_MAX

#endif

static inline const std::string integer_type_max_str =
    std::to_string(INTEGER_TYPE_MAX);

struct intinfinity_t {
  v_container<short> _v_s;
  bool negative;
  bool is_valid;
  std::string err;
};

std::string inintinfinity_t_get_err(intinfinity_t &i) {
  if (!i.err.length()) {
    return i.err;
  }

  const std::string ret = i.err;

  i.err = "";

  return ret;
}

bool is_char_int(const char &c) {
  static constexpr const char int_chars[] = "0123456789";

  for (const char &ic : int_chars) {
    if (ic == c)
      return true;
  }

  return false;
}

void str_to_vs(intinfinity_t &container, const std::string &str) {
  v_container<short> result;

  bool first_idx = true;
  for (const char &c : str) {
    if (first_idx) {
      first_idx = false;

      if (c == '-') {
        container.negative = true;
        continue;
      }

      if (c == '+') {
        continue;
      }
    }

    if (is_char_int(c)) {
      container.is_valid = false;
      container.err = std::string("Invalid integer: ") + c;
      return;
    };

    result.push_back(c - '0');
  }

  container._v_s = result;
  container.is_valid = true;
}

void print_vs(const intinfinity_t &vs) {
  for (const short s : vs._v_s) {
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
#ifdef CAPABILITY_PUSH_FRONT
  size_t vs_len = vs.size();
#else
  const size_t vs_len = vs.size();
#endif

  size_t sub_idx = 1;
  size_t last_idx = vs_len - sub_idx;
  short last_digit = 9;
  while ((last_digit = vs[last_idx]) == 9) {
    sub_idx++;

    if (last_idx == 0) {
#ifdef CAPABILITY_PUSH_FRONT
      vs.push_front(0);
      vs_len = vs.size();
#else
      throw std::overflow_error("Container size doesn't support push_front and "
                                "is not large enough to contain more digit");
#endif
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
bool is_bigger_than_str(const std::string &a, const std::string &b) {
  const size_t a_len = a.length();
  const size_t b_len = b.length();

  if (a_len == b_len) {
    // both string length are the same, compare per digit
    for (size_t i = 0; i < a_len; i++) {
      const char &ac = a[i];
      const char &bc = b[i];

      const int ai = ac - '0';
      const int bi = bc - '0';

      if (ai > bi)
        return true;

      if (ai < bi)
        return false;
    }
  }

  if (a_len > b_len) {
    // a is indeed bigger than b
    return true;
  }

  if (a_len < b_len) {
    // nope
    return false;
  }

  // both str are equal
  return false;
}

std::string intinfinity_t_to_string(const intinfinity_t &i) {
  std::string result = "";

  for (const short s : i._v_s) {
    result += std::to_string(s);
  }

  return result;
}

std::string add(const std::string &a, const std::string &b) {
  // check whether a and b should be swapped, for better performance, the
  // smaller gets added to the bigger
  bool swap = false;
  if (is_bigger_than_str(b, a)) {
    swap = true;
  }

  // with the swap logic, a_v will always contain the bigger
  // number than b_v
  intinfinity_t a_v;
  intinfinity_t b_v;
  str_to_vs(a_v, swap ? b : a);
  str_to_vs(b_v, swap ? a : b);

  print_vs(a_v);
  print_vs(b_v);

  for (size_t i = b_v._v_s.size(); i >= 0; i--) {
    // !TODO
    if (i == 0)
      break;
  }

  print_vs(a_v);
  print_vs(b_v);

  return intinfinity_t_to_string(a_v);
}

int main(const int argc, const char *argv[]) {
  const std::string result = add(argv[1], argv[2]);

  printf("result: %s\n", result.c_str());
  /* assert(result == "5000"); */
  return 0;
}

/*
   123
   210
   333

   666
   444
   1110
*/
