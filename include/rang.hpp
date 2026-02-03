#pragma once

namespace rang {

namespace fg {
inline constexpr const char* red = "\033[31m";
inline constexpr const char* green = "\033[32m";
inline constexpr const char* yellow = "\033[33m";
inline constexpr const char* blue = "\033[34m";
inline constexpr const char* magenta = "\033[35m";
inline constexpr const char* cyan = "\033[36m";
inline constexpr const char* gray = "\033[90m";
inline constexpr const char* reset = "\033[39m";
}  // namespace fg

namespace style {
inline constexpr const char* bold = "\033[1m";
inline constexpr const char* reset = "\033[0m";
}  // namespace style

}  // namespace rang
