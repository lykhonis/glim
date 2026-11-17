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
void test_tilt();
void test_scoped();
void test_theme();
void test_hit();
void test_text_edit();
void test_text_field();

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
  test_tilt();
  test_scoped();
  test_theme();
  test_hit();
  test_text_edit();
  test_text_field();

  std::cout << "toolkit tests ok\n";
  return 0;
}
