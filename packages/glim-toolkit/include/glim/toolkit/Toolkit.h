#pragma once

// Full umbrella: everything a widget/app author needs. Granular
// headers stay for toolkit internals and tests that want isolation.
// Layers (bottom-up): state (State/Scope/Motion/Anim/Place),
// structure (Block/Layout/Measure/Custom/Emit/Ops/Factories/elements),
// style (Theme/Tags/Tokens/text/theme), interaction
// (Screen/Focus/Event/Action/A11y/Walk/DirtyGate/Configure).
#include "glim/toolkit/A11y.h"
#include "glim/toolkit/Action.h"
#include "glim/toolkit/Anim.h"
#include "glim/toolkit/Block.h"
#include "glim/toolkit/Configure.h"
#include "glim/toolkit/ConfigureFwd.h"
#include "glim/toolkit/Custom.h"
#include "glim/toolkit/DirtyGate.h"
#include "glim/toolkit/Emit.h"
#include "glim/toolkit/Event.h"
#include "glim/toolkit/Factories.h"
#include "glim/toolkit/Focus.h"
#include "glim/toolkit/Layout.h"
#include "glim/toolkit/Measure.h"
#include "glim/toolkit/Motion.h"
#include "glim/toolkit/Ops.h"
#include "glim/toolkit/Place.h"
#include "glim/toolkit/Scope.h"
#include "glim/toolkit/Screen.h"
#include "glim/toolkit/State.h"
#include "glim/toolkit/Tagged.h"
#include "glim/toolkit/Tags.h"
#include "glim/toolkit/Theme.h"
#include "glim/toolkit/Tokens.h"
#include "glim/toolkit/Walk.h"
#include "glim/toolkit/elements/Layer.h"
#include "glim/toolkit/text/Text.h"
#include "glim/toolkit/theme/Themed.h"
