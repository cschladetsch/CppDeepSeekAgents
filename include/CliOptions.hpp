#pragma once

#include <optional>
#include <string>

namespace app {

struct CliOptions {
  std::string topic;
  bool topic_set = false;
  std::string model = "deepseek-reasoner";
  int rounds = 1;
  bool stream = true;
  bool help = false;
  bool local_only = true;
  int gpu_layers = 0;
  bool gpu_layers_auto = false;
  std::string load_path;
  std::string save_path;
};

std::string Usage();

std::optional<CliOptions> ParseCli(int argc, char** argv, std::string* error_out = nullptr);

}  // namespace app
