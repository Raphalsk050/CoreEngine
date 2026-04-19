// core/window/window_system.h
#pragma once

#include <memory>

#include "core/window/i_window_backend.h"
#include "core/window/window_event_queue.h"

namespace CoreEngine {

class WindowSystem final {
public:
  explicit WindowSystem(std::unique_ptr<IWindowBackend> backend);

  [[nodiscard]] bool Initialize(const WindowDesc &desc);
  void PollEvents();
  void Shutdown();

  [[nodiscard]] const WindowEventQueue &Events() const;
  [[nodiscard]] bool ShouldClose() const;

private:
  std::unique_ptr<IWindowBackend> backend_;
  WindowEventQueue events_;
};

} // namespace CoreEngine
