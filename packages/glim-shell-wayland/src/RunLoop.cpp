#include <glim/shell/RunLoop.h>

#include "Display.h"

#include <iterator>
#include <poll.h>
#include <sys/eventfd.h>
#include <sys/timerfd.h>
#include <unistd.h>

#include <cerrno>
#include <cstdint>

namespace glim::shell {

RunLoop::LocalRunLoop& RunLoop::LocalRunLoop::get() {
    thread_local LocalRunLoop runLoop;
    return runLoop;
}

RunLoop::LocalRunLoop::LocalRunLoop() noexcept {
    wakeFd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
    timerFd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC | TFD_NONBLOCK);
}

RunLoop::LocalRunLoop::~LocalRunLoop() {
    if (wakeFd >= 0) {
        close(wakeFd);
    }
    if (timerFd >= 0) {
        close(timerFd);
    }
}

RunLoop::RunLoop() : local_(LocalRunLoop::get()) {}

void RunLoop::run() {
    local_.running = true;
    while (local_.running.load(std::memory_order_relaxed)) {
        detail::waylandDispatchPending();
        detail::waylandFlush();

        pollfd fds[3]{};
        nfds_t n = 0;
        const int displayFd = detail::waylandFd();
        int displayIndex = -1;
        bool prepared = false;
        if (displayFd >= 0) {
            prepared = detail::waylandPrepareRead();
            if (!prepared) {
                continue;
            }
            displayIndex = static_cast<int>(n);
            fds[n].fd = displayFd;
            fds[n].events = POLLIN;
            ++n;
        }
        if (local_.wakeFd >= 0) {
            fds[n].fd = local_.wakeFd;
            fds[n].events = POLLIN;
            ++n;
        }
        if (local_.timerFd >= 0) {
            fds[n].fd = local_.timerFd;
            fds[n].events = POLLIN;
            ++n;
        }

        const int pr = poll(fds, n, -1);
        if (pr < 0) {
            if (errno == EINTR) {
                if (prepared) {
                    detail::waylandCancelRead();
                }
                continue;
            }
            break;
        }

        bool displayReadable = false;
        if (displayIndex >= 0 && (fds[displayIndex].revents & (POLLIN | POLLERR | POLLHUP))) {
            displayReadable = true;
        }
        if (prepared) {
            if (displayReadable) {
                detail::waylandReadEvents();
            } else {
                detail::waylandCancelRead();
            }
        }
        detail::waylandDispatchPending();

        if (local_.wakeFd >= 0) {
            uint64_t val = 0;
            read(local_.wakeFd, &val, sizeof(val));
        }
        if (local_.timerFd >= 0) {
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
