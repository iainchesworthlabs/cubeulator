# SPDX-FileCopyrightText: 2026 Iain Chesworth
# SPDX-License-Identifier: GPL-3.0-or-later
"""MkDocs hook: put the app icon on the site, from the one place it is drawn.

The icon is drawn by docs/design/identity (docs/design/tools/make_icon.py), which `exclude_docs` keeps out of the site. The pages
show it and the Press page offers it, so this copies the three files they use to assets/img/ of the built site, and the build
fails when one is missing. Nothing is copied into the repository, so the icon cannot drift from its source.
"""

from __future__ import annotations

import shutil
from pathlib import Path

ICONS = ("icon.svg", "icon-dark.svg", "icon-mono.svg")


def on_post_build(config):  # noqa: ANN001, ANN201 (MkDocs passes its own types)
    source = Path(config["docs_dir"]) / "design" / "identity"
    target = Path(config["site_dir"]) / "assets" / "img"
    target.mkdir(parents=True, exist_ok=True)
    for name in ICONS:
        shutil.copyfile(source / name, target / name)
