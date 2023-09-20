#include <assert.h>
#include <deque>
#include <stdint.h>
#include <string>
#include <vector>

// #define MY_DEBUG
// #define USE_DEQUE

#ifndef USE_DEQUE

#define v_container std::vector

#else

#define v_container std::deque
#define CAPABILITY_PUSH_FRONT

#endif

// we would want to have subtraction, multiplication and division too later on!

// simply container for data
// no method allowed!
struct intinfinity_t {
  v_container<short> _v_s;
  bool negative;
  bool is_valid;
  std::string err;
};

std::string intinfinity_t_get_err(intinfinity_t &i) {
  if (!i.err.length()) {
    return i.err;
  }

  const std::string ret = i.err;

  i.err = "";

  return ret;
}

void intinfinity_t_printerr(const char *dbg, intinfinity_t &i) {
  fprintf(stderr, "%s: %s\n", dbg, intinfinity_t_get_err(i).c_str());
}

bool is_char_int(const char &c) {
  static constexpr const char int_chars[] = "0123456789";

  for (const char &ic : int_chars) {
    if (ic == c)
      return true;
  }

  return false;
}

// init container with a string of number, returns 0 on success
int intinfinity_t_init(intinfinity_t &container, const std::string &str) {
  // initialize negative member
  container.negative = false;

  // add a digit in front to make sure there's enough space
  // in the container for one time addition operation
  v_container<short> result(str.length() + 1);

  bool first_idx = true;
  size_t idx = 1;
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

    if (!is_char_int(c)) {
      container.is_valid = false;
      container.err = std::string("Invalid integer: ") + c;
      return 1;
    };

    result[idx++] = c - '0';
  }

  container._v_s = result;
  container.is_valid = true;

  return 0;
}

void print_vs(const intinfinity_t &vs) {
#ifdef MY_DEBUG
  for (const short s : vs._v_s) {
    fprintf(stderr, "%d ", s);
  }

  puts("");
#endif
}

// check whether string number a is bigger than b
bool is_bigger_than_str(const std::string &a, const std::string &b) {
  const size_t a_len = a.length();
  const size_t b_len = b.length();

  if (a_len == b_len) {
    // both string length are the same, compare per digit
    // any digit difference decides which one is bigger here
    for (size_t i = 0; i < a_len; i++) {
      const int ai = a[i] - '0';
      const int bi = b[i] - '0';

      // compare current digit
      if (ai > bi)
        return true;

      if (ai < bi)
        return false;

      // else compare next digit
    }
  }

  else if (a_len > b_len) {
    // a is indeed bigger than b
    return true;
  }

  // a is smaller or equal to b
  return false;
}

std::string intinfinity_t_to_string(const intinfinity_t &i) {
  std::string result = i.negative ? "-" : "";

  bool first_idx = true;
  for (const short s : i._v_s) {
    if (first_idx) {
      if (s == 0)
        continue;

      first_idx = false;
    }

    result += std::to_string(s);
  }

  if (!result.length())
    result += '0';

  return result;
}

// sum `vs` digit at index `i` with `bi`, automatically add second digit of the
// result to the next index in `vs`, this function doesn't alter `vs` size in
// any way as it should have been pre-allocated
int intinfinity_t_add_digit_at(intinfinity_t &vs, const size_t &i,
                               const short bi) {
  const short ai = vs._v_s[i];

  const short ri = ai + bi;
  const bool ri2digit = ri > 9;

  vs._v_s[i] = ri > 0 ? ri % 10 : 0;

  if (ri2digit) {
    if (i == 0) {
      // this should never possibly happen unless addition called more than once
      // to the same container!! check your logic!
      // !TODO: add resize function

      static constexpr const char e[] = "add: vector overflow: index reached 0";

      vs.err = e;

      fprintf(stderr, "%s: %zu\n", e, vs._v_s.size());

      return 1;
    }

    const size_t next_idx = i - 1;
    return intinfinity_t_add_digit_at(vs, next_idx, 1);
  }

  return 0;
}

std::string add(const std::string &a, const std::string &b) {
  // check whether a and b should be swapped, for better performance, the
  // smaller gets added to the bigger
  bool swap = false;
  if (is_bigger_than_str(b, a)) {
    swap = true;
  }

  // with the swap logic, a_v should always contain the bigger
  // number than b_v
  const std::string &bigger_n = swap ? b : a;
  const std::string &smaller_n = swap ? a : b;

  intinfinity_t a_v;
  intinfinity_t b_v;

  int status = 0;
  status = intinfinity_t_init(a_v, bigger_n);
  if (status) {
    intinfinity_t_printerr("intinfinity_t_init a", a_v);
    return "";
  }
  status = intinfinity_t_init(b_v, smaller_n);
  if (status) {
    intinfinity_t_printerr("intinfinity_t_init b", b_v);
    return "";
  }

  print_vs(a_v);
  print_vs(b_v);

  /*
     index difference
            vector length
     diff = 5-4 = 1
     b = 3 (max index accessible)
     a = b + diff
   */

  const size_t idx_diff = a_v._v_s.size() - b_v._v_s.size();

  for (size_t i = (b_v._v_s.size() - 1); i >= 0; i--) {
    const size_t a_idx = i + idx_diff;

    const short ai = a_v._v_s[a_idx];
    const short bi = b_v._v_s[i];

    if (intinfinity_t_add_digit_at(a_v, a_idx, bi))
      break;

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

   9999
   1111
   11110
*/
