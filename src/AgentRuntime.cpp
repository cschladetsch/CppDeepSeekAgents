#include "AgentRuntime.hpp"
#include "rang.hpp"

#include <nlohmann/json.hpp>

#include <fstream>
#include <iostream>
#include <stdexcept>

namespace app {

std::vector<deepseek::Message> BuildPrompt(const Agent& agent, std::string_view user_input) {
  std::vector<deepseek::Message> messages = agent.memory;
  messages.push_back({"user", std::string(user_input), ""});
  return messages;
}

AgentResult RunAgent(ChatBackend& backend,
                     Agent& agent,
                     std::string_view user_input,
                     bool stream,
                     std::mutex* print_mutex) {
  AgentResult result;
  result.name = agent.name;

  std::string error;
  auto messages = BuildPrompt(agent, user_input);

  if (stream) {
    std::string reasoning_accum;
    std::string content_accum;
    bool ok = backend.stream(
        messages, agent.system_prompt,
        [&](std::string_view reasoning_delta, std::string_view content_delta) {
          if (print_mutex) {
            std::lock_guard<std::mutex> lock(*print_mutex);
            if (!reasoning_delta.empty()) {
              std::cout << rang::fg::magenta << "[" << agent.name << "][Reasoning] "
                        << rang::fg::reset << reasoning_delta << std::flush;
            }
            if (!content_delta.empty()) {
              std::cout << rang::fg::cyan << "[" << agent.name << "] " << rang::fg::reset
                        << content_delta << std::flush;
            }
          }
          reasoning_accum.append(reasoning_delta);
          content_accum.append(content_delta);
        },
        &error);

    if (!ok) {
      throw std::runtime_error("Stream error (" + agent.name + "): " + error);
    }
    result.response.reasoning = std::move(reasoning_accum);
    result.response.content = std::move(content_accum);
  } else {
    auto response = backend.chat(messages, agent.system_prompt, &error);
    if (!response) {
      throw std::runtime_error("Request error (" + agent.name + "): " + error);
    }
    result.response = *response;
  }

  agent.memory.push_back({"assistant", result.response.content, result.response.reasoning});
  return result;
}

std::vector<AgentResult> RunAgentsConcurrent(ChatBackend& backend,
                                             std::vector<Agent>& agents,
                                             std::string_view user_input,
                                             bool stream) {
  std::mutex print_mutex;
  std::vector<std::future<AgentResult>> futures;
  futures.reserve(agents.size());

  for (auto& agent : agents) {
    Agent* agent_ptr = &agent;
    futures.push_back(
        std::async(std::launch::async, [&backend, agent_ptr, user_input, stream, &print_mutex]() {
          return RunAgent(backend, *agent_ptr, user_input, stream, &print_mutex);
        }));
  }

  std::vector<AgentResult> results;
  results.reserve(futures.size());
  for (auto& fut : futures) {
    results.push_back(fut.get());
  }
  return results;
}

std::vector<AgentResult> RunDebateRounds(ChatBackend& backend,
                                         std::vector<Agent>& agents,
                                         std::string_view topic,
                                         int rounds,
                                         bool stream) {
  if (rounds <= 0) {
    return {};
  }
  std::vector<AgentResult> all_results;
  all_results.reserve(static_cast<size_t>(rounds) * agents.size());

  std::string current_prompt = std::string(topic);
  for (int r = 0; r < rounds; ++r) {
    for (auto& agent : agents) {
      auto result = RunAgent(backend, agent, current_prompt, stream, nullptr);
      // Feed the previous response into the next agent for a simple debate loop.
      current_prompt = result.response.content;
      all_results.push_back(std::move(result));
    }
  }
  return all_results;
}

bool SaveAgents(const std::vector<Agent>& agents,
                std::string_view path,
                std::string* error_out) {
  nlohmann::json root = nlohmann::json::array();
  for (const auto& agent : agents) {
    nlohmann::json a;
    a["name"] = agent.name;
    a["system_prompt"] = agent.system_prompt;
    a["memory"] = nlohmann::json::array();
    for (const auto& msg : agent.memory) {
      a["memory"].push_back({{"role", msg.role},
                             {"content", msg.content},
                             {"reasoning", msg.reasoning}});
    }
    root.push_back(std::move(a));
  }

  std::ofstream out(std::string(path), std::ios::binary);
  if (!out) {
    if (error_out) {
      *error_out = "Failed to open file for write: " + std::string(path);
    }
    return false;
  }
  out << root.dump(2);
  if (!out) {
    if (error_out) {
      *error_out = "Failed to write file: " + std::string(path);
    }
    return false;
  }
  return true;
}

bool LoadAgents(std::vector<Agent>* agents,
                std::string_view path,
                std::string* error_out) {
  if (!agents) {
    if (error_out) {
      *error_out = "Agents output pointer is null.";
    }
    return false;
  }
  std::ifstream in(std::string(path), std::ios::binary);
  if (!in) {
    if (error_out) {
      *error_out = "Failed to open file for read: " + std::string(path);
    }
    return false;
  }

  nlohmann::json root;
  try {
    in >> root;
  } catch (const std::exception& ex) {
    if (error_out) {
      *error_out = std::string("Invalid JSON: ") + ex.what();
    }
    return false;
  }

  if (!root.is_array()) {
    if (error_out) {
      *error_out = "Invalid JSON: root is not an array.";
    }
    return false;
  }

  std::vector<Agent> loaded;
  for (const auto& a : root) {
    Agent agent;
    agent.name = a.value("name", "");
    agent.system_prompt = a.value("system_prompt", "");
    for (const auto& msg : a.value("memory", nlohmann::json::array())) {
      deepseek::Message m;
      m.role = msg.value("role", "");
      m.content = msg.value("content", "");
      m.reasoning = msg.value("reasoning", "");
      agent.memory.push_back(std::move(m));
    }
    loaded.push_back(std::move(agent));
  }

  *agents = std::move(loaded);
  return true;
}

}  // namespace app
