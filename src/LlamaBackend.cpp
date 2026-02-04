#include "LlamaBackend.hpp"

#include <llama.h>

#include <algorithm>
#include <thread>
#include <stdexcept>
#include <string>
#include <vector>

namespace app {
namespace {

std::string BuildPrompt(const std::vector<deepseek::Message>& messages,
                        std::string_view system_prompt) {
  std::string prompt;
  prompt.reserve(1024);
  // Minimal role-tagged prompt. Keep it predictable for local inference.
  prompt.append("System: ").append(system_prompt).append("\n");
  for (const auto& msg : messages) {
    if (msg.role == "user") {
      prompt.append("User: ").append(msg.content).append("\n");
    } else if (msg.role == "assistant") {
      prompt.append("Assistant: ").append(msg.content).append("\n");
    } else {
      prompt.append("Message: ").append(msg.content).append("\n");
    }
  }
  prompt.append("Assistant:");
  return prompt;
}

std::string Generate(llama_context* ctx,
                     llama_model* model,
                     llama_sampler* sampler,
                     std::string_view prompt,
                     int max_tokens) {
  std::vector<llama_token> tokens(prompt.size() + max_tokens + 4);
  const llama_vocab* vocab = llama_model_get_vocab(model);
  int n_tokens = llama_tokenize(vocab,
                                prompt.data(),
                                (int)prompt.size(),
                                tokens.data(),
                                (int)tokens.size(),
                                true,
                                true);
  if (n_tokens < 0) {
    throw std::runtime_error("Failed to tokenize prompt.");
  }
  tokens.resize(n_tokens);

  llama_batch batch = llama_batch_get_one(tokens.data(), n_tokens);
  if (llama_decode(ctx, batch) != 0) {
    throw std::runtime_error("Failed to decode prompt.");
  }

  std::string output;
  output.reserve(max_tokens * 4);
  for (int i = 0; i < max_tokens; ++i) {
    llama_token id = llama_sampler_sample(sampler, ctx, -1);
    if (id == llama_vocab_eos(vocab)) {
      break;
    }
    llama_sampler_accept(sampler, id);

    char buf[128];
    const int n = llama_token_to_piece(vocab, id, buf, sizeof(buf), 0, true);
    if (n > 0) {
      output.append(buf, buf + n);
    }

    llama_token next_tokens[1] = {id};
    llama_batch batch_next = llama_batch_get_one(next_tokens, 1);
    if (llama_decode(ctx, batch_next) != 0) {
      break;
    }
  }
  return output;
}

}  // namespace

LlamaBackend::LlamaBackend(std::string model_path,
                           int n_ctx,
                           int n_threads,
                           int n_gpu_layers)
    : model_path_(std::move(model_path)),
      n_ctx_(n_ctx),
      n_threads_(n_threads),
      n_gpu_layers_(n_gpu_layers) {
  // Silence llama.cpp logs to keep demo output readable.
  llama_log_set([](ggml_log_level, const char*, void*) {}, nullptr);
  llama_backend_init();

  llama_model_params mparams = llama_model_default_params();
  mparams.n_gpu_layers = n_gpu_layers_;
  model_ = llama_model_load_from_file(model_path_.c_str(), mparams);
  if (!model_) {
    throw std::runtime_error("Failed to load model: " + model_path_);
  }

  llama_context_params cparams = llama_context_default_params();
  cparams.n_ctx = n_ctx_;
  cparams.n_threads = n_threads_ > 0 ? n_threads_ : (int)std::thread::hardware_concurrency();
  cparams.n_threads_batch = cparams.n_threads;
  ctx_ = llama_init_from_model(model_, cparams);
  if (!ctx_) {
    throw std::runtime_error("Failed to create llama context.");
  }

  sampler_ = llama_sampler_init_greedy();
  if (!sampler_) {
    throw std::runtime_error("Failed to create sampler.");
  }
}

LlamaBackend::~LlamaBackend() {
  if (sampler_) {
    llama_sampler_free(sampler_);
  }
  if (ctx_) {
    llama_free(ctx_);
  }
  if (model_) {
    llama_model_free(model_);
  }
  llama_backend_free();
}

ChatBackend LlamaBackend::Backend() {
  ChatBackend backend;
  backend.chat = [this](const std::vector<deepseek::Message>& messages,
                        std::string_view system_prompt,
                        std::string* /*error_out*/) -> std::optional<deepseek::ChatResponse> {
    deepseek::ChatResponse resp;
    std::string prompt = BuildPrompt(messages, system_prompt);
    resp.content = Generate(ctx_, model_, sampler_, prompt, 256);
    return resp;
  };
  backend.stream = [this](const std::vector<deepseek::Message>& messages,
                          std::string_view system_prompt,
                          const ChatBackend::StreamCallback& on_delta,
                          std::string* /*error_out*/) {
    std::string prompt = BuildPrompt(messages, system_prompt);
    std::string text = Generate(ctx_, model_, sampler_, prompt, 256);
    const size_t chunk = 16;
    for (size_t i = 0; i < text.size(); i += chunk) {
      on_delta("", std::string_view(text.data() + i, std::min(chunk, text.size() - i)));
    }
    return true;
  };
  return backend;
}

}  // namespace app
