#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Move every icl-common-infra reference in the workflows to one commit.

    python3 tools/ci/repin.py --infra ../icl-common-infra --version v0.1.1

--infra is a checkout of icl-common-infra at the commit to use, and --version is the tag at
that commit; the tag goes in the trailing comment of each `uses:` line. The script also copies
the CMake modules from the same checkout, so the workflows and cmake/icl always agree (CI
fails when they do not). Dependabot bumps the `uses:` lines on its own; run this script on its
branch to bring the modules along.
"""

from __future__ import annotations

import argparse
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
REF = re.compile(r"(iainchesworthlabs/icl-common-infra/[^\s@]+)@[0-9a-f]{40}(?: # \S+)?")


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--infra", required=True, type=Path)
    ap.add_argument("--version", required=True, help="tag at that commit, for example v0.1.1")
    args = ap.parse_args()
    out = subprocess.run(["git", "-C", str(args.infra), "rev-parse", "HEAD"], capture_output=True, text=True)
    if out.returncode != 0:
        print(f"{args.infra} is not a git checkout", file=sys.stderr)
        return 2
    sha = out.stdout.strip()
    changed = 0
    for path in sorted((ROOT / ".github").rglob("*.yml")):
        text = path.read_text(encoding="utf-8")
        new = REF.sub(lambda m: f"{m.group(1)}@{sha} # {args.version}", text)
        if new != text:
            path.write_text(new, encoding="utf-8", newline="")
            changed += 1
            print(f"updated {path.relative_to(ROOT)}")
    print(f"{changed} files now pin {sha[:12]} ({args.version})")
    return subprocess.call([sys.executable, str(ROOT / "tools/ci/sync_cmake.py"), "--infra", str(args.infra)])


if __name__ == "__main__":
    sys.exit(main())
