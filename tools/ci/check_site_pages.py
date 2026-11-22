#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Iain Chesworth
# SPDX-License-Identifier: GPL-3.0-or-later
"""Check the built documentation site: the Cubeulator pages are plain HTML, load nothing from other sites, and link to files that exist.

    python3 tools/ci/check_site_pages.py [--site site] [--root .]

Without --site it builds the site with `mkdocs build --strict` into a temporary folder (it needs the packages of
docs/requirements.txt) and checks that. With --site it checks a folder that is already built.

Every page except 404.html:
- has a <title>, exactly one <h1>, a canonical address, and lang="en-AU" on the Cubeulator pages (marketing.html) or any
  language on the guide's pages (MkDocs Material);
- links to a file that exists, with an anchor that exists in it. Links that start with "/" are the company shell's
  (/, /support/, /assets/v1/...) and are not followed, but none of them may start with /cubeulator/ (the pages must work
  under a preview address, so the pages of this site use relative links). An absolute link is allowed to GitHub and to gnu.org
  on the Cubeulator pages; the guide may name other hosts in its links, never in what it loads. A link into app/ is not
  followed: the web app is added to the site by the Docs workflow, after this build.

The Cubeulator pages run under a preview that blocks every script, so they also have

- no <script> except a JSON-LD data block, no <style> and no style= attribute;
- nothing loaded from another host: no src, srcset, stylesheet link or CSS url() or @import that starts with http(s):// or //;
- no mailto: link (support is through GitHub issues only);
- every <img> with a width and a height in pixels, and, for a PNG or a WebP, of the same shape as the file, so the page does
  not shift as the picture loads.

On every page every <img> has an alt attribute (alt="" for a picture that only decorates; any other alt is a description of at
least 10 characters, not a file name). Whether the alt text is true of the picture is for a person to check: look at each
picture before writing its alt.

And the addresses that a store listing, the app or a release will print exist: /, privacy/, support/, guide/ (they never move
once printed) and the other pages of the site; the page "What's new" names the newest release of CHANGELOG.md; and the key
fingerprint of guide/verify/ is the one of docs/SECURITY.md.
"""

from __future__ import annotations

import argparse
import re
import shutil
import struct
import subprocess
import sys
import tempfile
from html.parser import HTMLParser
from pathlib import Path
from urllib.parse import unquote, urlparse

ROOT = Path(__file__).resolve().parents[2]
FIXED = ["index.html", "privacy/index.html", "support/index.html", "features/index.html", "faq/index.html",
         "whats-new/index.html", "accessibility/index.html", "press/index.html", "guide/index.html",
         "guide/install/index.html", "guide/web/index.html", "guide/iphone/index.html", "guide/verify/index.html",
         "guide/scan/index.html", "guide/solve/index.html", "guide/other-sizes/index.html", "guide/history/index.html",
         "guide/settings/index.html", "guide/how-it-works/index.html", "guide/troubleshooting/index.html",
         "theme.css", "icl-app.json", "assets/icl.css", "assets/cubeulator.css", "assets/guide.css",
         "assets/img/icon.svg", "assets/img/icon-dark.svg", "assets/img/icon-mono.svg"]
ALLOWED_HOSTS = {"github.com", "www.gnu.org"}
# Where the guide's own links may go (a link, never a load). The Material footer names its own host.
GUIDE_HOSTS = ALLOWED_HOSTS | {"docs.github.com", "cli.github.com", "emscripten.org", "squidfunk.github.io"}
APP_PREFIX = "app/"  # the web app, added by docs.yml after the build
MIN_ALT = 10
FILE_NAME = re.compile(r"[.](png|jpe?g|gif|svg|webp|avif)", re.IGNORECASE)
PNG_SIGNATURE = bytes([0x89]) + b"PNG" + bytes([0x0D, 0x0A, 0x1A, 0x0A])
FINGERPRINT = re.compile(r"(?:[0-9A-F]{4}[ 	]{0,2}){10}")


class Page(HTMLParser):
    def __init__(self) -> None:
        super().__init__(convert_charrefs=True)
        self.title = ""
        self._in_title = False
        self.h1 = 0
        self.lang = ""
        self.canonical = ""
        self.material = False
        self.ids: set[str] = set()
        self.refs: list[tuple[str, str, str]] = []  # (tag, attribute, value)
        self.scripts: list[str] = []  # the type of each <script>
        self.styles = 0
        self.style_attrs = 0
        self.images: list[dict[str, str | None]] = []  # src, alt (None when absent), width, height of each <img>

    def handle_starttag(self, tag, attrs):  # noqa: ANN001, ANN201
        a = {k: (v or "") for k, v in attrs}
        if tag == "html":
            self.lang = a.get("lang", "")
        if tag == "title":
            self._in_title = True
        if tag == "h1":
            self.h1 += 1
        if tag == "script":
            self.scripts.append(a.get("type", ""))
        if tag == "style":
            self.styles += 1
        if tag == "img":
            self.images.append({"src": a.get("src", ""), "alt": a["alt"] if "alt" in a else None,
                                "width": a.get("width"), "height": a.get("height")})
        if "style" in a:
            self.style_attrs += 1
        if "class" in a and "md-container" in a["class"].split():
            self.material = True
        if "id" in a:
            self.ids.add(a["id"])
        if tag == "a" and a.get("name"):
            self.ids.add(a["name"])
        if tag == "link" and a.get("rel") == "canonical":
            self.canonical = a.get("href", "")
            return
        for attribute in ("href", "src"):
            if attribute in a:
                self.refs.append((tag, attribute, a[attribute]))
        if "srcset" in a:
            for part in a["srcset"].split(","):
                if part.strip():
                    self.refs.append((tag, "srcset", part.split()[0]))

    def handle_endtag(self, tag):  # noqa: ANN001, ANN201
        if tag == "title":
            self._in_title = False

    def handle_data(self, data):  # noqa: ANN001, ANN201
        if self._in_title:
            self.title += data


def parse(path: Path) -> Page:
    page = Page()
    page.feed(path.read_text(encoding="utf-8"))
    return page


def target_file(site: Path, page_file: Path, link: str) -> Path | None:
    """The file a relative link names, or None when it leaves the site."""
    path = unquote(urlparse(link).path)
    page_file = page_file.resolve()
    target = (page_file.parent / path).resolve() if path else page_file
    try:
        target.relative_to(site.resolve())
    except ValueError:
        return None
    if target.is_dir():
        target = target / "index.html"
    return target


def image_size(path: Path) -> tuple[int, int] | None:
    """The width and height of a PNG or a WebP file, or None when the file is neither."""
    try:
        with path.open("rb") as handle:
            head = handle.read(32)
    except OSError:
        return None
    if len(head) >= 24 and head.startswith(PNG_SIGNATURE) and head[12:16] == b"IHDR":
        return int.from_bytes(head[16:20], "big"), int.from_bytes(head[20:24], "big")
    if len(head) >= 30 and head[:4] == b"RIFF" and head[8:12] == b"WEBP":
        kind = head[12:16]
        if kind == b"VP8X":
            return 1 + int.from_bytes(head[24:27], "little"), 1 + int.from_bytes(head[27:30], "little")
        if kind == b"VP8 ":
            return struct.unpack("<H", head[26:28])[0] & 0x3FFF, struct.unpack("<H", head[28:30])[0] & 0x3FFF
        if kind == b"VP8L":
            bits = int.from_bytes(head[21:25], "little")
            return (bits & 0x3FFF) + 1, ((bits >> 14) & 0x3FFF) + 1
    return None


def check_images(site: Path, path: Path, page: Page) -> list[str]:
    """The picture rules of the module docstring, for the <img> elements of one page."""
    where = path.relative_to(site).as_posix()
    problems: list[str] = []
    for image in page.images:
        src = image["src"] or "(no src)"
        alt = image["alt"]
        if alt is None:
            problems.append(f'{where}: {src}: <img> has no alt attribute (alt="" if it only decorates)')
        elif alt.strip():
            if FILE_NAME.search(alt):
                problems.append(f"{where}: {src}: the alt text looks like a file name; describe what the picture shows")
            elif len(alt.strip()) < MIN_ALT:
                problems.append(f"{where}: {src}: the alt text is shorter than {MIN_ALT} characters; describe what the picture shows")
        elif alt != "":
            problems.append(f'{where}: {src}: write alt="" for a decorative picture, not a space')
        if page.material:
            continue  # the guide's pictures are sized by the theme's CSS, not by their markup
        sizes = []
        for name in ("width", "height"):
            value = image[name]
            if value is None or not value.isdigit() or int(value) <= 0:
                problems.append(f"{where}: {src}: <img> needs a {name} in pixels, so the page does not shift as it loads")
            else:
                sizes.append(int(value))
        if len(sizes) == 2 and image["src"] and not urlparse(image["src"]).scheme and not image["src"].startswith(("/", "//")):
            target = target_file(site, path, image["src"])
            real = image_size(target) if target is not None and target.is_file() else None
            if real and abs(sizes[0] / sizes[1] - real[0] / real[1]) > 0.02 * real[0] / real[1]:
                problems.append(f"{where}: {src}: width {sizes[0]} and height {sizes[1]} are not the shape of the picture ({real[0]} by {real[1]})")
    return problems


def newest_release(root: Path) -> str | None:
    """The version of the newest dated release in CHANGELOG.md, or None when there is none."""
    changelog = root / "CHANGELOG.md"
    if not changelog.is_file():
        return None
    match = re.search(r"^## \[(\d+\.\d+\.\d+(?:-[0-9A-Za-z.]+)?)\] - \d{4}-\d{2}-\d{2}", changelog.read_text(encoding="utf-8"), re.M)
    return match.group(1) if match else None


def check_facts(site: Path, root: Path) -> list[str]:
    """Facts that two files must agree on: the release the news page names, and the signing key's fingerprint."""
    problems: list[str] = []
    version = newest_release(root)
    news = site / "whats-new" / "index.html"
    if version and news.is_file() and f"<strong>{version}</strong>" not in news.read_text(encoding="utf-8"):
        problems.append(f"whats-new/: does not name the newest release of CHANGELOG.md, {version}, in bold at the top: "
                        "a release changes docs/whats-new.md as well")
    security = root / "docs" / "SECURITY.md"
    verify = site / "guide" / "verify" / "index.html"
    if security.is_file() and verify.is_file():
        want = {re.sub(r"\s+", "", m.group(0)) for m in FINGERPRINT.finditer(security.read_text(encoding="utf-8"))}
        have = {re.sub(r"\s+", "", m.group(0)) for m in FINGERPRINT.finditer(re.sub(r"<[^>]+>", "", verify.read_text(encoding="utf-8")))}
        if not want:
            problems.append("docs/SECURITY.md: no key fingerprint found to hold guide/verify/ to")
        elif not (want & have):
            problems.append("guide/verify/: the key fingerprint is not the one of docs/SECURITY.md; change both together")
    return problems


def check_site(site: Path, root: Path) -> list[str]:
    problems: list[str] = []
    for name in FIXED:
        if not (site / name).is_file():
            problems.append(f"{name}: is not in the built site (a store listing, the app or a release will print it, or it is a page of the site)")
    pages = {p: parse(p) for p in sorted(site.rglob("*.html"))}
    for path, page in pages.items():
        rel = path.relative_to(site).as_posix()
        if rel == "404.html":
            continue
        where = rel
        ours = not page.material  # a page of marketing.html: the strict rules apply
        if not page.title.strip():
            problems.append(f"{where}: no <title>")
        if page.h1 != 1:
            problems.append(f"{where}: has {page.h1} <h1>, not one")
        if not page.canonical:
            problems.append(f"{where}: no canonical address")
        if ours and page.lang != "en-AU":
            problems.append(f'{where}: <html> needs lang="en-AU"')
        if ours:
            for kind in page.scripts:
                if kind != "application/ld+json":
                    problems.append(f"{where}: has a <script>; the Cubeulator pages are script-free (a preview blocks scripts)")
            if page.styles or page.style_attrs:
                problems.append(f"{where}: has <style> or a style= attribute; put it in docs/assets/cubeulator.css")
        problems.extend(check_images(site, path, page))
        hosts = ALLOWED_HOSTS if ours else GUIDE_HOSTS
        for tag, attribute, value in page.refs:
            value = value.strip()
            if not value or value.startswith(("data:", "javascript:")):
                continue
            if value.startswith("mailto:"):
                problems.append(f"{where}: {value}: support is through GitHub issues only, no email address")
                continue
            parts = urlparse(value)
            if parts.scheme in ("http", "https") or value.startswith("//"):
                if not ours and attribute == "href" and tag in ("a", "link") and parts.scheme in ("http", "https") and parts.netloc not in hosts:
                    continue  # the guide's footer and Material's own links
                if parts.scheme not in ("http", "https") or parts.netloc not in hosts:
                    problems.append(f"{where}: {value}: not a link to a host this site may name ({', '.join(sorted(hosts))})")
                elif attribute == "srcset" or (tag != "a" and tag != "link"):
                    problems.append(f"{where}: {value}: loads something from another host")
                elif tag == "link" and "icon" not in value:
                    problems.append(f"{where}: {value}: a <link> to another host")
                continue
            if value.startswith("/"):
                if value.startswith("/cubeulator/"):
                    problems.append(f"{where}: {value}: write a relative link, so the page works under a preview address")
                continue
            if parts.scheme:
                problems.append(f"{where}: {value}: unknown link type")
                continue
            target = target_file(site, path, value)
            if target is not None and target.relative_to(site.resolve()).as_posix().startswith(APP_PREFIX.rstrip("/")):
                continue  # the web app, added after the build
            if target is None or not target.exists():
                problems.append(f"{where}: {value}: no such file in the built site")
                continue
            if parts.fragment and target.suffix == ".html":
                other = pages.get(target) or parse(target)
                if unquote(parts.fragment) not in other.ids:
                    problems.append(f"{where}: {value}: no anchor #{parts.fragment} in {target.relative_to(site).as_posix()}")
    for name in ("theme.css", "assets/cubeulator.css", "assets/guide.css", "assets/icl.css"):
        css = site / name
        if css.is_file():
            text = re.sub(r"/\*.*?\*/", "", css.read_text(encoding="utf-8"), flags=re.S)
            if re.search(r"@import|url\(\s*['\"]?(https?:)?//", text):
                problems.append(f"{name}: loads something with @import or an address; the site loads nothing from other sites")
    problems.extend(check_facts(site, root))
    return problems


def build(root: Path, out: Path) -> int:
    result = subprocess.run([sys.executable, "-m", "mkdocs", "build", "--strict", "--quiet", "--site-dir", str(out)],
                            cwd=root, capture_output=True, text=True, check=False)
    if result.returncode != 0:
        sys.stderr.write(result.stdout + result.stderr)
    return result.returncode


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("--site", type=Path, help="a site that is already built")
    args = parser.parse_args(argv)
    tmp = None
    site = args.site
    if site is None:
        if subprocess.run([sys.executable, "-c", "import mkdocs, material"], capture_output=True, check=False).returncode != 0:
            print("check_site_pages: mkdocs-material is not installed (pip install -r docs/requirements.txt): cannot build the site", file=sys.stderr)
            return 2
        tmp = tempfile.mkdtemp(prefix="cubeulator-site-")
        site = Path(tmp)
        if build(args.root, site) != 0:
            shutil.rmtree(tmp, ignore_errors=True)
            return 1
    try:
        problems = check_site(site, args.root)
    finally:
        if tmp:
            shutil.rmtree(tmp, ignore_errors=True)
    for problem in problems:
        print(f"::error::{problem}")
    if problems:
        return 1
    print("Site pages: ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
