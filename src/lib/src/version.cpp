// SPDX-FileCopyrightText: 2026 Iain Chesworth
// SPDX-License-Identifier: GPL-3.0-or-later
#include <cube/version.hpp>

#include <fmt/format.h>

namespace cube {

std::string version_details() {
    return fmt::format(
        "Cubeulator {}\n"
        "  release: {}\n"
        "  commit:  {}\n"
        "  branch:  {}\n"
        "  target:  {}{}",
        version_string, git_describe, git_commit_full, git_branch, build_target,
        git_dirty ? "\n  state:   dirty (uncommitted changes)" : "");
}

}  // namespace cube
