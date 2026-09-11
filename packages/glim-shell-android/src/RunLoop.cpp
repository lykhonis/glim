#include <glim/shell/RunLoop.h>

#include "App.h"

#include <android/looper.h>

#include <iterator>
#include <sys/eventfd.h>
#include <sys/timerfd.h>
#include <unistd.h>

#include <cerrno>
#include <cstdint>

namespace glim::shell {
namespace {

constexpr int kWakeIdent = LOOPER_ID_USER;
constexpr int kTimerIdent = LOOPER_ID_USER + 1;

}  // namespace

RunLoop::LocalRunLoop& RunLoop::LocalRunLoop::get() {
    thread_local LocalRunLoop runLoop;
    return runLoop;
}

RunLoop::LocalRunLoop::LocalRunLoop() noexcept {
    wakeFd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
    timerFd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC | TFD_NONBLOCK);
}

RunLoop::LocalRunLoop::~LocalRunLoop() {
    android_app* app = detail::androidApp();
    if (app && app->looper && fdsAdded) {
        if (wakeFd >= 0) {
            ALooper_removeFd(app->looper, wakeFd);
        }
        if (timerFd >= 0) {
            ALooper_removeFd(app->looper, timerFd);
        }
    }
    if (wakeFd >= 0) {
        close(wakeFd);
    }
    if (timerFd >= 0) {
        close(timerFd);
    }
}

void RunLoop::LocalRunLoop::addLooperFds() {
    android_app* app = detail::androidApp();
    if (!app || !app->looper || fdsAdded) {
        return;
    }
    if (wakeFd >= 0) {
        ALooper_addFd(app->looper, wakeFd, kWakeIdent, ALOOPER_EVENT_INPUT, nullptr, nullptr);
    }
    if (timerFd >= 0) {
        ALooper_addFd(app->looper, timerFd, kTimerIdent, ALOOPER_EVENT_INPUT, nullptr, nullptr);
    }
    fdsAdded = true;
}

RunLoop::RunLoop() : local_(LocalRunLoop::get()) {}

void RunLoop::run() {
    android_app* app = detail::androidApp();
    if (!app) {
        return;
    }
    local_.addLooperFds();
    local_.running = true;
    while (local_.running.load(std::memory_order_relaxed) && !app->destroyRequested) {
        android_poll_source* source = nullptr;
        const int ident = ALooper_pollOnce(-1, nullptr, nullptr, reinterpret_cast<void**>(&source));
        if (ident == ALOOPER_POLL_ERROR) {
            break;
        }
        if (source) {
            source->process(app, source);
        }
        if (ident == kWakeIdent && local_.wakeFd >= 0) {
            uint64_t val = 0;
            read(local_.wakeFd, &val, sizeof(val));
        }
        if (ident == kTimerIdent && local_.timerFd >= 0) {
            uint64_t exp = 0;
            read(local_.timerFd, &exp, sizeof(exp));
        }

        if (local_.framePending.exchange(false)) {
            FrameCallback frame;
            {
                std::scoped_lock lock(local_.mutex);
                frame = local_.frameCallback;
            }
            if (frame) {
                frame();
            }
        }

        std::vector<ScheduledTask> tasks;
        std::vector<ScheduledTask> repeated;
        {
            std::scoped_lock lock(local_.mutex);
            std::move(local_.tasks.begin(), local_.tasks.end(), std::back_inserter(tasks));
            local_.tasks.clear();
        }
        for (auto& task : tasks) {
            TaskContext context;
            task(context);
            if (context.stopped_) {
                local_.running = false;
            }
            if (task.repeated && !context.cancelled_) {
                repeated.push_back(std::move(task));
            }
        }
        {
            std::scoped_lock lock(local_.mutex);
            std::move(repeated.begin(), repeated.end(), std::back_inserter(local_.tasks));
        }
    }
}

void RunLoop::wake() {
    if (local_.wakeFd >= 0) {
        uint64_t one = 1;
        write(local_.wakeFd, &one, sizeof(one));
    }
}

void RunLoop::stop() {
    local_.running = false;
    wake();
}

void RunLoop::scheduleTimer(std::chrono::duration<float> delay) {
    if (local_.timerFd < 0) {
        return;
    }
    local_.addLooperFds();
    itimerspec spec{};
    const auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(delay);
    spec.it_value.tv_sec = static_cast<time_t>(ns.count() / 1000000000);
    spec.it_value.tv_nsec = ns.count() % 1000000000;
    if (spec.it_value.tv_sec == 0 && spec.it_value.tv_nsec == 0) {
        spec.it_value.tv_nsec = 1;
    }
    timerfd_settime(local_.timerFd, 0, &spec, nullptr);
}

void RunLoop::scheduleTask(Task task) {
    {
        std::scoped_lock lock(local_.mutex);
        local_.tasks.push_back({std::move(task)});
    }
    wake();
}

void RunLoop::scheduleRepeatedTask(Task task) {
    {
        std::scoped_lock lock(local_.mutex);
        local_.tasks.push_back({std::move(task), true});
    }
    wake();
}

void RunLoop::scheduleFrameCallback(FrameCallback cb) {
    std::scoped_lock lock(local_.mutex);
    local_.frameCallback = std::move(cb);
}

void RunLoop::requestFrame() {
    local_.framePending = true;
    wake();
}

}  // namespace glim::shell
