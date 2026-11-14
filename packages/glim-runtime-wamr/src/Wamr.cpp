#include <glim/runtime_wamr/Wamr.h>

#include <wasm_export.h>

#include <cstring>
#include <exception>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

namespace glim::runtime_wamr {

namespace {

constexpr std::uint32_t kErrorSize = 256;

wasm_val_t i32(std::uint32_t value) {
  return {.kind = WASM_I32, ._paddings = {}, .of = {.i32 = static_cast<std::int32_t>(value)}};
}

wasm_val_t i64(std::uint64_t value) {
  return {.kind = WASM_I64, ._paddings = {}, .of = {.i64 = static_cast<std::int64_t>(value)}};
}

[[noreturn]] void fail(std::string_view what) {
  throw std::runtime_error("glim::runtime_wamr: " + std::string(what));
}

// Host faults become guest traps; C++ exceptions never unwind through the
// interpreter.
template <class Body>
auto withHost(wasm_exec_env_t env, Body body) -> decltype(body(std::declval<transport::Imports&>())
                                              ) {
  const auto instance = wasm_runtime_get_module_inst(env);
  auto* host = static_cast<transport::Imports*>(wasm_runtime_get_custom_data(instance));
  if (!host) {
    wasm_runtime_set_exception(instance, "glim: host import before attach");
    return {};
  }
  try {
    return body(*host);
  } catch (const std::exception& error) {
    wasm_runtime_set_exception(instance, error.what());
    return {};
  }
}

void hostSend(wasm_exec_env_t env) {
  withHost(env, [](transport::Imports& host) {
    host.send();
    return std::monostate{};
  });
}

std::uint64_t hostNow(wasm_exec_env_t env) {
  return withHost(env, [](transport::Imports& host) { return host.now(); });
}

void hostLog(wasm_exec_env_t env, std::uint32_t level, const char* text, std::uint32_t length) {
  withHost(env, [&](transport::Imports& host) {
    host.log(level, {text, length});
    return std::monostate{};
  });
}

// The WASI calls wasi-libc links into every guest, answered from the same
// Imports: stdout and stderr go to the log, every clock is host_now, and no
// other descriptor exists.
constexpr std::uint32_t kWasiSuccess = 0;
constexpr std::uint32_t kWasiBadDescriptor = 8;
constexpr std::uint32_t kWasiFault = 21;

std::byte* guestBytes(wasm_exec_env_t env, std::uint32_t offset, std::uint64_t size) {
  const auto instance = wasm_runtime_get_module_inst(env);
  if (!wasm_runtime_validate_app_addr(instance, offset, size)) {
    return nullptr;
  }
  return static_cast<std::byte*>(wasm_runtime_addr_app_to_native(instance, offset));
}

std::uint32_t wasiFdWrite(
  wasm_exec_env_t env,
  std::uint32_t fd,
  std::uint32_t iovs,
  std::uint32_t count,
  std::uint32_t written
) {
  if (fd != 1 && fd != 2) {
    return kWasiBadDescriptor;
  }
  auto* vectors = guestBytes(env, iovs, std::uint64_t{count} * 8);
  auto* total = guestBytes(env, written, 4);
  if (!vectors || !total) {
    return kWasiFault;
  }
  return withHost(env, [&](transport::Imports& host) {
    std::uint32_t sum = 0;
    for (std::uint32_t i = 0; i < count; ++i) {
      std::uint32_t at[2];
      std::memcpy(at, vectors + i * 8, sizeof(at));
      const auto* text = guestBytes(env, at[0], at[1]);
      if (!text) {
        return kWasiFault;
      }
      host.log(
        fd == 2 ? transport::kLogError : transport::kLogInfo,
        {reinterpret_cast<const char*>(text), at[1]}
      );
      sum += at[1];
    }
    std::memcpy(total, &sum, sizeof(sum));
    return kWasiSuccess;
  });
}

std::uint32_t wasiClockTimeGet(
  wasm_exec_env_t env, std::uint32_t, std::uint64_t, std::uint32_t out
) {
  auto* time = guestBytes(env, out, 8);
  if (!time) {
    return kWasiFault;
  }
  return withHost(env, [&](transport::Imports& host) {
    const auto now = host.now();
    std::memcpy(time, &now, sizeof(now));
    return kWasiSuccess;
  });
}

std::uint32_t wasiFdSeek(
  wasm_exec_env_t, std::uint32_t, std::int64_t, std::uint32_t, std::uint32_t
) {
  return kWasiBadDescriptor;
}

std::uint32_t wasiFdClose(wasm_exec_env_t, std::uint32_t) { return kWasiBadDescriptor; }

NativeSymbol kImports[] = {
  {.symbol = "host_send",
   .func_ptr = reinterpret_cast<void*>(hostSend),
   .signature = "()",
   .attachment = nullptr},
  {.symbol = "host_now",
   .func_ptr = reinterpret_cast<void*>(hostNow),
   .signature = "()I",
   .attachment = nullptr},
  {.symbol = "host_log",
   .func_ptr = reinterpret_cast<void*>(hostLog),
   .signature = "(i*~)",
   .attachment = nullptr},
};

NativeSymbol kWasi[] = {
  {.symbol = "fd_write",
   .func_ptr = reinterpret_cast<void*>(wasiFdWrite),
   .signature = "(iiii)i",
   .attachment = nullptr},
  {.symbol = "clock_time_get",
   .func_ptr = reinterpret_cast<void*>(wasiClockTimeGet),
   .signature = "(iIi)i",
   .attachment = nullptr},
  {.symbol = "fd_seek",
   .func_ptr = reinterpret_cast<void*>(wasiFdSeek),
   .signature = "(iIii)i",
   .attachment = nullptr},
  {.symbol = "fd_close",
   .func_ptr = reinterpret_cast<void*>(wasiFdClose),
   .signature = "(i)i",
   .attachment = nullptr},
};

// A guest may import only what the Engine registered; anything else would
// trap on first call, so it is refused at load instead.
void requireLinkedImports(wasm_module_t module) {
  const auto count = wasm_runtime_get_import_count(module);
  for (std::int32_t i = 0; i < count; ++i) {
    wasm_import_t import;
    wasm_runtime_get_import_type(module, i, &import);
    if (import.kind == WASM_IMPORT_EXPORT_KIND_FUNC &&
        !wasm_runtime_is_import_func_linked(import.module_name, import.name)) {
      fail(
        std::string("guest imports ") + import.module_name + "." + import.name +
        " outside the guest ABI"
      );
    }
  }
}

}  // namespace

Engine::Engine() {
  RuntimeInitArgs args;
  std::memset(&args, 0, sizeof(args));
  args.mem_alloc_type = Alloc_With_System_Allocator;
  if (!wasm_runtime_full_init(&args)) {
    fail("runtime init failed");
  }
  if (!wasm_runtime_register_natives("glim", kImports, std::size(kImports)) ||
      !wasm_runtime_register_natives("wasi_snapshot_preview1", kWasi, std::size(kWasi))) {
    wasm_runtime_destroy();
    fail("registering host imports failed");
  }
}

Engine::~Engine() { wasm_runtime_destroy(); }

struct Instance::Vm {
  wasm_module_t module = nullptr;
  wasm_module_inst_t instance = nullptr;
  wasm_exec_env_t env = nullptr;
  wasm_function_inst_t init = nullptr;
  wasm_function_inst_t recv = nullptr;
  wasm_function_inst_t tick = nullptr;

  ~Vm() {
    if (env) {
      wasm_runtime_destroy_exec_env(env);
    }
    if (instance) {
      wasm_runtime_deinstantiate(instance);
    }
    if (module) {
      wasm_runtime_unload(module);
    }
  }

  wasm_function_inst_t exported(const char* name) const {
    auto* function = wasm_runtime_lookup_function(instance, name);
    if (!function) {
      fail(std::string("guest does not export ") + name);
    }
    return function;
  }

  void call(
    wasm_function_inst_t function, std::span<wasm_val_t> args, std::span<wasm_val_t> results
  ) {
    if (!wasm_runtime_call_wasm_a(
          env,
          function,
          static_cast<std::uint32_t>(results.size()),
          results.data(),
          static_cast<std::uint32_t>(args.size()),
          args.data()
        )) {
      const std::string trap = wasm_runtime_get_exception(instance);
      wasm_runtime_clear_exception(instance);
      fail("guest trapped: " + trap);
    }
  }
};

Instance::Instance(Engine&, std::span<const std::byte> module, Limits limits)
    : module_(
        reinterpret_cast<const std::uint8_t*>(module.data()),
        reinterpret_cast<const std::uint8_t*>(module.data()) + module.size()
      ),
      vm_(std::make_unique<Vm>()) {
  char error[kErrorSize] = {};
  vm_->module = wasm_runtime_load(
    module_.data(), static_cast<std::uint32_t>(module_.size()), error, kErrorSize
  );
  if (!vm_->module) {
    fail(error);
  }
  requireLinkedImports(vm_->module);
  vm_->instance = wasm_runtime_instantiate(vm_->module, limits.stack, 0, error, kErrorSize);
  if (!vm_->instance) {
    fail(error);
  }
  vm_->env = wasm_runtime_create_exec_env(vm_->instance, limits.stack);
  if (!vm_->env) {
    fail("exec env creation failed");
  }
  if (auto* initialize = wasm_runtime_lookup_function(vm_->instance, "_initialize")) {
    vm_->call(initialize, {}, {});
  }
  vm_->init = vm_->exported("glim_init");
  vm_->recv = vm_->exported("glim_recv");
  vm_->tick = vm_->exported("glim_tick");
}

Instance::~Instance() = default;

void Instance::attach(transport::Imports& host) {
  wasm_runtime_set_custom_data(vm_->instance, &host);
}

std::span<std::byte> Instance::memory() {
  auto* memory = wasm_runtime_get_default_memory(vm_->instance);
  if (!memory) {
    return {};
  }
  return {
    static_cast<std::byte*>(wasm_memory_get_base_address(memory)),
    static_cast<std::size_t>(wasm_memory_get_cur_page_count(memory)) *
      wasm_memory_get_bytes_per_page(memory)
  };
}

std::uint32_t Instance::init(std::uint32_t protocol, std::uint32_t capacity) {
  wasm_val_t args[] = {i32(protocol), i32(capacity)};
  wasm_val_t result[] = {i32(0)};
  vm_->call(vm_->init, args, result);
  return static_cast<std::uint32_t>(result[0].of.i32);
}

void Instance::recv() { vm_->call(vm_->recv, {}, {}); }

void Instance::tick(std::uint64_t now) {
  wasm_val_t args[] = {i64(now)};
  vm_->call(vm_->tick, args, {});
}

}  // namespace glim::runtime_wamr
