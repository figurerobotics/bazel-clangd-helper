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
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "nlohmann/json.hpp"

ABSL_FLAG(std::string, output_path, "", "The output file path to write to.");
ABSL_FLAG(std::string, source_path, "",
          "The source file path for the compile command.");
ABSL_FLAG(std::string, directory, "",
          "The owning directory of the source path.");

namespace {

void GenerateCompileCommands(const std::string &output_path,
                             const std::string &source_path,
                             const std::string &directory,
                             const std::vector<char *> &positional_flags) {
  std::ofstream output_file(output_path);
  if (!output_file.is_open()) {
    std::cerr << "Failed to open output file: " << output_path << std::endl;
    return;
  }

  nlohmann::ordered_json output;
  output["file"] = source_path;
  // Ignore the first positional flag, which is the executable.
  output["arguments"] = std::vector<std::string>(positional_flags.begin() + 1,
                                                 positional_flags.end());
  output["directory"] = directory;

  output_file << output.dump(2) << ",\n";
  output_file.close();
}

} // namespace

int main(int argc, char **argv) {
  std::vector<char *> positional_flags = absl::ParseCommandLine(argc, argv);

  const std::string &output_path = absl::GetFlag(FLAGS_output_path);
  if (output_path.empty()) {
    std::cerr << "--output_path must be specified." << std::endl;
    return 1;
  }

  const std::string &source_path = absl::GetFlag(FLAGS_source_path);
  const std::string &directory = absl::GetFlag(FLAGS_directory);

  GenerateCompileCommands(output_path, source_path, directory,
                          positional_flags);

  return 0;
}
