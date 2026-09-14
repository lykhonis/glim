#pragma once

#include <cstdint>
#include <unordered_map>

namespace glim::shell {

// Embedder-owned native view. Glim does not retain or style it.
struct SlotNative {
    void* view = nullptr;  // NSView* / UIView* / ANativeWindow* / wl_subsurface*
};

class SlotTable {
public:
    void attach(std::uint32_t id, void* view) {
        if (id == 0 || view == nullptr) {
            return;
        }
        views_[id] = view;
    }

    void* get(std::uint32_t id) const {
        const auto it = views_.find(id);
        return it == views_.end() ? nullptr : it->second;
    }

    void* detach(std::uint32_t id) {
        const auto it = views_.find(id);
        if (it == views_.end()) {
            return nullptr;
        }
        void* view = it->second;
        views_.erase(it);
        return view;
    }

    template <typename Fn>
    void forEach(Fn&& fn) const {
        for (const auto& e : views_) {
            fn(e.first, e.second);
        }
    }

    void clear() { views_.clear(); }

private:
    std::unordered_map<std::uint32_t, void*> views_;
};

}  // namespace glim::shell
