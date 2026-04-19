#pragma once

#include <string_view>

namespace CoreEngine {

struct WindowDesc {
  int width = 1280;
  int height = 720;
  std::string_view title = "CoreEngine";
  bool resizable = true;
  bool highDpi = true;
};

} // namespace CoreEngine
