#pragma once

#include "AgentRuntime.hpp"

#include <memory>
#include <string>

struct llama_context;
struct llama_model;
struct llama_sampler;

namespace app {

class LlamaBackend {
 public:
  LlamaBackend(std::string model_path,
               int n_ctx = 4096,
               int n_threads = 0,
               int n_gpu_layers = 0);
  ~LlamaBackend();

  ChatBackend Backend();

 private:
  std::string model_path_;
  int n_ctx_;
  int n_threads_;
  int n_gpu_layers_;
  llama_model* model_ = nullptr;
  llama_context* ctx_ = nullptr;
  llama_sampler* sampler_ = nullptr;
};

}  // namespace app
