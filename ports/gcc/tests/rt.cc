#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <stdexcept>
#include <memory>
#include <thread>
#include <cmath>
struct G { G() { std::cout << "static ctor\n"; } ~G() { std::cout << "static dtor\n"; } } g;
struct Base { virtual ~Base() {} virtual const char *n() { return "base"; } };
struct Der : Base { const char *n() override { return "derived"; } };
int main() {
  std::vector<int> v{5, 3, 9, 1}; std::sort(v.begin(), v.end());
  std::map<std::string, int> m{{"a", 1}, {"b", 2}};
  std::cout << "sorted:"; for (int x : v) std::cout << ' ' << x; std::cout << "\n";
  try { throw std::runtime_error("boom"); } catch (const std::exception &e) { std::cout << "caught: " << e.what() << "\n"; }
  try { std::vector<int> w; (void)w.at(3); } catch (std::out_of_range &) { std::cout << "out_of_range ok\n"; }
  auto p = std::make_unique<std::string>("unique"); std::cout << *p << " " << m["b"] << "\n";
  Base *b = new Der; std::cout << b->n() << " " << (dynamic_cast<Der *>(b) ? "dyncast ok" : "dyncast FAIL") << "\n"; delete b;
  std::cout << std::sqrt(2.0) << "\n";
  std::thread t([] { std::cout << "in thread\n"; }); t.join();
  return 0;
}
