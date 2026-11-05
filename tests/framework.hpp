#pragma once
#include <cstdio>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace tf {

struct TestCase {
  const char* name;
  void (*fn)();
};

inline std::vector<TestCase>& registry() {
  static std::vector<TestCase> r;
  return r;
}

inline int& failures() {
  static int f = 0;
  return f;
}

struct Registrar {
  Registrar(const char* n, void (*f)()) { registry().push_back({n, f}); }
};

inline void fail(const char* file, int line, const std::string& what) {
  ++failures();
  std::fprintf(stderr, "  FAIL %s:%d: %s\n", file, line, what.c_str());
}

template <typename T, typename = void>
struct Streamable : std::false_type {};
template <typename T>
struct Streamable<T, std::void_t<decltype(std::declval<std::ostream&>() << std::declval<const T&>())>>
    : std::true_type {};

template <typename T>
std::string show(const T& v) {
  if constexpr (Streamable<T>::value) {
    std::ostringstream o;
    o << v;
    std::string s = o.str();
    if (s.size() > 200) s = s.substr(0, 200) + "...";
    return s;
  } else {
    return "<value>";
  }
}

}

#define TF_CAT2(a, b) a##b
#define TF_CAT(a, b) TF_CAT2(a, b)

#define TEST(name)                                                  \
  static void name();                                               \
  static tf::Registrar TF_CAT(name, _registrar)(#name, &name);      \
  static void name()

#define CHECK(cond)                                    \
  do {                                                 \
    if (!(cond)) tf::fail(__FILE__, __LINE__, #cond);  \
  } while (0)

#define CHECK_EQ(a, b)                                                                     \
  do {                                                                                     \
    const auto& tf_a_ = (a);                                                               \
    const auto& tf_b_ = (b);                                                               \
    if (!(tf_a_ == tf_b_))                                                                 \
      tf::fail(__FILE__, __LINE__,                                                         \
               std::string(#a " == " #b "  [") + tf::show(tf_a_) + " vs " + tf::show(tf_b_) + "]"); \
  } while (0)

#define CHECK_OK(expr)                                                         \
  do {                                                                         \
    ::chr::Status tf_s_ = (expr);                                              \
    if (!tf_s_.ok()) tf::fail(__FILE__, __LINE__, #expr " -> " + tf_s_.msg);   \
  } while (0)
