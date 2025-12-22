/*
 * Copyright 2024 Figure AI, Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/strings/match.h"
#include "absl/strings/strip.h"
#include "nlohmann/json.hpp"

ABSL_FLAG(std::string, workspace_dir, "", "The path to the workspace directory.");
ABSL_FLAG(std::string, compile_commands_path, "", "The path to the compile_commands.json file.");
ABSL_FLAG(std::string, build_event_path, "",
          "The path to the Bazel build event file.");

int main(int argc, char **argv) {
  std::vector<char *> positional_flags = absl::ParseCommandLine(argc, argv);

  std::string workspace_dir = absl::GetFlag(FLAGS_workspace_dir);
  if (workspace_dir.empty()) {
    std::cerr << "--workspace_dir must be specified." << std::endl;
    return 1;
  }
  // Ensure no trailing slash.
  while (workspace_dir.back() == '/') {
    workspace_dir.pop_back();
  }

  const std::string &compile_commands_path = absl::GetFlag(FLAGS_compile_commands_path);
  if (compile_commands_path.empty()) {
    std::cerr << "--compile_commands_path must be specified." << std::endl;
    return 1;
  }
  const std::string &build_event_path = absl::GetFlag(FLAGS_build_event_path);
  if (build_event_path.empty()) {
    std::cerr << "--build_event_path must be specified." << std::endl;
    return 1;
  }

  nlohmann::ordered_json compile_commands_json;
  absl::flat_hash_map<std::string, size_t> file_to_index;

  if (std::filesystem::exists(compile_commands_path)) {
    std::ifstream compile_commands_file(compile_commands_path);
    try {
      compile_commands_json = nlohmann::ordered_json::parse(compile_commands_file);
    } catch (const nlohmann::json::parse_error& e) {
      std::cerr << "Error parsing compile commands file: " << e.what() << std::endl;
      // Start with an empty JSON object.
    }
    // Populate file_to_index with the existing compile commands.
    for (size_t i = 0; i < compile_commands_json.size(); ++i) {
      const auto file = compile_commands_json[i]["file"].get<std::string>();
      file_to_index[file] = i;
    }
    std::cout << "Read " << file_to_index.size() << " existing compile commands from " << compile_commands_path << std::endl;
  }

  {
    std::cout << "Reading build event file: " << build_event_path << std::endl;
    std::ifstream build_event_file(build_event_path);
    std::string line;
    while (std::getline(build_event_file, line)) {
      nlohmann::json entry = nlohmann::json::parse(line);
      if (!entry.contains("namedSetOfFiles")) {
        continue;
      }
      const auto& file_infos = entry["namedSetOfFiles"]["files"];
      for (const auto& file_info : file_infos) {
        std::filesystem::path file_path = workspace_dir;
        for (const auto& prefix : file_info["pathPrefix"]) {
          file_path /= prefix.get<std::string>();
        }
        file_path /= file_info["name"].get<std::string>();
        if (!std::filesystem::exists(file_path)) {
          std::cerr << "File not found: " << file_path << std::endl;
          continue;
        }

        nlohmann::ordered_json fragment_json;
        {
          std::ifstream fragment_file(file_path);
          // Remove trailing comma if present (produced from legacy `generate_compile_command`).
          std::string content((std::istreambuf_iterator<char>(fragment_file)),
                              std::istreambuf_iterator<char>());
          content = absl::StripTrailingAsciiWhitespace(content);
          if (content.back() == ',') {
            content.pop_back();
          }
          fragment_json = nlohmann::ordered_json::parse(content);
        }
        // Replace the placeholder workspace directory.
        if (fragment_json["directory"].get<std::string_view>() == "__BAZEL_WORKSPACE_DIR__") {
          fragment_json["directory"] = workspace_dir;
        }

        auto file_name = fragment_json["file"].get<std::string>();
        if (auto it = file_to_index.find(file_name); it != file_to_index.end()) {
          // Overwrite existing entry.
          compile_commands_json[it->second] = fragment_json;
        } else {
          // Append new entry and add to map.
          file_to_index[file_name] = compile_commands_json.size();
          compile_commands_json.push_back(fragment_json);
        }
      }
    }
  }

  if (file_to_index.empty()) {
    std::cout << "No compile commands generated." << std::endl;
    return 0;
  }

  std::ofstream compile_commands_file(compile_commands_path);
  compile_commands_file << compile_commands_json.dump(/*indent=*/4);
  std::cout << "Wrote " << file_to_index.size() << " compile commands to " << compile_commands_path << std::endl;
  return 0;
}
