#include <glim/shell/RunLoop.h>

namespace glim::shell {

RunLoop::LocalRunLoop& RunLoop::LocalRunLoop::get() {
    thread_local LocalRunLoop loop;
    return loop;
}

RunLoop::LocalRunLoop::LocalRunLoop() noexcept = default;
RunLoop::LocalRunLoop::~LocalRunLoop() = default;

RunLoop::RunLoop() : local_(LocalRunLoop::get()) {}

void RunLoop::run() {
#if defined(__EMSCRIPTEN__)
    local_.running = true;
#else
    local_.running = true;
    TaskContext context;
    std::vector<ScheduledTask> tasks;
    {
        std::lock_guard<std::mutex> lock(local_.mutex);
        tasks = local_.tasks;
    }
    for (auto& task : tasks) {
        task(context);
        if (context.stopped_) {
            break;
        }
    }
    local_.running = false;
#endif
}

void RunLoop::wake() {
    local_.framePending = true;
}

void RunLoop::stop() {
    local_.running = false;
}

void RunLoop::scheduleTimer(std::chrono::duration<float>) {}

void RunLoop::scheduleTask(Task task) {
    std::lock_guard<std::mutex> lock(local_.mutex);
    local_.tasks.push_back(ScheduledTask{std::move(task), false});
}

void RunLoop::scheduleRepeatedTask(Task task) {
    std::lock_guard<std::mutex> lock(local_.mutex);
    local_.tasks.push_back(ScheduledTask{std::move(task), true});
}

void RunLoop::scheduleFrameCallback(FrameCallback callback) {
    std::lock_guard<std::mutex> lock(local_.mutex);
    local_.frameCallback = std::move(callback);
}

void RunLoop::requestFrame() {
    local_.framePending = true;
}

}  // namespace glim::shell
