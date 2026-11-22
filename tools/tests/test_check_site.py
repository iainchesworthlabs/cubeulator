# SPDX-FileCopyrightText: 2026 Iain Chesworth
# SPDX-License-Identifier: GPL-3.0-or-later
"""Tests for check_site_kit.py, check_site_pages.py, site_shell_hook.py and site_assets_hook.py: the Cubeulator pages.

Run with: python3 -m unittest discover -s tools/tests -v
The tests that need Node (the company's palette checker) or jinja2 (the MkDocs hook) skip themselves when it is missing.
With CUBEULATOR_SITE_TESTS_STRICT=1 (the Site job of ci.yml) a missing tool fails instead, so a skipped test cannot hide a
broken check. The page checks run on small sites written to a temporary folder; the kit check runs on a copy of the kit.
"""

from __future__ import annotations

import importlib.util
import os
import re
import shutil
import struct
import sys
import tempfile
import unittest
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def requires(present: bool, reason: str):  # noqa: ANN201
    """Skip a test when a tool is missing, unless CUBEULATOR_SITE_TESTS_STRICT is set: then the test runs and fails."""
    if os.environ.get("CUBEULATOR_SITE_TESTS_STRICT"):
        return lambda item: item
    return unittest.skipUnless(present, reason)


needs_node = requires(shutil.which("node") is not None, "node is not installed")
needs_jinja = requires(importlib.util.find_spec("jinja2") is not None, "jinja2 is not installed")
sys.path.insert(0, str(ROOT / "tools" / "ci"))

import check_site_kit as kit_check  # noqa: E402
import check_site_pages as pages_check  # noqa: E402


def load_hook(name: str):
    spec = importlib.util.spec_from_file_location(name, ROOT / "tools" / "docs" / f"{name}.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def png(width: int, height: int) -> bytes:
    """The smallest PNG of the given size (a blank grey image), made by hand so the tests need no imaging library."""

    def chunk(kind: bytes, data: bytes) -> bytes:
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))

    rows = b"".join(bytes([0]) + bytes([0x80]) * width for _ in range(height))
    header = struct.pack(">IIBBBBB", width, height, 8, 0, 0, 0, 0)
    return bytes([0x89]) + b"PNG" + bytes([0x0D, 0x0A, 0x1A, 0x0A]) + chunk(b"IHDR", header) + chunk(b"IDAT", zlib.compress(rows)) + chunk(b"IEND", b"")


def webp(width: int, height: int) -> bytes:
    """The header of a lossy WebP of the given size (enough for a size check; it is not a picture)."""
    frame = b"\x9d\x01\x2a" + struct.pack("<HH", width, height)
    chunk = b"VP8 " + struct.pack("<I", len(frame)) + b"\x00\x00\x00" + frame
    return b"RIFF" + struct.pack("<I", 4 + len(chunk)) + b"WEBP" + chunk


PAGE = """<!doctype html><html lang="en-AU" data-app="cubeulator"><head><title>T</title>
<link rel="canonical" href="https://example.test/x/"><link rel="stylesheet" href="{css}"></head>
<body><main><h1>Heading</h1>{body}</main></body></html>"""

MATERIAL = """<!doctype html><html lang="en" data-app="cubeulator"><head><title>T</title>
<link rel="canonical" href="https://example.test/x/"><script src="../assets/bundle.js"></script></head>
<body><div class="md-container"><main><h1>Heading</h1>{body}</main></div></body></html>"""


def page(body: str = "", css: str = "/assets/shell.css") -> str:
    return PAGE.format(body=body, css=css)


class KitCheck(unittest.TestCase):
    def setUp(self) -> None:
        self.dir = tempfile.TemporaryDirectory()
        self.root = Path(self.dir.name)
        shutil.copytree(ROOT / kit_check.KIT, self.root / kit_check.KIT)
        shutil.copyfile(ROOT / kit_check.THEME, self._theme())

    def _theme(self) -> Path:
        path = self.root / kit_check.THEME
        path.parent.mkdir(parents=True, exist_ok=True)
        return path

    def tearDown(self) -> None:
        self.dir.cleanup()

    @needs_node
    def test_the_pinned_copy_and_palette_pass(self) -> None:
        self.assertEqual(kit_check.check_sums(self.root), [])
        self.assertEqual(kit_check.check_palette(self.root), [])

    def test_an_edited_kit_file_fails(self) -> None:
        with (self.root / kit_check.KIT / "icl.css").open("a", encoding="utf-8") as handle:
            handle.write("/* edited */\n")
        self.assertTrue(any("icl.css" in p and "edited" in p for p in kit_check.check_sums(self.root)))

    def test_a_missing_and_an_extra_kit_file_fail(self) -> None:
        (self.root / kit_check.KIT / "kit" / "theme.css").unlink()
        (self.root / kit_check.KIT / "extra.css").write_text("x", encoding="utf-8")
        problems = kit_check.check_sums(self.root)
        self.assertTrue(any("kit/theme.css" in p and "missing" in p for p in problems))
        self.assertTrue(any("extra.css" in p for p in problems))

    def test_line_endings_are_not_compared(self) -> None:
        path = self.root / kit_check.KIT / "icl.css"
        path.write_bytes(path.read_bytes().replace(b"\r\n", b"\n").replace(b"\n", b"\r\n"))
        self.assertEqual(kit_check.check_sums(self.root), [])

    @needs_node
    def test_a_palette_that_fails_contrast_is_reported(self) -> None:
        theme = self.root / kit_check.THEME
        theme.write_text(theme.read_text(encoding="utf-8").replace("light-dark(#b82009, #ff5a3d)", "light-dark(#ffe08a, #ff5a3d)", 1),
                         encoding="utf-8")
        problems = kit_check.check_palette(self.root)
        self.assertEqual(len(problems), 1)
        self.assertIn("FAIL", problems[0])

    @needs_node
    def test_a_property_that_is_not_a_role_is_reported(self) -> None:
        theme = self.root / kit_check.THEME
        theme.write_text(theme.read_text(encoding="utf-8").replace("}", "  --app-extra: #000000;\n}"), encoding="utf-8")
        self.assertTrue(kit_check.check_palette(self.root))


class PagesCheck(unittest.TestCase):
    def setUp(self) -> None:
        self.dir = tempfile.TemporaryDirectory()
        self.site = Path(self.dir.name)
        self.root = self.site / "_repo"  # a repository with no CHANGELOG.md, so the fact checks have nothing to compare
        self.root.mkdir()
        for name in pages_check.FIXED:
            path = self.site / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(page() if name.endswith(".html") else "x", encoding="utf-8")

    def tearDown(self) -> None:
        self.dir.cleanup()

    def problems(self, body: str, where: str = "faq") -> list[str]:
        (self.site / where / "index.html").write_text(page(body), encoding="utf-8")
        return pages_check.check_site(self.site, self.root)

    def test_a_clean_site_passes(self) -> None:
        self.assertEqual(self.problems('<a href="../privacy/">p</a> <a href="https://github.com/x">g</a> <a href="/support/">shell</a>'), [])

    def test_a_missing_fixed_address_is_reported(self) -> None:
        (self.site / "privacy" / "index.html").unlink()
        self.assertTrue(any("privacy/index.html" in p for p in pages_check.check_site(self.site, self.root)))

    def test_a_script_is_reported_but_json_ld_is_not(self) -> None:
        self.assertTrue(any("script" in p for p in self.problems("<script>x</script>")))
        self.assertEqual(self.problems('<script type="application/ld+json">{}</script>'), [])

    def test_a_load_from_another_host_is_reported(self) -> None:
        self.assertTrue(self.problems('<img src="https://example.org/a.png">'))
        self.assertTrue(self.problems('<a href="https://example.org/">x</a>'))

    def test_a_style_attribute_is_reported(self) -> None:
        self.assertTrue(any("style" in p for p in self.problems('<p style="color:red">x</p>')))

    def test_a_broken_link_and_anchor_are_reported(self) -> None:
        self.assertTrue(any("no such file" in p for p in self.problems('<a href="../nope/">x</a>')))
        self.assertTrue(any("no anchor" in p for p in self.problems('<a href="../privacy/#missing">x</a>')))

    def test_an_anchor_that_exists_passes(self) -> None:
        (self.site / "privacy" / "index.html").write_text(page('<h2 id="there">x</h2>'), encoding="utf-8")
        self.assertEqual(self.problems('<a href="../privacy/#there">x</a>'), [])

    def test_a_link_that_names_the_section_is_reported(self) -> None:
        self.assertTrue(any("relative" in p for p in self.problems('<a href="/cubeulator/faq/">x</a>')))

    def test_a_link_into_the_web_app_is_not_followed(self) -> None:
        self.assertEqual(self.problems('<a href="../app/">open</a> <a href="../app/?demo=7">demo</a>'), [])

    def test_an_email_address_is_reported(self) -> None:
        self.assertTrue(any("email" in p for p in self.problems('<a href="mailto:a@b.test">x</a>')))

    def test_a_picture_needs_alt_text_and_a_size(self) -> None:
        sized = 'src="../assets/img/icon.svg"'
        self.assertTrue(any("no alt" in p for p in self.problems(f'<img {sized} width="8" height="8">')))
        self.assertTrue(any("width" in p for p in self.problems(f'<img {sized} height="8" alt="">')))
        self.assertTrue(any("height" in p for p in self.problems(f'<img {sized} width="8" alt="">')))
        self.assertTrue(any("width" in p for p in self.problems(f'<img {sized} width="wide" height="8" alt="">')))

    def test_alt_text_that_is_a_file_name_or_too_short_is_reported(self) -> None:
        sized = 'src="../assets/img/icon.svg" width="8" height="8"'
        self.assertTrue(any("file name" in p for p in self.problems(f'<img {sized} alt="icon-1024.png">')))
        self.assertTrue(any("shorter" in p for p in self.problems(f'<img {sized} alt="Icon">')))
        self.assertTrue(any("decorative" in p for p in self.problems(f'<img {sized} alt=" ">')))
        self.assertEqual(self.problems(f'<img {sized} alt="The Cubeulator icon: a cube drawn in ink.">'), [])

    def test_a_size_of_the_wrong_shape_for_a_picture_is_reported(self) -> None:
        (self.site / "assets" / "img").mkdir(parents=True, exist_ok=True)
        (self.site / "assets" / "img" / "a.png").write_bytes(png(40, 20))
        (self.site / "assets" / "img" / "b.webp").write_bytes(webp(900, 640))
        self.assertEqual(self.problems('<img src="../assets/img/a.png" width="400" height="200" alt="">'), [])
        self.assertTrue(any("shape" in p for p in self.problems('<img src="../assets/img/a.png" width="200" height="200" alt="">')))
        self.assertEqual(self.problems('<img src="../assets/img/b.webp" width="900" height="640" alt="">'), [])
        self.assertTrue(any("shape" in p for p in self.problems('<img src="../assets/img/b.webp" width="640" height="640" alt="">')))

    def test_the_size_of_a_picture_is_read_from_the_file(self) -> None:
        folder = self.site / "assets" / "img"
        folder.mkdir(parents=True, exist_ok=True)
        (folder / "a.png").write_bytes(png(40, 20))
        (folder / "b.webp").write_bytes(webp(390, 844))
        (folder / "c.svg").write_text("<svg/>", encoding="utf-8")
        self.assertEqual(pages_check.image_size(folder / "a.png"), (40, 20))
        self.assertEqual(pages_check.image_size(folder / "b.webp"), (390, 844))
        self.assertIsNone(pages_check.image_size(folder / "c.svg"))
        self.assertIsNone(pages_check.image_size(folder / "missing.png"))

    def test_a_page_needs_one_heading(self) -> None:
        (self.site / "faq" / "index.html").write_text(page().replace("</main>", "<h1>Two</h1></main>"), encoding="utf-8")
        self.assertTrue(any("<h1>" in p for p in pages_check.check_site(self.site, self.root)))

    def test_a_stylesheet_that_imports_is_reported(self) -> None:
        (self.site / "assets" / "cubeulator.css").write_text('@import url("https://example.org/f.css");', encoding="utf-8")
        self.assertTrue(any("cubeulator.css" in p for p in pages_check.check_site(self.site, self.root)))

    def test_the_guide_may_run_script_and_link_out_but_not_load_from_another_host(self) -> None:
        (self.site / "guide" / "scan" / "index.html").write_text(
            MATERIAL.format(body='<a href="https://example.org/x">out</a> <img src="https://example.org/a.png" alt="A picture from elsewhere.">'),
            encoding="utf-8")
        problems = pages_check.check_site(self.site, self.root)
        self.assertEqual([p for p in problems if "has a <script>" in p or "example.org/x" in p], [])
        self.assertTrue(any("example.org/a.png" in p for p in problems))

    def test_the_news_page_must_name_the_newest_release(self) -> None:
        (self.root / "CHANGELOG.md").write_text("## [Unreleased]\n\n## [1.2.3] - 2026-01-02\n\n## [1.2.2] - 2026-01-01\n", encoding="utf-8")
        self.assertEqual(pages_check.newest_release(self.root), "1.2.3")
        self.assertTrue(any("1.2.3" in p for p in pages_check.check_site(self.site, self.root)))
        (self.site / "whats-new" / "index.html").write_text(page("<p>The latest is <strong>1.2.3</strong>.</p>"), encoding="utf-8")
        self.assertEqual(pages_check.check_site(self.site, self.root), [])

    def test_the_verify_page_must_carry_the_fingerprint_of_the_security_policy(self) -> None:
        (self.root / "docs").mkdir()
        (self.root / "docs" / "SECURITY.md").write_text("| Fingerprint | `0CF2 A242 440A 7AF0 18A2  BFF3 03E2 D330 B299 EB46` |", encoding="utf-8")
        (self.site / "guide" / "verify" / "index.html").write_text(
            page("<code>0CF2 A242 440A 7AF0 18A2  BFF3 03E2 D330 B299 EB46</code>"), encoding="utf-8")
        self.assertEqual(pages_check.check_site(self.site, self.root), [])
        (self.site / "guide" / "verify" / "index.html").write_text(page("<code>0CF2 A242 440A 7AF0 18A2  BFF3 03E2 D330 B299 EB47</code>"), encoding="utf-8")
        self.assertTrue(any("fingerprint" in p for p in pages_check.check_site(self.site, self.root)))


class TheRepository(unittest.TestCase):
    """The checks run on the files of this repository, as CI runs them."""

    def test_the_pictures_of_the_pages_are_the_shape_the_pages_write(self) -> None:
        # The sizes in the markup of the pages (docs/*.md with the marketing template), against the files, without building.
        checked = 0
        for path in sorted((ROOT / "docs").glob("*.md")):
            text = path.read_text(encoding="utf-8")
            if "template: marketing.html" not in text:
                continue
            for match in re.finditer(r'<img [^>]*src="((?:[.][.]/)*)([^"]+)"[^>]*>', text):
                tag, name = match.group(0), match.group(2)
                # The icons are drawn in docs/design/identity and copied to assets/img/ by tools/docs/site_assets_hook.py.
                source = ROOT / "docs" / ("design/identity/" + Path(name).name if name.startswith("assets/img/icon") else name)
                self.assertTrue(source.is_file(), f"{path.name}: {name} is not in docs/")
                width = re.search(r'width="(\d+)"', tag)
                height = re.search(r'height="(\d+)"', tag)
                self.assertTrue(width and height, f"{path.name}: {name} has no width and height")
                real = pages_check.image_size(source)
                if real is None:
                    continue  # an SVG scales to any size
                self.assertAlmostEqual(int(width.group(1)) / int(height.group(1)), real[0] / real[1],
                                       delta=0.02 * real[0] / real[1], msg=f"{path.name}: {name}")
                checked += 1
        self.assertGreater(checked, 3)

    @needs_node
    def test_the_kit_and_palette_of_the_repository_pass(self) -> None:
        self.assertEqual(kit_check.check_sums(ROOT), [])
        self.assertEqual(kit_check.check_palette(ROOT), [])

    def test_the_palette_is_one_rule_with_the_eight_roles(self) -> None:
        text = (ROOT / kit_check.THEME).read_text(encoding="utf-8")
        body = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
        self.assertEqual(body.count("{"), 1)
        self.assertIn('[data-app="cubeulator"]', body)
        self.assertEqual(len(re.findall(r"--app-[a-z-]+:\s*light-dark\(#[0-9a-f]{6}, #[0-9a-f]{6}\);", body)), 8)

    def test_every_page_source_names_the_template_and_quotes_its_title(self) -> None:
        for path in sorted((ROOT / "docs").glob("*.md")):
            text = path.read_text(encoding="utf-8")
            head = text.split("---")[1] if text.startswith("---") else ""
            if "template: marketing.html" not in head:
                continue
            for line in head.splitlines():
                key, _, value = line.partition(": ")
                if key in ("title_tag", "description") and ": " in value:
                    self.assertTrue(value.startswith('"') and value.endswith('"'), f"{path.name}: quote the {key}")

    def test_the_tile_manifest_is_the_one_the_company_site_reads(self) -> None:
        import json

        manifest = json.loads((ROOT / "docs" / "icl-app.json").read_text(encoding="utf-8"))
        self.assertEqual(manifest["version"], 1)
        self.assertEqual(manifest["family"], "games")
        self.assertLessEqual(len(manifest["name"]), 40)
        self.assertLessEqual(len(manifest["summary"]), 120)
        self.assertLessEqual(len(manifest.get("tags", [])), 4)
        self.assertEqual(manifest["icon"], "assets/img/icon.svg")

    def test_the_documentation_that_stays_in_the_repository_is_not_published(self) -> None:
        # Every Markdown file of docs/ is either in the guide, one of the pages, or excluded from the site by mkdocs.yml.
        config = (ROOT / "mkdocs.yml").read_text(encoding="utf-8")
        excluded = set(re.findall(r"^  (/?[\w./-]+)$", config.split("exclude_docs: |")[1].split("strict:")[0], re.M))
        pages = {"index.md", "features.md", "faq.md", "privacy.md", "support.md", "whats-new.md", "accessibility.md", "press.md"}
        for path in sorted((ROOT / "docs").glob("*.md")):
            self.assertTrue(path.name in pages or path.name in excluded or f"/{path.name}" in excluded,
                            f"docs/{path.name} is not a page of the site and is not in exclude_docs of mkdocs.yml")
        for name in sorted(excluded):
            self.assertTrue((ROOT / "docs" / name.lstrip("/")).exists(), f"mkdocs.yml excludes docs/{name.lstrip('/')}, which does not exist")


@needs_jinja
class Hooks(unittest.TestCase):
    def test_data_app_is_added_once(self) -> None:
        hook = load_hook("site_shell_hook")
        once = hook.add_app('<!doctype html><html lang="en"><head></head></html>')
        self.assertIn('<html data-app="cubeulator" lang="en">', once)
        self.assertEqual(hook.add_app(once), once)

    def test_a_page_with_no_html_tag_raises(self) -> None:
        with self.assertRaises(ValueError):
            load_hook("site_shell_hook").add_app("<p>no page</p>")

    def test_the_icons_the_pages_use_are_where_the_hook_copies_them_from(self) -> None:
        hook = load_hook("site_assets_hook")
        for name in hook.ICONS:
            self.assertTrue((ROOT / "docs" / "design" / "identity" / name).is_file(), name)


if __name__ == "__main__":
    unittest.main()
