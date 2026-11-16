#pragma once

#include <glim/shell/Present.h>
#include <glim/shell/Window.h>

#include <functional>
#include <utility>

#include "glim/toolkit/Place.h"

namespace glim {
namespace toolkit {
namespace host {

// Engine service: run() provides one backed by its window.
struct Presenter {
  std::function<void(const shell::PresentRequest&, shell::PresentDone)> present;
};

// after runs once each result has reached the app (run() repaints there).
inline Presenter windowPresenter(shell::Window& window, std::function<void()> after) {
  return {[&window, after = std::move(after)](
            const shell::PresentRequest& request, shell::PresentDone done
          ) {
    window.present(request, [after, done = std::move(done)](const shell::PresentResult& r) {
      done(r);
      after();
    });
  }};
}

// Without a Presenter (tests, offscreen) done gets Unsupported at once.
inline void present(
  const Engine& e, const shell::PresentRequest& request, shell::PresentDone done
) {
  const auto presenter = e.get<Presenter>();
  if (!presenter || !presenter->get().present) {
    done({shell::PresentStatus::Unsupported, {}});
    return;
  }
  presenter->get().present(request, std::move(done));
}

}  // namespace host
}  // namespace toolkit
}  // namespace glim
