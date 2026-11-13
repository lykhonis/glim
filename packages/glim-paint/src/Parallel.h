#pragma once

#include <algorithm>
#include <cstddef>
#include <thread>
#include <vector>

namespace glim::paint {

template <class Fn>
void parallelFor(std::size_t count, Fn&& fn) {
  const auto strided = [&](std::size_t begin, std::size_t step) {
    for (std::size_t i = begin; i < count; i += step) fn(i);
  };
#if (defined(__EMSCRIPTEN__) && !defined(__EMSCRIPTEN_PTHREADS__)) || \
    (defined(__wasi__) && !defined(__wasm_atomics__))
  strided(0, 1);
#else
  const std::size_t workers = std::min<std::size_t>(
      count, std::clamp<std::size_t>(std::thread::hardware_concurrency(), 1, 8)
  );
  std::vector<std::thread> threads;
  for (std::size_t t = 1; t < workers; ++t) threads.emplace_back(strided, t, workers);
  strided(0, std::max<std::size_t>(workers, 1));
  for (std::thread& t : threads) t.join();
#endif
}

}  // namespace glim::paint
