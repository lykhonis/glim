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
#include "glim/toolkit/tags/Center.h"
#include "glim/toolkit/tags/Effect.h"
#include "glim/toolkit/tags/Expand.h"
#include "glim/toolkit/tags/Glass.h"
#include "glim/toolkit/tags/Hit.h"
#include "glim/toolkit/tags/Orientation.h"
#include "glim/toolkit/tags/Pad.h"
#include "glim/toolkit/tags/Role.h"
#include "glim/toolkit/tags/Rotate.h"
#include "glim/toolkit/tags/Scroll.h"
#include "glim/toolkit/tags/Semantic.h"
#include "glim/toolkit/tags/Shadow.h"
#include "glim/toolkit/tags/Shape.h"
#include "glim/toolkit/tags/Spacing.h"
#include "glim/toolkit/tags/Transform.h"
#include "glim/toolkit/tags/Wrap.h"
