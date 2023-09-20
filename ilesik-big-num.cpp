#define V3

#ifdef V1

#include <algorithm>
#include <iostream>
#include <vector>

class BigInt {
private:
  static const int DIGITS_PER_CELL = 18;
  long long int biggest_cell_value;
  std::vector<long long int> value;

public:
  BigInt(const std::string &string) {
    std::string nines;
    for (int i = 0; i < DIGITS_PER_CELL; i++) {
      nines += '9';
    }
    biggest_cell_value = std::stoll(nines);

    size_t cnt = string.size() / DIGITS_PER_CELL + 1;
    value.resize(cnt);
    for (size_t i = 0; i < string.size(); i += DIGITS_PER_CELL) {

      auto temp = string.substr(i, DIGITS_PER_CELL);
      value[i / DIGITS_PER_CELL] = std::stoll(temp);
    }
  }

  operator std::string() const {
    std::string r;
    r.resize(value.size() * DIGITS_PER_CELL);
    for (size_t ind = 0; ind < value.size(); ind++) {
      auto num_string = std::to_string(value[ind]);
      size_t local_offset = 0;
      while (local_offset + num_string.size() < DIGITS_PER_CELL) {
        r[ind * DIGITS_PER_CELL + local_offset] = '0';
        local_offset++;
      }
      local_offset = ind * DIGITS_PER_CELL + local_offset;
      for (size_t string_ind = 0; string_ind < num_string.size();
           string_ind++) {
        r[local_offset + string_ind] = num_string[string_ind];
      }
    }
    return r;
  }

  BigInt operator+(const BigInt &other) const {
    BigInt result("0"); // Initialize the result as 0
    size_t max_size = std::max(value.size(), other.value.size());
    result.value.resize(max_size);

    long long carry = 0;
    for (size_t i = 0; i < max_size; i++) {
      long long sum = carry;
      if (i < value.size()) {
        sum += value[i];
      }
      if (i < other.value.size()) {
        sum += other.value[i];
      }

      result.value[i] = sum % biggest_cell_value;
      carry = sum / biggest_cell_value;
    }

    if (carry > 0) {
      result.value.push_back(carry); // Add any remaining carry
    }

    return result;
  }

  friend std::ostream &operator<<(std::ostream &out, const BigInt &n) {
    return out << std::string(n);
  }
};

#endif // V1

#ifdef V2

#include <algorithm>
#include <iostream>
#include <vector>

class BigInt {
private:
  static const int DIGITS_PER_CELL = 18;
  long long int biggest_cell_value;
  std::vector<long long int> value;

public:
  BigInt(const std::string &string) {
    std::string nines;
    for (int i = 0; i < DIGITS_PER_CELL; i++) {
      nines += '9';
    }
    biggest_cell_value = std::stoll(nines);

    size_t cnt = string.size() / DIGITS_PER_CELL + 1;
    value.resize(cnt);
    for (size_t i = 0; i < string.size(); i += DIGITS_PER_CELL) {

      auto temp = string.substr(i, DIGITS_PER_CELL);
      value[i / DIGITS_PER_CELL] = std::stoll(temp);
    }
  }

  BigInt(const std::vector<long long int> &v) {
    std::string nines;
    for (int i = 0; i < DIGITS_PER_CELL; i++) {
      nines += '9';
    }
    value = v;
  }

  operator std::string() const {
    std::string r;
    bool fst_n = false;
    r.resize(value.size() * DIGITS_PER_CELL);
    for (size_t ind = 0; ind < value.size(); ind++) {
      auto num_string = std::to_string(value[ind]);
      size_t local_offset = 0;
      while (local_offset + num_string.size() < DIGITS_PER_CELL && fst_n) {
        r[ind * DIGITS_PER_CELL + local_offset] = '0';
        local_offset++;
      }
      local_offset = ind * DIGITS_PER_CELL + local_offset;
      for (size_t string_ind = 0; string_ind < num_string.size();
           string_ind++) {
        r[local_offset + string_ind] = num_string[string_ind];
        fst_n = true;
      }
    }
    return r;
  }

  friend std::ostream &operator<<(std::ostream &out, const BigInt &n) {
    return out << std::string(n);
  }

  BigInt operator+(const BigInt &other) const {
    size_t max_size = std::max(value.size(), other.value.size());
    std::vector<long long int> lhs(value);
    std::vector<long long int> rhs(other.value);
    while (lhs.size() < max_size) {
      lhs.insert(lhs.begin(), 0);
    }
    while (rhs.size() < max_size) {
      rhs.insert(rhs.begin(), 0);
    }

    std::vector<long long int> result(max_size);

    long long int carry = 0;

    for (size_t i = max_size; i-- > 0;) {
      long long int sum = lhs[i] + rhs[i] + carry;
      result[i] = sum % biggest_cell_value;
      carry = sum / biggest_cell_value;
    }

    if (carry > 0) {
      result.insert(result.begin(), carry);
    }
    return BigInt(result);
  }
};

#endif // V2

#ifdef V3

#include <algorithm>
#include <iostream>
#include <vector>

class BigInt {
private:
  static const int DIGITS_PER_CELL = 18;
  long long int biggest_cell_value;
  std::vector<long long int> value;

public:
  BigInt(const std::string &string) {
    std::string nines;
    for (int i = 0; i < DIGITS_PER_CELL; i++) {
      nines += '9';
    }
    biggest_cell_value = std::stoll(nines);

    size_t cnt = string.size() / DIGITS_PER_CELL + 1;
    value.resize(cnt);
    for (size_t i = 0; i < string.size(); i += DIGITS_PER_CELL) {

      auto temp = string.substr(i, DIGITS_PER_CELL);
      value[i / DIGITS_PER_CELL] = std::stoll(temp);
    }
  }

  BigInt(const std::vector<long long int> &v) {
    std::string nines;
    for (int i = 0; i < DIGITS_PER_CELL; i++) {
      nines += '9';
    }
    value = v;
  }

  operator std::string() const {
    std::string r;
    bool fst_n = false;
    r.resize(value.size() * DIGITS_PER_CELL);
    for (size_t ind = 0; ind < value.size(); ind++) {
      auto num_string = std::to_string(value[ind]);
      size_t local_offset = 0;
      while (local_offset + num_string.size() < DIGITS_PER_CELL && fst_n) {
        r[ind * DIGITS_PER_CELL + local_offset] = '0';
        local_offset++;
      }
      local_offset = ind * DIGITS_PER_CELL + local_offset;
      for (size_t string_ind = 0; string_ind < num_string.size();
           string_ind++) {
        r[local_offset + string_ind] = num_string[string_ind];
        fst_n = true;
      }
    }
    return r;
  }

  friend std::ostream &operator<<(std::ostream &out, const BigInt &n) {
    return out << std::string(n);
  }

  BigInt operator+(const BigInt &other) const {
    size_t max_size = std::max(value.size(), other.value.size());
    std::vector<long long int> result(max_size);
    size_t diff = abs(value.size() - other.value.size());
    auto &b_v = value.size() > other.value.size() ? value : other.value;
    auto &s_v = value.size() > other.value.size() ? other.value : value;

    for (size_t i = 0; i < diff; i++) {
      result[i] = b_v[i];
    }

    long long int carry = 0;
    long long int a;
    long long int b;
    for (size_t i = max_size - diff; i-- > 0;) {
      a = b_v[i + diff];
      b = s_v[i];
      long long int sum = a + b + carry;
      result[i + diff] = sum % biggest_cell_value;
      carry = sum / biggest_cell_value;
    }

    if (carry > 0) {
      result.insert(result.begin(), carry);
    }
    return BigInt(result);
  }
};

#endif

int main(const int argc, const char *argv[]) {
  BigInt a(argv[1]);
  BigInt b(argv[2]);
  std::cout << (a + b) << '\n';
}
