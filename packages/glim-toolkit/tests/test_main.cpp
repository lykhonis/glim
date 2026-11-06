#include <iostream>

void test_basics();
void test_layout();
void test_scroll();
void test_motion();
void test_pump();
void test_chrome();
void test_state();
void test_text();
void test_tags();
void test_scoped();
void test_theme();

int main() {
  test_basics();
  test_layout();
  test_scroll();
  test_motion();
  test_pump();
  test_chrome();
  test_state();
  test_text();
  test_tags();
  test_scoped();
  test_theme();

  std::cout << "toolkit tests ok\n";
  return 0;
}
