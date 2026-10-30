#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Iain Chesworth
# SPDX-License-Identifier: GPL-3.0-or-later
"""Write the iOS bundle's localised InfoPlist.strings from the app's catalogues.

    python packaging/ios/make_infoplist_strings.py \\
        --catalogues src/app/i18n --out build/packaging/ios/lproj

The text iOS shows in its own camera prompt is NSCameraUsageDescription, which
the system reads from <language>.lproj/InfoPlist.strings of the app bundle, in
the language of the device and not the one chosen inside the app. The wording is
the string `permission.system.camera` of docs/design/strings.json, so it is
read from the translation catalogues (src/app/i18n/translations/cubeulator_*.ts)
instead of being written a second time.

Writes, under --out:

    <lproj>/InfoPlist.strings        one per language that has the string translated
    languages.txt                    those <lproj> names, one per line, English first

A language whose entry is missing, empty or still marked unfinished is left out:
iOS then falls back to English, as the app does. Standard library only.
"""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from xml.etree import ElementTree

KEY = "permission.system.camera"
PLIST_KEY = "NSCameraUsageDescription"

# Apple's name for a language directory where it differs from the app's code.
LPROJ = {"zh_CN": "zh-Hans"}


def translation(catalogue: Path) -> str | None:
    """The translated text of KEY in a .ts file, or None when there is none to use."""
    for message in ElementTree.parse(catalogue).getroot().iter("message"):
        if message.get("id") != KEY:
            continue
        node = message.find("translation")
        if node is None or node.get("type") in ("unfinished", "vanished", "obsolete"):
            return None
        text = "".join(node.itertext()).strip()
        return text or None
    return None


def quoted(text: str) -> str:
    escaped = text.replace("\\", "\\\\").replace('"', '\\"').replace("\n", "\\n")
    return f'"{escaped}"'


def build(catalogues: Path, out: Path) -> list[str]:
    languages = json.loads((catalogues / "languages.json").read_text(encoding="utf-8"))["languages"]
    written: list[str] = []
    for language in languages:
        text = translation(catalogues / "translations" / f"cubeulator_{language['file']}.ts")
        if text is None:
            continue
        lproj = LPROJ.get(language["code"], language["code"])
        target = out / f"{lproj}.lproj" / "InfoPlist.strings"
        target.parent.mkdir(parents=True, exist_ok=True)
        data = f"{quoted(PLIST_KEY)} = {quoted(text)};\n".encode("utf-8")
        # Leave an unchanged file alone so that build systems do not see a new timestamp.
        if not target.exists() or target.read_bytes() != data:
            target.write_bytes(data)
        written.append(lproj)
    if "en" in written:
        written.remove("en")
        written.insert(0, "en")
    listing = ("\n".join(written) + "\n").encode("utf-8")
    listing_path = out / "languages.txt"
    out.mkdir(parents=True, exist_ok=True)
    if not listing_path.exists() or listing_path.read_bytes() != listing:
        listing_path.write_bytes(listing)
    return written


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--catalogues", type=Path, required=True, help="src/app/i18n")
    parser.add_argument("--out", type=Path, required=True, help="output directory")
    args = parser.parse_args(argv)
    written = build(args.catalogues, args.out)
    if "en" not in written:
        print(f"error: no English entry for {KEY} in {args.catalogues}", file=sys.stderr)
        return 1
    for lproj in written:
        print(f"  {args.out / (lproj + '.lproj') / 'InfoPlist.strings'}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
