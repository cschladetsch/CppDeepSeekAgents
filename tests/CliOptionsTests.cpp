#include "CliOptions.hpp"

#include <gtest/gtest.h>

TEST(CliOptionsTests, Defaults) {
  const char* argv[] = {"CppDeepSeek"};
  int argc = 1;
  std::string error;
  auto opts = app::ParseCli(argc, const_cast<char**>(argv), &error);
  ASSERT_TRUE(opts.has_value());
  EXPECT_EQ(opts->rounds, 1);
  EXPECT_TRUE(opts->stream);
  EXPECT_FALSE(opts->help);
  EXPECT_TRUE(opts->local_only);
  EXPECT_FALSE(opts->topic_set);
  EXPECT_EQ(opts->gpu_layers, 0);
  EXPECT_FALSE(opts->gpu_layers_auto);
}

TEST(CliOptionsTests, ParsesValues) {
  const char* argv[] = {"CppDeepSeek", "--topic", "T", "--model", "M", "--rounds", "3",
                        "--gpu-layers", "12", "--no-stream", "--load", "in.json", "--save",
                        "out.json"};
  int argc = 14;
  std::string error;
  auto opts = app::ParseCli(argc, const_cast<char**>(argv), &error);
  ASSERT_TRUE(opts.has_value());
  EXPECT_EQ(opts->topic, "T");
  EXPECT_EQ(opts->model, "M");
  EXPECT_EQ(opts->rounds, 3);
  EXPECT_EQ(opts->gpu_layers, 12);
  EXPECT_FALSE(opts->gpu_layers_auto);
  EXPECT_FALSE(opts->stream);
  EXPECT_EQ(opts->load_path, "in.json");
  EXPECT_EQ(opts->save_path, "out.json");
}

TEST(CliOptionsTests, RejectsInvalidRounds) {
  const char* argv[] = {"CppDeepSeek", "--rounds", "0"};
  int argc = 3;
  std::string error;
  auto opts = app::ParseCli(argc, const_cast<char**>(argv), &error);
  EXPECT_FALSE(opts.has_value());
  EXPECT_FALSE(error.empty());
}

TEST(CliOptionsTests, RemoteFlagDisablesLocalOnly) {
  const char* argv[] = {"CppDeepSeek", "--remote"};
  int argc = 2;
  std::string error;
  auto opts = app::ParseCli(argc, const_cast<char**>(argv), &error);
  ASSERT_TRUE(opts.has_value());
  EXPECT_FALSE(opts->local_only);
}

TEST(CliOptionsTests, RejectsInvalidGpuLayers) {
  const char* argv[] = {"CppDeepSeek", "--gpu-layers", "-1"};
  int argc = 3;
  std::string error;
  auto opts = app::ParseCli(argc, const_cast<char**>(argv), &error);
  EXPECT_FALSE(opts.has_value());
  EXPECT_FALSE(error.empty());
}

TEST(CliOptionsTests, ParsesAutoGpuLayers) {
  const char* argv[] = {"CppDeepSeek", "--gpu-layers", "auto"};
  int argc = 3;
  std::string error;
  auto opts = app::ParseCli(argc, const_cast<char**>(argv), &error);
  ASSERT_TRUE(opts.has_value());
  EXPECT_TRUE(opts->gpu_layers_auto);
  EXPECT_EQ(opts->gpu_layers, 0);
}
