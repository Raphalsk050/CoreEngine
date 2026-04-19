#pragma once

#include "core/window/window_desc.h"

namespace CoreEngine {

class WindowEventQueue;

class IWindowBackend {
public:
  virtual ~IWindowBackend() = default;

  [[nodiscard]] virtual bool Initialize(const WindowDesc &desc) = 0;
  virtual void PollEvents(WindowEventQueue &queue) = 0;
  virtual void Shutdown() = 0;

  [[nodiscard]] virtual bool ShouldClose() const = 0;
};

} // namespace CoreEngine
