#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Run the pull request's static checks locally.

    python3 tools/ci/precheck.py [--base develop] [--root .]

Finds a checkout of icl-common-infra in ICL_COMMON_INFRA, then ../icl-common-infra, and runs
its scripts/precheck.py with the same arguments. These are the checks the Policy job runs.
"""

from __future__ import annotations

import os
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent


def find_infra() -> Path | None:
    candidates = []
    if os.environ.get("ICL_COMMON_INFRA"):
        candidates.append(Path(os.environ["ICL_COMMON_INFRA"]))
    candidates += [ROOT.parent / "icl-common-infra", ROOT / ".icl-infra"]
    for path in candidates:
        if (path / "scripts" / "precheck.py").is_file():
            return path
    return None


def main() -> int:
    infra = find_infra()
    if infra is None:
        print("icl-common-infra was not found. Clone it next to this repository or set "
              "ICL_COMMON_INFRA to its path.", file=sys.stderr)
        return 2
    args = sys.argv[1:]
    if "--root" not in args:
        args += ["--root", str(ROOT)]
    return subprocess.call([sys.executable, str(infra / "scripts" / "precheck.py"), *args])


if __name__ == "__main__":
    sys.exit(main())
