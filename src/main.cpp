#include "AgentRuntime.hpp"
#include "CliOptions.hpp"
#include "DeepSeekClient.hpp"
#include "LogicGate.hpp"
#include "LlamaBackend.hpp"
#include "ModelStore.hpp"
#include "rang.hpp"

#include <cstdlib>
#include <cctype>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#if !defined(_WIN32)
#include <unistd.h>
#endif

namespace {

uint64_t TotalSystemMemoryBytes() {
#if defined(_WIN32)
  return 0;
#else
  long pages = sysconf(_SC_PHYS_PAGES);
  long page_size = sysconf(_SC_PAGE_SIZE);
  if (pages <= 0 || page_size <= 0) {
    return 0;
  }
  return static_cast<uint64_t>(pages) * static_cast<uint64_t>(page_size);
#endif
}

int EstimateLayerCountFromModelSize(double model_gb) {
  if (model_gb <= 5.5) {
    return 32;
  }
  if (model_gb <= 9.5) {
    return 40;
  }
  if (model_gb <= 18.0) {
    return 60;
  }
  return 80;
}

int EstimateGpuLayersAuto(const std::string& model_path) {
  std::error_code ec;
  const uint64_t model_size = std::filesystem::file_size(model_path, ec);
  if (ec || model_size == 0) {
    return 0;
  }
  const uint64_t total_mem = TotalSystemMemoryBytes();
  if (total_mem == 0) {
    return 0;
  }

  const double model_gb = static_cast<double>(model_size) / (1024.0 * 1024.0 * 1024.0);
  const double mem_gb = static_cast<double>(total_mem) / (1024.0 * 1024.0 * 1024.0);

  // Keep generous headroom for OS + KV cache.
  const double usable_gb = mem_gb * 0.6;
  if (usable_gb < model_gb * 1.1) {
    return 0;
  }

  const int total_layers = EstimateLayerCountFromModelSize(model_gb);
  double fraction = usable_gb / (model_gb * 1.1);
  if (fraction > 1.0) {
    fraction = 1.0;
  }
  if (fraction < 0.1) {
    fraction = 0.1;
  }
  int layers = static_cast<int>(std::lround(total_layers * fraction));
  if (layers < 1) {
    layers = 1;
  }
  if (layers > total_layers) {
    layers = total_layers;
  }
  return layers;
}

bool ContainsToken(std::string_view text, std::string_view token) {
  auto it = std::search(
      text.begin(), text.end(),
      token.begin(), token.end(),
      [](char a, char b) { return std::tolower(static_cast<unsigned char>(a)) ==
                                  std::tolower(static_cast<unsigned char>(b)); });
  return it != text.end();
}

bool IsEngineeringTopic(std::string_view text) {
  return ContainsToken(text, "c++") || ContainsToken(text, "software") ||
         ContainsToken(text, "agent") || ContainsToken(text, "program");
}

}  // namespace

int main(int argc, char** argv) {
  std::string parse_error;
  auto options = app::ParseCli(argc, argv, &parse_error);
  if (!options) {
    std::cerr << parse_error << "\n\n" << app::Usage();
    return 1;
  }
  if (options->help) {
    std::cout << app::Usage();
    return 0;
  }

  app::ChatBackend backend;
  std::unique_ptr<deepseek::DeepSeekClient> client;
  std::unique_ptr<app::LlamaBackend> local_backend;
  int resolved_gpu_layers = options->gpu_layers;
  bool resolved_gpu_auto = options->gpu_layers_auto;
  if (options->local_only) {
    const std::string model_path =
        deepseek::ModelStore::ResolveModelPath("deepseek-r1") + "/model.gguf";
    if (options->gpu_layers_auto) {
      resolved_gpu_layers = EstimateGpuLayersAuto(model_path);
    }
    try {
      local_backend =
          std::make_unique<app::LlamaBackend>(model_path, 4096, 0, resolved_gpu_layers);
    } catch (const std::exception& ex) {
      std::cerr << rang::fg::red << "Failed to initialize local model: " << rang::fg::reset
                << ex.what() << "\n";
      std::cerr << "Expected model at: " << model_path << "\n";
      return 1;
    }
    backend = local_backend->Backend();
  } else {
    const char* api_key = std::getenv("DEEPSEEK_API_KEY");
    if (!api_key || std::string(api_key).empty()) {
      std::cerr << "Set DEEPSEEK_API_KEY in your environment.\n";
      return 1;
    }
    client = std::make_unique<deepseek::DeepSeekClient>(api_key, options->model);
    backend.chat = [&](const std::vector<deepseek::Message>& messages,
                       std::string_view system_prompt,
                       std::string* error_out) {
      return client->chat(messages, system_prompt, error_out);
    };
    backend.stream = [&](const std::vector<deepseek::Message>& messages,
                         std::string_view system_prompt,
                         const app::ChatBackend::StreamCallback& on_delta,
                         std::string* error_out) {
      return client->stream_chat(messages, system_prompt, on_delta, error_out);
    };
  }

  app::Agent researcher{
      "Researcher",
      "You are a research-oriented agent. Provide evidence, tradeoffs, and cite real engineering"
      " constraints. Be concise.",
      {}};
  app::Agent critic{
      "Critic",
      "You are a critical agent. Challenge assumptions, probe weaknesses, and seek counterexamples."
      " Be concise.",
      {}};

  std::vector<app::Agent> agents{researcher, critic};
  if (!options->load_path.empty()) {
    std::string load_error;
    if (!app::LoadAgents(&agents, options->load_path, &load_error)) {
      std::cerr << "Failed to load agents: " << load_error << "\n";
      return 1;
    }
  }

  const std::string topic = options->topic;
  if (options->topic_set) {
    std::cout << rang::style::bold << rang::fg::cyan << "Debate topic: " << rang::style::reset
              << topic << "\n";
  }
  std::cout << rang::fg::yellow << "Model: " << rang::fg::reset << options->model << "\n";
  std::cout << rang::fg::yellow << "Rounds: " << rang::fg::reset << options->rounds << "\n";
  std::cout << rang::fg::yellow << "Streaming: " << rang::fg::reset
            << (options->stream ? "on" : "off") << "\n";
  if (options->local_only) {
    std::cout << rang::fg::yellow << "GPU layers: " << rang::fg::reset;
    if (resolved_gpu_auto) {
      std::cout << "auto -> " << resolved_gpu_layers << "\n";
    } else {
      std::cout << resolved_gpu_layers << "\n";
    }
  }
  std::cout << "Model home (shared across projects): "
            << deepseek::ModelStore::ResolveModelHome() << "\n";
  std::cout << "Example model path (deepseek-r1): "
            << deepseek::ModelStore::ResolveModelPath("deepseek-r1") << "\n";
  if (!deepseek::ModelStore::ModelExists("deepseek-r1")) {
    std::cout << "Model not present. You can place it at: "
              << deepseek::ModelStore::ResolveModelPath("deepseek-r1") << "\n";
  }

  auto run_topic = [&](std::string_view t) -> bool {
    app::LogicGate gate("Allow only software engineering topics.");
    if (options->local_only) {
      // Local gate is deterministic for demo reliability.
      if (!IsEngineeringTopic(t)) {
        std::cerr << rang::fg::red << "Gate rejected the topic." << rang::fg::reset << "\n";
        return false;
      }
    } else {
      std::string gate_error;
      auto gate_result = gate.Evaluate(backend, t, false, &gate_error);
      if (!gate_result) {
        std::cerr << rang::fg::red << "Gate evaluation failed: " << rang::fg::reset << gate_error
                  << "\n";
        return false;
      }
      if (!gate_result->allow) {
        std::cerr << rang::fg::red << "Gate rejected the topic." << rang::fg::reset << "\n";
        return false;
      }
    }

    auto results = app::RunDebateRounds(backend, agents, t, options->rounds, options->stream);
    std::cout << "\n\n" << rang::style::bold << "--- Summary ---" << rang::style::reset << "\n";
    for (const auto& result : results) {
      if (result.name == "Researcher") {
        std::cout << rang::fg::blue << result.name << rang::fg::reset << ":\n";
      } else if (result.name == "Critic") {
        std::cout << rang::fg::magenta << result.name << rang::fg::reset << ":\n";
      } else {
        std::cout << rang::fg::green << result.name << rang::fg::reset << ":\n";
      }
      std::cout << result.response.content << "\n\n";
      std::cout << rang::fg::gray << "Press ENTER to continue..." << rang::fg::reset;
      std::cout.flush();
      std::string line;
      std::getline(std::cin, line);
    }
    return true;
  };

  try {
    if (options->topic_set) {
      if (!run_topic(topic)) {
        return 1;
      }
    } else {
      std::cout << rang::fg::cyan << "Interactive mode. Type a topic, or 'exit' to quit."
                << rang::fg::reset << "\n";
      while (true) {
        std::cout << rang::fg::green << "> " << rang::fg::reset;
        std::string line;
        if (!std::getline(std::cin, line)) {
          break;
        }
        if (line == "exit" || line == "quit") {
          break;
        }
        if (line.empty()) {
          continue;
        }
        if (!run_topic(line)) {
          // Keep the CLI running even if a gate rejects.
          continue;
        }
      }
    }

    if (!options->save_path.empty()) {
      std::string save_error;
      if (!app::SaveAgents(agents, options->save_path, &save_error)) {
        std::cerr << "Failed to save agents: " << save_error << "\n";
        return 1;
      }
    }
  } catch (const std::exception& ex) {
    std::cerr << rang::fg::red << "Error: " << rang::fg::reset << ex.what() << "\n";
    return 1;
  }

  return 0;
}
