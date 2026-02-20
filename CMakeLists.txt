cmake_minimum_required(VERSION 3.20)
project(CppDeepSeek LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(CURL REQUIRED)

option(CPPDEEPSEEK_ALLOW_FETCHCONTENT "Allow downloading dependencies via FetchContent" OFF)
find_package(nlohmann_json CONFIG QUIET)
if (NOT nlohmann_json_FOUND)
  if (CPPDEEPSEEK_ALLOW_FETCHCONTENT)
    include(FetchContent)
    FetchContent_Declare(
      nlohmann_json
      URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
      DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(nlohmann_json)
  else()
    message(FATAL_ERROR "nlohmann_json not found. Install nlohmann-json3-dev or enable CPPDEEPSEEK_ALLOW_FETCHCONTENT.")
  endif()
endif()


set(MODELSTORE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(MODELSTORE_ALLOW_FETCHCONTENT OFF CACHE BOOL "" FORCE)
add_subdirectory(modelstore)

option(CPPDEEPSEEK_BUILD_APP "Build the CppDeepSeek demo application" ON)
option(CPPDEEPSEEK_USE_LLAMA "Use llama.cpp for local backend" ON)
option(CPPDEEPSEEK_ALLOW_FETCHCONTENT_LLAMA "Allow downloading llama.cpp via FetchContent" ON)
set(LLAMA_CPP_DIR "" CACHE PATH "Path to llama.cpp (if not vendored)")

if (CPPDEEPSEEK_USE_LLAMA)
  if (LLAMA_CPP_DIR)
    add_subdirectory(${LLAMA_CPP_DIR} ${CMAKE_CURRENT_BINARY_DIR}/llama.cpp)
  elseif (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/third_party/llama.cpp/CMakeLists.txt)
    add_subdirectory(third_party/llama.cpp)
  elseif (CPPDEEPSEEK_ALLOW_FETCHCONTENT_LLAMA)
    include(FetchContent)
    FetchContent_Declare(
      llama_cpp
      GIT_REPOSITORY https://github.com/ggml-org/llama.cpp.git
      GIT_TAG b7472
      DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(llama_cpp)
  else()
    message(FATAL_ERROR "llama.cpp not found. Set LLAMA_CPP_DIR, add third_party/llama.cpp, or enable CPPDEEPSEEK_ALLOW_FETCHCONTENT_LLAMA.")
  endif()
endif()
if (CPPDEEPSEEK_BUILD_APP)
  add_executable(CppDeepSeek
    src/main.cpp
    src/DeepSeekClient.cpp
    src/AgentRuntime.cpp
    src/LogicGate.cpp
    src/CliOptions.cpp
    src/LlamaBackend.cpp
  )

  target_include_directories(CppDeepSeek PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
  target_link_libraries(CppDeepSeek PRIVATE CURL::libcurl nlohmann_json::nlohmann_json ModelStore::ModelStore)
  if (CPPDEEPSEEK_USE_LLAMA)
    target_link_libraries(CppDeepSeek PRIVATE llama)
  endif()
endif()

option(CPPDEEPSEEK_BUILD_TESTS "Build CppDeepSeek tests" ON)
if (CPPDEEPSEEK_BUILD_TESTS)
  set(GTEST_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/googletest)
  if (EXISTS ${GTEST_SOURCE_DIR}/CMakeLists.txt)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    add_subdirectory(${GTEST_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/third_party/googletest)
  else()
    include(FetchContent)
    FetchContent_Declare(
      googletest
      URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
      DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
  endif()
  enable_testing()
  include(GoogleTest)
  add_executable(AgentRuntimeTests tests/AgentRuntimeTests.cpp src/AgentRuntime.cpp)
  target_include_directories(AgentRuntimeTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
  target_link_libraries(AgentRuntimeTests PRIVATE GTest::gtest_main nlohmann_json::nlohmann_json)
  gtest_discover_tests(AgentRuntimeTests)

  add_executable(AgentPersistenceTests tests/AgentPersistenceTests.cpp src/AgentRuntime.cpp)
  target_include_directories(AgentPersistenceTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
  target_link_libraries(AgentPersistenceTests PRIVATE GTest::gtest_main nlohmann_json::nlohmann_json)
  gtest_discover_tests(AgentPersistenceTests)

  add_executable(LogicGateTests tests/LogicGateTests.cpp src/LogicGate.cpp)
  target_include_directories(LogicGateTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
  target_link_libraries(LogicGateTests PRIVATE GTest::gtest_main)
  gtest_discover_tests(LogicGateTests)

  add_executable(CliOptionsTests tests/CliOptionsTests.cpp src/CliOptions.cpp)
  target_include_directories(CliOptionsTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
  target_link_libraries(CliOptionsTests PRIVATE GTest::gtest_main)
  gtest_discover_tests(CliOptionsTests)

endif()
