#include <glim/shell/RunLoop.h>

#include <iterator>

extern "C" void glimRunLoopTimerCallback(CFRunLoopTimerRef, void* info) {
    CFRunLoopStop(static_cast<CFRunLoopRef>(info));
}

namespace glim::shell {
namespace {

constexpr CFTimeInterval kDistantFuture = 1.0e10;
constexpr CFTimeInterval kFrameInterval = 1.0 / 60.0;

}  // namespace

RunLoop::LocalRunLoop& RunLoop::LocalRunLoop::get() {
    thread_local LocalRunLoop runLoop;
    return runLoop;
}

RunLoop::LocalRunLoop::LocalRunLoop() noexcept : runLoop(CFRunLoopGetCurrent()) {
    CFRunLoopTimerContext ctx{};
    ctx.version = 0;
    ctx.info = runLoop;
    timer = CFRunLoopTimerCreate(kCFAllocatorDefault, kDistantFuture, kDistantFuture, 0, 0,
                                 glimRunLoopTimerCallback, &ctx);
    CFRunLoopAddTimer(runLoop, timer, kCFRunLoopCommonModes);

    frameTimer = CFRunLoopTimerCreate(kCFAllocatorDefault, CFAbsoluteTimeGetCurrent() + kFrameInterval,
                                      kFrameInterval, 0, 0, glimRunLoopTimerCallback, &ctx);
    CFRunLoopAddTimer(runLoop, frameTimer, kCFRunLoopCommonModes);
}

RunLoop::LocalRunLoop::~LocalRunLoop() {
    CFRunLoopRemoveTimer(runLoop, timer, kCFRunLoopCommonModes);
    CFRunLoopRemoveTimer(runLoop, frameTimer, kCFRunLoopCommonModes);
    CFRelease(timer);
    CFRelease(frameTimer);
}

RunLoop::RunLoop() : local_(LocalRunLoop::get()) {}

void RunLoop::run() {
    local_.running = true;
    while (local_.running.load(std::memory_order_relaxed)) {
        const auto result = CFRunLoopRunInMode(kCFRunLoopDefaultMode, kDistantFuture, true);
        if (result == kCFRunLoopRunFinished) {
            break;
        }
        FrameCallback frame;
        {
            std::scoped_lock lock(local_.mutex);
            frame = local_.frameCallback;
        }
        if (frame) {
            frame();
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
    CFRunLoopStop(local_.runLoop);
}

void RunLoop::stop() {
    local_.running = false;
    CFRunLoopStop(local_.runLoop);
}

void RunLoop::scheduleTimer(std::chrono::duration<float> delay) {
    CFRunLoopTimerSetNextFireDate(local_.timer, CFAbsoluteTimeGetCurrent() + delay.count());
}

void RunLoop::scheduleTask(Task task) {
    std::scoped_lock lock(local_.mutex);
    local_.tasks.push_back({std::move(task)});
}

void RunLoop::scheduleRepeatedTask(Task task) {
    std::scoped_lock lock(local_.mutex);
    local_.tasks.push_back({std::move(task), true});
}

void RunLoop::scheduleFrameCallback(FrameCallback cb) {
    std::scoped_lock lock(local_.mutex);
    local_.frameCallback = std::move(cb);
}

}  // namespace glim::shell
