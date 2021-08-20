#pragma once

#include <functional>
#include <memory>
#include <span>
#include <string_view>

namespace dpcpp_trace {
namespace detail {
class NativeTracerImpl;
}

class OpenHandler {
public:
  virtual void replaceFilename(std::string_view newFile) const = 0;
};

class StatHandler {
public:
  virtual void replaceFilename(std::string_view newFile) const = 0;
};

class Tracer {
public:
  using onFileOpenHandler =
      std::function<void(std::string_view, const OpenHandler &)>;
  using onStatHandler =
      std::function<void(std::string_view, const StatHandler &)>;

  virtual void launch(std::string_view executable, std::span<std::string> args,
                      std::span<std::string> env) = 0;

  virtual void onFileOpen(onFileOpenHandler) = 0;
  virtual void onStat(onStatHandler) = 0;

  virtual ~Tracer() = default;

  virtual int wait() = 0;
  virtual void kill() = 0;
  virtual void interrupt() = 0;
};

class NativeTracer : public Tracer {
public:
  using onFileOpenHandler = Tracer::onFileOpenHandler;
  using onStatHandler = Tracer::onStatHandler;
  NativeTracer();

  void launch(std::string_view executable, std::span<std::string> args,
              std::span<std::string> env) final;
  void fork(std::function<void()> child, bool initialSuspend = true);

  void onFileOpen(onFileOpenHandler) final;
  void onStat(onStatHandler) final;

  int wait() final;
  void kill() final;
  void interrupt() final;

private:
  std::shared_ptr<detail::NativeTracerImpl> mImpl;
};
} // namespace dpcpp_trace
