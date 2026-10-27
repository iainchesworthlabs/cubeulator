#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Keep cmake/icl in step with icl-common-infra.

    python3 tools/ci/sync_cmake.py --infra ../icl-common-infra          copy the modules, record the commit
    python3 tools/ci/sync_cmake.py --infra ../icl-common-infra --check  fail when the copy differs

cmake/icl/SOURCE holds the commit of icl-common-infra the copy came from. The check compares
the files with the checkout and the recorded commit with the checkout's HEAD. CI runs it
against the commit the workflows pin, so a bump of that commit fails until this script has
been run.
"""

from __future__ import annotations

import argparse
import filecmp
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
DEST = ROOT / "cmake" / "icl"


def head(infra: Path) -> str:
    out = subprocess.run(["git", "-C", str(infra), "rev-parse", "HEAD"], capture_output=True, text=True)
    if out.returncode != 0:
        raise SystemExit(f"{infra} is not a git checkout")
    return out.stdout.strip()


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--infra", required=True, type=Path)
    ap.add_argument("--check", action="store_true")
    args = ap.parse_args()
    src = args.infra / "cmake"
    sha = head(args.infra)
    names = sorted(p.name for p in src.glob("*.cmake"))
    if args.check:
        problems = []
        recorded = (DEST / "SOURCE").read_text(encoding="utf-8").strip() if (DEST / "SOURCE").exists() else ""
        if recorded != sha:
            problems.append(f"SOURCE records {recorded or 'nothing'}, the pinned commit is {sha}")
        for name in names:
            if not (DEST / name).exists() or not filecmp.cmp(src / name, DEST / name, shallow=False):
                problems.append(f"cmake/icl/{name} differs from the pinned commit")
        for path in DEST.glob("*.cmake"):
            if path.name not in names:
                problems.append(f"cmake/icl/{path.name} is not in the pinned commit")
        for problem in problems:
            print(problem, file=sys.stderr)
        if problems:
            print("Run: python3 tools/ci/sync_cmake.py --infra <icl-common-infra checkout>", file=sys.stderr)
        return 1 if problems else 0
    DEST.mkdir(parents=True, exist_ok=True)
    for path in DEST.glob("*.cmake"):
        path.unlink()
    for name in names:
        shutil.copyfile(src / name, DEST / name)
    (DEST / "SOURCE").write_text(sha + "\n", encoding="utf-8")
    print(f"Copied {len(names)} modules from {sha}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
