// Defaulted here: Block is incomplete in element headers and vector instantiates eagerly.
#include "glim/toolkit/Block.h"

namespace glim {
namespace toolkit {

Row::Row() = default;
Row::~Row() = default;
Row::Row(const Row&) = default;
Row& Row::operator=(const Row&) = default;
Row::Row(Row&&) = default;
Row& Row::operator=(Row&&) = default;

Column::Column() = default;
Column::~Column() = default;
Column::Column(const Column&) = default;
Column& Column::operator=(const Column&) = default;
Column::Column(Column&&) = default;
Column& Column::operator=(Column&&) = default;

Stack::Stack() = default;
Stack::~Stack() = default;
Stack::Stack(const Stack&) = default;
Stack& Stack::operator=(const Stack&) = default;
Stack::Stack(Stack&&) = default;
Stack& Stack::operator=(Stack&&) = default;

Tap::Tap() = default;
Tap::~Tap() = default;
Tap::Tap(const Tap&) = default;
Tap& Tap::operator=(const Tap&) = default;
Tap::Tap(Tap&&) = default;
Tap& Tap::operator=(Tap&&) = default;

List::List() = default;
List::~List() = default;
List::List(const List&) = default;
List& List::operator=(const List&) = default;
List::List(List&&) = default;
List& List::operator=(List&&) = default;

Roll::Roll() = default;
Roll::~Roll() = default;
Roll::Roll(const Roll&) = default;
Roll& Roll::operator=(const Roll&) = default;
Roll::Roll(Roll&&) = default;
Roll& Roll::operator=(Roll&&) = default;

Box Tap::place(Constraints c, const Engine& e) const {
  Constraints ic = detail::inset(c, static_cast<float>(2 * padH), static_cast<float>(2 * padV));
  Box inner = e.place(children[0], ic);
  return detail::resolveSize(
      inner.w + static_cast<float>(2 * padH), inner.h + static_cast<float>(2 * padV), c
  );
}

}  // namespace toolkit
}  // namespace glim
