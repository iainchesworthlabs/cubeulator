// SPDX-FileCopyrightText: 2026 Iain Chesworth
// SPDX-License-Identifier: GPL-3.0-or-later
// Runs the fuzz harnesses over files, with no fuzzing engine: each argument is a file or a
// directory of files, and every file is handed to LLVMFuzzerTestOneInput once. It is what the
// harnesses link to in a build without libFuzzer, so that the seeds and the regressions are
// replayed by ctest on any compiler, under the sanitizers of the preset. Arguments that start
// with a dash (libFuzzer's, such as -runs=0) are ignored.

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <print>
#include <span>
#include <string>
#include <vector>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size);

namespace {

namespace fs = std::filesystem;

std::vector<fs::path> inputs_in(const fs::path& path) {
    std::vector<fs::path> files;
    if (fs::is_directory(path)) {
        for (const auto& entry : fs::directory_iterator(path)) {
            if (entry.is_regular_file() && entry.path().filename() != ".gitkeep") {
                files.push_back(entry.path());
            }
        }
        std::ranges::sort(files);
    } else {
        files.push_back(path);
    }
    return files;
}

}  // namespace

int main(int argc, char** argv) {
    const std::span<char*> arguments(argv, static_cast<std::size_t>(argc));
    std::size_t replayed = 0;
    for (const std::string argument : arguments.subspan(1)) {
        if (argument.starts_with('-')) {
            continue;
        }
        const fs::path path(argument);
        if (!fs::exists(path)) {
            std::println(stderr, "no such file or directory: {}", argument);
            return 2;
        }
        for (const fs::path& file : inputs_in(path)) {
            std::ifstream stream(file, std::ios::binary);
            const std::vector<char> bytes((std::istreambuf_iterator<char>(stream)),
                                          std::istreambuf_iterator<char>());
            LLVMFuzzerTestOneInput(reinterpret_cast<const std::uint8_t*>(bytes.data()),
                                   bytes.size());
            ++replayed;
        }
    }
    std::println("replayed {} input(s)", replayed);
    return 0;
}
