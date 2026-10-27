#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Fail when a workflow, action or install script carries a toolchain version that differs from
.github/toolchain-versions.json.

    python3 tools/ci/check_toolchain_versions.py [--root .]

Each rule names the files it reads, a pattern with one group, and the key whose value the group
must equal. A rule that matches nothing is an error too, so a rewrite that moves a version
cannot leave the check passing without looking at anything.
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

WORKFLOWS = [".github/workflows/*.yml", ".github/actions/*/action.yml"]

# (key path, files, pattern)
RULES = [
    ("qt", ["ci/install-qt.sh"], r'^QT_VERSION="([^"]+)"'),
    ("qt", ["ci/install-qt.ps1"], r'^\$QtVersion = "([^"]+)"'),
    ("qt", WORKFLOWS, r'^\s+version: "(6\.\d+\.\d+)"'),
    ("qt", [".github/workflows/ios-device.yml"], r"/Qt/(6\.\d+\.\d+)/"),
    ("aqtsource", WORKFLOWS + ["ci/*.ps1", "ci/*.sh"], r"aqtinstall\.git@([0-9a-f]{40})"),
    ("cmake", WORKFLOWS, r'cmakeVersion: "([^"]+)"'),
    ("ninja", WORKFLOWS, r'ninjaVersion: "([^"]+)"'),
    ("gcc", WORKFLOWS, r"CMAKE_CXX_COMPILER=g\+\+-(\d+)"),
    ("gcc", [".github/actions/install-gcc/action.yml"], r'default: "(\d+)"'),
    ("emscripten", [".github/actions/web-build/action.yml"], r'^\s+version: "(5\.\d+\.\d+)"'),
    ("android.ndk", [".github/workflows/*.yml"], r"ndk[/;](\d+\.\d+\.\d+)"),
    ("android.platform", [".github/workflows/*.yml"], r"platforms;android-(\d+)"),
    ("android.build_tools", [".github/workflows/*.yml"], r"build-tools;(\d+\.\d+\.\d+)"),
]


def pinned(pins: dict, key: str) -> str:
    value = pins
    for part in key.split("."):
        value = value[part]
    if key == "aqtsource":
        value = value.rsplit("@", 1)[-1]
    return str(value)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", type=Path, default=Path(__file__).resolve().parent.parent.parent)
    root = ap.parse_args().root
    pins = json.loads((root / ".github" / "toolchain-versions.json").read_text(encoding="utf-8"))
    problems: list[str] = []
    for key, globs, pattern in RULES:
        want = pinned(pins, key)
        regex = re.compile(pattern, re.MULTILINE)
        seen = 0
        for glob in globs:
            for path in sorted(root.glob(glob)):
                for match in regex.finditer(path.read_text(encoding="utf-8")):
                    seen += 1
                    if match.group(1) != want:
                        line = path.read_text(encoding="utf-8")[: match.start(1)].count("\n") + 1
                        problems.append(f"{path.relative_to(root)}:{line}: {key} is {match.group(1)}, the pin is {want}")
        if seen == 0:
            problems.append(f"no file matches the rule for {key} ({pattern}); update tools/ci/check_toolchain_versions.py")
    for problem in problems:
        print(problem, file=sys.stderr)
    if not problems:
        print(f"toolchain versions agree with .github/toolchain-versions.json ({len(RULES)} rules)")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
