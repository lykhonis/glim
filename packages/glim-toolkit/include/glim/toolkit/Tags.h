#pragma once

#include <type_traits>

namespace glim {
namespace toolkit {

template <class T>
struct is_tag : std::false_type {};

template <class T>
struct is_slot_tag : std::false_type {};

}  // namespace toolkit
}  // namespace glim

#include "glim/toolkit/tags/Align.h"
#include "glim/toolkit/tags/Expand.h"
#include "glim/toolkit/tags/Hit.h"
#include "glim/toolkit/tags/Orientation.h"
#include "glim/toolkit/tags/Pad.h"
#include "glim/toolkit/tags/Role.h"
#include "glim/toolkit/tags/Scroll.h"
#include "glim/toolkit/tags/Semantic.h"
#include "glim/toolkit/tags/Shape.h"
#include "glim/toolkit/tags/Spacing.h"
#include "glim/toolkit/tags/Wrap.h"
