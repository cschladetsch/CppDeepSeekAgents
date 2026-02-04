#include "CliOptions.hpp"

#include <cstdlib>
#include <sstream>

namespace app {

std::string Usage() {
  std::ostringstream out;
  out << "Usage:\n"
      << "  CppDeepSeek [options]\n\n"
      << "Options:\n"
      << "  --topic <text>     Debate topic (otherwise interactive CLI)\n"
      << "  --model <name>     Model name (default: deepseek-reasoner)\n"
      << "  --rounds <n>       Debate rounds (default: 1)\n"
      << "  --gpu-layers <n|auto>   Offload N layers to GPU (llama.cpp, default: 0)\n"
      << "  --stream           Enable streaming (default)\n"
      << "  --no-stream        Disable streaming\n"
      << "  --local-only       Do not use network; require local backend (default)\n"
      << "  --remote           Use DeepSeek API (requires key)\n"
      << "  --load <path>      Load agent memory from JSON\n"
      << "  --save <path>      Save agent memory to JSON\n"
      << "  --help             Show this help\n";
  return out.str();
}

std::optional<CliOptions> ParseCli(int argc, char** argv, std::string* error_out) {
  CliOptions opts;
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--help") {
      opts.help = true;
      return opts;
    }
    if (arg == "--stream") {
      opts.stream = true;
      continue;
    }
    if (arg == "--no-stream") {
      opts.stream = false;
      continue;
    }
    if (arg == "--local-only") {
      opts.local_only = true;
      continue;
    }
    if (arg == "--remote") {
      opts.local_only = false;
      continue;
    }
    if (arg == "--topic" || arg == "--model" || arg == "--rounds" || arg == "--gpu-layers" ||
        arg == "--n-gpu-layers" || arg == "--load" || arg == "--save") {
      if (i + 1 >= argc) {
        if (error_out) {
          *error_out = "Missing value for " + arg;
        }
        return std::nullopt;
      }
      std::string value = argv[++i];
      if (arg == "--topic") {
        opts.topic = value;
        opts.topic_set = true;
      } else if (arg == "--model") {
        opts.model = value;
      } else if (arg == "--gpu-layers" || arg == "--n-gpu-layers") {
        if (value == "auto") {
          opts.gpu_layers_auto = true;
          opts.gpu_layers = 0;
        } else {
          try {
            opts.gpu_layers = std::stoi(value);
          } catch (...) {
            if (error_out) {
              *error_out = "Invalid gpu-layers value: " + value;
            }
            return std::nullopt;
          }
          if (opts.gpu_layers < 0) {
            if (error_out) {
              *error_out = "gpu-layers must be >= 0";
            }
            return std::nullopt;
          }
        }
      } else if (arg == "--load") {
        opts.load_path = value;
      } else if (arg == "--save") {
        opts.save_path = value;
      } else {
        try {
          opts.rounds = std::stoi(value);
        } catch (...) {
          if (error_out) {
            *error_out = "Invalid rounds value: " + value;
          }
          return std::nullopt;
        }
        if (opts.rounds <= 0) {
          if (error_out) {
            *error_out = "Rounds must be > 0";
          }
          return std::nullopt;
        }
      }
      continue;
    }
    if (error_out) {
      *error_out = "Unknown option: " + arg;
    }
    return std::nullopt;
  }
  return opts;
}

}  // namespace app
