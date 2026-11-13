#pragma once

#include <glim/transport/GuestAbi.h>

#include <cstddef>
#include <span>

// Wasm-only glue for a guest module (K81): the "glim" host imports and the
// linear memory.
namespace glim::guest {

transport::Imports& hostImports();

// All of linear memory; ring offsets are plain addresses in it.
std::span<std::byte> linearMemory();

}  // namespace glim::guest
