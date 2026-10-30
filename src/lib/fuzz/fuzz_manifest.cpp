// SPDX-FileCopyrightText: 2026 Iain Chesworth
// SPDX-License-Identifier: GPL-3.0-or-later
// A recording's manifest.json (cube::vision::recording::read_manifest) on any bytes. The bytes
// are written to a file in a directory of the harness's own, because the reader takes a
// directory.

#include <cube/vision/recording_v2.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <system_error>

namespace {

namespace fs = std::filesystem;

const fs::path& scratch_directory() {
    static const fs::path directory = [] {
        std::error_code ignored;
        fs::path path = fs::temp_directory_path(ignored) / "cubeulator-fuzz-manifest";
        fs::create_directories(path, ignored);
        return path;
    }();
    return directory;
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    const fs::path& directory = scratch_directory();
    {
        std::ofstream file(directory / "manifest.json", std::ios::binary | std::ios::trunc);
        file.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(size));
    }
    (void)cube::vision::recording::read_manifest(directory.string());
    return 0;
}
