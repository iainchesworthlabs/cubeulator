# Licensing: inventory, the Qt finding, options

Written 2026-10-13 for workstream E0b of the
[app experience plan](app-experience-plan.md). Status: **reviewed; two decisions
taken, see below.** The facts were re-checked on 2026-11-22 against 0.2.0-beta.4
(the note after the decisions says what changed).

> **Decision (the maintainer, 2026-10-13): skip the iOS App Store and the Mac App Store
> for now and deal with them later.** Cubeulator stays GPL-3.0-or-later and
> ships on the desktop (direct download, Linux packages) and, when mobile
> capture exists, Google Play and F-Droid. This is Option A below. Nothing in
> the build or the notices changes: the Apple-store question stays open in
> [`LICENSE-NOTE.md`](../LICENSE-NOTE.md) and returns when an Apple store
> becomes a goal. It is the only reason a Qt commercial licence (Option B)
> would be needed.

> **Decision (the maintainer, 2026-10-30): the source is GPL-3.0-or-later, the
> binaries are GPL-3.0-only.** Qt Quick 3D, which the app links, is available
> only under GPL-3.0-only or a commercial licence (point 1 of the summary), so
> a program built with it is distributed under version 3 only. The source files
> keep "or later", which is the licence anyone who builds without Qt Quick 3D
> would have. The About screen, the RPM's licence field and
> [`LICENSE-NOTE.md`](../LICENSE-NOTE.md) say so. Qt Quick 3D stays.

> [!NOTE]
> **Facts re-checked on 2026-11-22, at 0.2.0-beta.4.** The analysis and the two
> decisions stand. The facts under them were brought up to date, and each change
> is marked where it is made.
>
> - **Re-read, and found to hold:** Qt Quick 3D is `Commercial OR GPL-3.0-only`
>   and Shader Tools' runtime library has an LGPL option, in the headers and the
>   SBOMs of the Qt 6.12.0 install ([section 3.1](#31-qt-quick-3d---holds)); the
>   ports and their versions, against the current baseline
>   ([section 2.4](#24-vcpkg-ports)); what a Windows build generates
>   ([section 6](#6-how-the-notices-are-produced)); and the statement that the
>   source is GPL-3.0-or-later and the programs are GPL-3.0-only, against the
>   About text, the RPM's licence field, `vcpkg.json`, `REUSE.toml` and the
>   manifest.
> - **Changed since it was written:** the About and Licences screens exist, but
>   cannot be reached on a phone
>   ([section 7](#7-about-and-licences-screens-the-requirement));
>   Archivo and IBM Plex Mono are bundled; the iPhone app builds and goes to
>   TestFlight; the vcpkg baseline moved to a commit of the labs' fork; the
>   shaders are five files, not three.
> - **Not re-read:** the Qt, FSF, VideoLAN and pricing pages and the Qt FAQ (the
>   2026-10-13 readings stand, and prices may have moved); Qt's Linux SBOMs,
>   which have not been read; and the ports' copyright files.
> - **Still open:** the four DLLs that `windeployqt` copies without a notice
>   ([section 2.5](#25-per-platform-differences-that-change-obligations)); Noto
>   Sans Hebrew and Arabic, planned and not bundled
>   ([section 2.3](#23-vendored-models-design-assets)); and the Apple-store
>   question.

This is an engineering inventory and a decision paper. It is not legal advice,
it does not change Cubeulator's licence (GPL-3.0-or-later), and it does not
settle the question [`LICENSE-NOTE.md`](../LICENSE-NOTE.md) leaves open. That
decision is the maintainer's.

How to read the evidence marks:

- **[V]** verified against a primary source, cited inline.
- **[I]** inferred - from the verified facts, from practice, or from a
  source that is not authoritative. Treat as a lead, not a finding.
- **[L]** a lawyer would need to confirm before anyone relies on it.

## 1. Summary

1. **The plan's Qt reading holds for Qt Quick 3D, and is wrong for Qt Shader
   Tools.** Qt Quick 3D is offered only as *Commercial or GPL-3.0-only*;
   there is no LGPL option [V]. Qt Shader Tools' *runtime library* is offered
   under LGPL-3.0 (or GPL-2.0/GPL-3.0, or commercial); only its `qsb`
   command-line tool is GPL-3.0-only, and that tool is a build-time program
   that does not ship [V]. See [section 3](#3-the-qt-question).
2. **Cubeulator's own code is not the obstacle.** The maintainer is the only author in
   the history (three identities, one person) [V: `git shortlog -sne`], so they
   can add a store exception, dual-license, or re-license it. The obstacle is
   that one Qt module the app links, Quick 3D, reaches the user under
   GPL-3.0-only and its copyright is The Qt Company's, not the maintainer's. A store
   exception on Cubeulator's own code does not change the terms Quick 3D's
   code arrives under [I][L].
3. **Qt is the only copyleft dependency that matters for the stores.** Every
   other shipped third-party component is permissive (MIT, BSD, Apache-2.0,
   Zlib, ISC, libpng, IJG), file-level weak copyleft (MPL-2.0: the public
   suffix list bundled in Qt Network), or notice-only [V: inventory below].
   Until the `highgui` and `gtk` OpenCV
   features were dropped, vcpkg's LGPL GTK3 chain reached Linux through
   them (see [section 2.4](#24-vcpkg-ports)); it no longer does.
4. **Routes for the App Store**, in the order this paper recommends them (full
   analysis in [section 5](#5-options)): (a) do not put iOS on the first
   release, and take the decision with a quote in hand; (b) a Qt commercial
   licence - Qt for Small Business is EUR 546 per developer per year while
   revenue stays under 1 MEUR/USD [V]; (c) draw the cube without Quick 3D,
   which moves the question from GPL to LGPL but does not remove it, because
   The Qt Company's own page says app stores may have rules that conflict
   with the LGPL [V]. Desktop and Google Play are not blocked by anything
   found here [I][L].
5. **The notices are generated, not written.** `THIRD-PARTY-NOTICES.txt` and a
   structured JSON are produced by the build from the app's resolved link
   line, vcpkg's installed tree and two checked-in manifests; a CTest case
   fails when something that ships has no notice
   ([section 6](#6-how-the-notices-are-produced)).
6. **Two things in the plan to correct** ([section 8](#8-things-in-the-plan-that-were-wrong-or-missing)):
   Qt Shader Tools, and the claim that iOS Qt linking is static (not
   confirmed by the Qt documentation I could read).

## 2. Inventory

Version numbers are from the vcpkg baseline (`9eec002d`, a commit of the labs'
fork of 2026-10-17, set by pull request #267; the first version of this paper
read `5b698638`, of 2026-07-15) as installed for a Windows build, and Qt 6.12.0.
They were rechecked on 2026-11-22 against the installed tree of a 2026-10-30
build and the port manifests at the current baseline: every version below
matches except fmt's, which is `12.2.0#1`.
"Ships" means "is part of an installed or distributed app on that platform";
the generated notices file is the authoritative per-build answer, this table
explains it.

Platform key: **W** Windows, **M** macOS, **L** Linux, **A** Android,
**I** iOS. The iPhone app builds only through the `ios-device-*` presets (the
`ios-clang-*` presets set `CUBEULATOR_BUILD_APP=OFF` and build the library
alone) [V: `CMakePresets.json`], and it goes to TestFlight, not to the release
page. The
browser build (WebAssembly, preset `web-emscripten-*`, [the web
build](web.md)) is a sixth platform whose set is smaller; it is described in
[section 2.5](#25-per-platform-differences-that-change-obligations) and is not
one of the letters.

### 2.1 Cubeulator itself

| component | licence | notes |
|---|---|---|
| Cubeulator | GPL-3.0-or-later [V: `LICENSE`, `vcpkg.json`] | Sole author: the maintainer [V: `git shortlog`]. First commit 2026-07-16, so the copyright line is "Copyright (C) 2026 Iain Chesworth". Once Quick 3D is linked the *combined binary* can only be conveyed under GPL-3.0 (Quick 3D is `-only`); the source files remain "or later". |
| `src/app/qt/shaders/*.frag` | first-party | Written against Qt's custom-material model; no Qt code copied [V: file headers]. |

### 2.2 Qt 6.12.0

> **Qt 6.12.0 update.** The build now uses Qt 6.12.0 (this section was first
> written against 6.8.3), and `third_party/notices/qt-modules.json` was
> regenerated from its SBOMs, for Windows and Android as the 6.8.3 snapshot
> was, and later for macOS as well (`tools/licences/qt_sbom_manifest.py`;
> `--check` agrees with the live Windows and Android SBOMs). What was
> re-read, and what was not:
>
> - **Licences did not move.** No existing component's licence expression
>   changed. The module and plugin expressions are the same three as before,
>   and Quick 3D is still `Commercial OR GPL-3.0-only`, so the GPL-3.0-only
>   election in section 3 stands. [V: 6.12.0 SBOMs]
> - **The component set changed**: 317 components for Windows and Android
>   (39 added, 9 removed; 337 once macOS was read in, below, and 343 with the iOS and
>   WebAssembly kits).
>   `QuickEffectsPrivate`, `QuickShapesPrivate` and `QuickParticlesPrivate` lost
>   their `Private` suffix (same DLL names), so `deployedRoots` in `manifest.json`
>   names `QuickEffects` and `QuickShapes` now. New attributions include
>   `tlexpected`, `emoji-segmenter`, `xsvg` and `yoga`.
> - **One licence id is new to the snapshot**: `Apache-2.0 WITH LLVM-exception`,
>   on qdoc's attribution for LLVM's clang headers. qdoc is a documentation
>   tool, the component is tagged Windows-only and nothing depends on it, so it
>   is not part of what ships.
> - **Re-read on 2026-11-22**: the source headers and the two SBOMs of the
>   6.12.0 install ([section 3.1](#31-qt-quick-3d---holds)). The Qt
>   documentation pages cited in sections 2.2 and 3 are the ones read on
>   2026-10-13 and were not fetched again.
> - **Unchanged and still open**: `dxcompiler.dll`, `dxil.dll`,
>   `d3dcompiler_47.dll` and `opengl32sw.dll` are copied beside the executable
>   by the deployment tool and have no `deployedFiles` entry (section 2.5). They
>   were deployed under 6.8.3 too.

Per-module SPDX expressions below are from the SBOMs Qt ships in its own
install (`<qt>/sbom/*.spdx.json`; the 6.8.3 ones were compared with the
`SPDX-License-Identifier` lines in the installed headers [V], the 6.12.0 ones
are what the snapshot is built from). The Qt Company's documentation says the
same for Quick 3D and Shader Tools (see section 3).

| module (as linked or imported) | linked by | offered under | ships |
|---|---|---|---|
| Qt Core, GUI, Concurrent, Network, OpenGL | `cube_app*`; Network and OpenGL are dependencies of Qml and Quick | Commercial OR LGPL-3.0 OR GPL-2.0 OR GPL-3.0 | W M L A I |
| Qt Qml, Quick, Quick Controls 2 (+ Templates 2), QmlCore (`QtCore`'s `Settings`) | `cube_app`, QML imports | same | W M L A I |
| **Qt Quick 3D** (Quick3D, Quick3DRuntimeRender, Quick3DUtils, Quick3DHelpers, Quick3DHelpersImpl) | `cube_app`; `import QtQuick3D`, `QtQuick3D.Helpers` in `Cube3DView.qml` | **Commercial OR GPL-3.0-only** | W M L A I |
| Qt Shader Tools (runtime library) | pulled in by Quick 3D (`Quick3DRuntimeRender` DEPENDS_ON `ShaderTools`) [V: SBOM]; the custom materials bake `*.frag` at run time | Commercial OR LGPL-3.0 OR GPL-2.0 OR GPL-3.0 | W M L A I |
| `qsb` (the shader baker), `moc`, `rcc`, `uic`, `lrelease`, `lupdate`, `windeployqt`, `androiddeployqt` | build tools (`lrelease` and `lupdate` through `find_package(Qt6 ... LinguistTools)` for the translations) | Commercial OR GPL-3.0 **with the Qt GPL exception 1.0** | no - build time only; the exception covers their output |
| Qt EntryPoint (`Qt6EntryPoint.lib`, Windows `WinMain` shim) | `WIN32_EXECUTABLE` | Commercial OR BSD-3-Clause | W |
| Qt Svg (+ `qsvg`, `qsvgicon` plugins) | deployed by `windeployqt`, not linked or imported [V: found by the notices test on the real deploy directory] | Commercial OR LGPL-3.0 OR GPL-2.0 OR GPL-3.0 | W (and wherever `windeployqt`/`androiddeployqt` add it) |
| Quick Controls 2 styles (Basic, Fusion, Imagine, Material, Universal, FluentWinUI3, Windows) and the Qt platform, image-format, TLS and network-information plugins | deployed by `windeployqt` because `QtQuick.Controls` is imported | same LGPL set; the Material style carries an MIT attribution (Angular Material shadows); the Windows platform plugin carries wintab's `LicenseRef-Lcs-Telegraphics` | W |

The Qt Company's bundled third-party code (zlib, libpng, harfbuzz, freetype,
PCRE2, double-conversion, md4c, tinycbor, Unicode CLDR, glslang, SPIRV-Cross,
Embree, tinyexr, xatlas, meshoptimizer, and several smaller attributions) is
listed per module in the SBOMs. The build reads a condensed snapshot
(`third_party/notices/qt-modules.json`) and emits one notice per component
reached from the modules above. The snapshot is generated from the Qt 6.12.0
SBOMs for **Windows, Android and macOS** [V: 2026-10-14; the macOS ones from
the `clang_64` kit's `sbom/` folders]. Platform differences among them:
Embree, D3D12 memory allocator, the Windows zlib wrappers and wintab are
Windows-only; `forkfd` is Android-only; the Cocoa plugin, the Darwin
permission plugins and `qeventdispatcher_cf` are macOS-only; and Qt GUI
depends on QtDBus on macOS but not on Windows [V]. Edges that exist on only
some platforms are kept per platform (`platformDepends`) so a macOS-only
dependency is not applied to Windows. **Linux has not been read**,
so on that platform the snapshot over-includes rather than guesses (the
WebAssembly kit has been read: `web` is in `verifiedPlatforms` since
2026-10-14, and the iOS kit's SBOMs since 2026-10-21; see the known gaps in
[section 6](#6-how-the-notices-are-produced));
`tools/licences/qt_sbom_manifest.py` re-derives it from any Qt install's
`sbom/` directory, and maps each library file name (`libqcocoa.a`,
`qwindows.dll`) to its component from the SBOM's own list of packaged
binaries, so plugin libraries no longer need hand-written aliases.

Obligation summary for the Qt set, *as distributed under GPL-3.0-only*: the
combined work is conveyed under GPL-3.0 with its Corresponding Source (which
includes the Qt libraries shipped with it, since they are not System
Libraries) or a written offer, the licence text, and no further restrictions
(GPLv3 sections 5(c), 6, 10 [V: `LICENSE` lines 208-342, 463]).

### 2.3 Vendored, models, design assets

| component | licence | where the text lives | ships | obligation |
|---|---|---|---|---|
| Lucide icon shapes (`Icon.qml`: camera, globe, menu, sun, refresh) | ISC, (c) Lucide Icons and Contributors [V: Lucide `LICENSE`] | `licences/ISC.txt` + the copyright line | all | notice must appear in all copies. `Icon.qml` also draws a `cross` (two crossing lines, added on 2026-10-14 for History) that neither this row nor the next names; the manifest entries cover the whole file, and where the shape comes from was not recorded |
| Feather-derived shapes (chevron-left, chevron-right, check, alert-triangle, help-circle) | MIT, (c) 2013-present Cole Bemis [V: same file lists these as Feather-derived] | `licences/MIT.txt` + the copyright line | all | notice |
| Archivo and IBM Plex Mono (`src/app/qt/fonts/`, on every platform but the browser); Noto Sans Hebrew and Noto Sans Arabic (planned for the same folder and not bundled: the files are only on `origin/feature/ax-kit`, which has not been merged) | OFL-1.1 [V: each family's OFL text, vendored with the fonts] | `OFL-Archivo.txt` and `OFL-IBMPlexMono.txt` beside the fonts (and `OFL-NotoSansHebrew.txt` and `OFL-NotoSansArabic.txt` when those come); the manifest's `licenseFile` points at it | Archivo and IBM Plex Mono: **yes**, compiled in under `:/fonts` and registered at start-up (the browser does not bundle them, but its notices list them too: the scan is by tree, not by platform). Their entries, with the full OFL text and the copyright line, are in the embedded `:/licences/third-party-notices.json` that the Licences screen lists, and `tests/unit/bundled_fonts_test.cpp` (`[licences]`) fails if a font the app registers is not in it, and `tests/unit/licences_screen_test.cpp` and `tests/system/licences_test.cpp` hold the screen to listing every one of them (About > Licences, `LicencesScreen.qml`), each opening to its full text. The Noto Hebrew and Arabic entries are dormant until the files are added to `src/app/qt/fonts/` (they were not there on 2026-11-22) | OFL: licence text travels with the fonts, no sale of the fonts alone, reserved font names ("Plex"). Copyright lines are copied from the OFL texts: Archivo (c) 2020 The Archivo Project Authors; IBM Plex Mono (c) 2017 IBM Corp.; Noto Sans Hebrew and Arabic (c) 2022 The Noto Project Authors. **Any font file under `assets/` or `src/app/qt/` that no emitted manifest component claims is an unresolved error**, so a new font cannot ship unnoticed |
| Noto Sans JP and Noto Sans SC, cut down to the characters the translations and the language names use (`src/app/platform/web/fonts/`; `tools/fonts/subset_cjk.py`) | OFL-1.1 [V: `OFL-NotoSansJP.txt` and `OFL-NotoSansSC.txt` there, and the licence field of each subset's own name table] | `OFL-NotoSans<JP,SC>.txt` beside the subsets; the manifest's `licenseFile` points at it | the **browser build only**, compiled in under `:/fonts` (a browser has no system fonts); the desktops use the system's faces and bundle nothing | OFL: licence text and the copyright line travel with them: (c) 2014-2021 Adobe, Reserved Font Name "Source". The subsets are modified (cut down, unhinted) and keep the names Noto Sans JP and Noto Sans SC, which do not contain the reserved name, as the OFL requires. Provenance and hashes are in `src/app/platform/web/fonts/README.md`. The manifest entries are limited to the `web` platform, so they are emitted for that build and no other |

The vendored M3T tracker (MIT, with `filesystem.h`, nanoflann and
tiny_obj_loader inside it), the corner-detector weights
`cube_corner_pose.onnx` (AGPL-3.0, derived from Ultralytics YOLO11n-pose, never
distributed) and the MediaPipe palm detection model (Apache-2.0, via the
OpenCV Zoo, with its unfilled licence template) were listed here until
recovery plan R6 removed the tracking pipeline and the hand detector.
Nothing embeds or links any of them now. The manifest holds no model. The weights stay in `.gitignore`;
a copy left in an old checkout is a file under `assets/` that no component
claims, which the generator reports as unresolved. There is no `assets/`
directory in the repository now (the models are gone, and the fonts planned
for `assets/fonts/` went to `src/app/qt/fonts/` and `src/app/platform/web/fonts/`),
and `third_party/` holds only the notices themselves; the checks that guard
both stay in place for the day either grows a file.

### 2.4 vcpkg ports

Windows list at the baseline; licences from each port's `vcpkg.json`, with the
text from its installed `share/<port>/copyright` [V]. Ports marked **[T]** are
tools/tests only. Rechecked on 2026-11-22: the sixteen ports in the
`vcpkg/status` of a 2026-10-30 Windows build are the ones below, with the
versions below, and the 11 that ship are the 11 vcpkg components of that
build's generated notices; the copyright files were not read again. The
platform column is read from `vcpkg.json` (the codec features are `!ios`, and the
browser build asks OpenCV for `intrinsics` alone, see 2.5), not from a build on
each platform.

| port | version | SPDX | ships | why / obligation |
|---|---|---|---|---|
| opencv4 | 4.14.0 | Apache-2.0 (port says `null`; the copyright file is Apache-2.0 plus SoftFloat and a CHI-table notice) | all (the browser build is smaller, see 2.5) | Apache: licence copy; keep the bundled notices in its copyright file. Features: calib3d, core, fs, intrinsics, jpeg, png, quirc, thread, tiff, webp, + dshow/msmf (W), v4l (L). `highgui`, `gtk` and `win32ui` are not requested (see the Linux-specific finding below) |
| libjpeg-turbo | 3.2.0#1 | BSD-3-Clause AND IJG (the port's field at the current baseline) | W M L A (not iOS, not the browser) | notice in docs |
| libpng | 1.6.59 | libpng-2.0 | W M L A (not iOS, not the browser) | notice; no misrepresentation |
| libspng | 0.7.4#1 | BSD-2-Clause AND libpng-2.0 | W M L A (not iOS, not the browser) | notice (OpenCV 4.14's PNG codec) |
| tiff | 4.7.2 | libtiff | W M L A (not iOS, not the browser) | notice |
| libwebp | 1.6.0#3 | BSD-3-Clause | W M L A (not iOS, not the browser) | notice |
| liblzma | 5.8.4 | 0BSD (liblzma) | W M L A (not iOS, not the browser) [I: it arrives with tiff's `lzma` feature] | none (0BSD); the copyright file notes LGPL getopt only for the CLI tools, which are not built |
| zlib | 1.3.2#2 | Zlib | all | none for binaries beyond not misrepresenting |
| quirc | 1.2 | ISC | all but the browser | notice |
| fmt | 12.2.0#1 | MIT | all (`cube_lib`) | notice |
| bgfx (+ libsquish, lodepng, mgnlibs, miniz, stb, tinyexr) | 1.129.8940-496#1 | BSD-2-Clause AND CC0-1.0 | **removed from `vcpkg.json`** - it was declared but nothing linked it [V: the file API's link line of `cube_app` had no bgfx; `src/render` was a stub] | none |
| cpu-features | 0.11.0 | Apache-2.0 [V: port manifest] | A | OpenCV's dependency on Android only; Apache: licence copy |
| catch2 **[T]** | 3.16.0 | BSL-1.0 | no | tests only |
| tracy **[T]** | 0.14.1 (overlay port-version 2) | BSD-3-Clause | **yes, on the desktop** | the `profiling` feature (`CUBEULATOR_ENABLE_TRACY`, on by default on Windows, Linux and macOS): the client library is linked into the desktop applications (`TracyClient.dll` on Windows), dormant until the application is started with `--profile` / `CUBEULATOR_PROFILE=1`. The notices pick it up from the link line with its licence text. Its command-line tools (the separate `profiling-tools` feature, which brings `capstone`, `ppqsort`, `zstd`) are host tools a shipped build does not select; see [Profiling](profiling.md#licence) |
| vulkan-headers | 1.4.363.0 | Apache-2.0 OR MIT | no | header-only, a `profiling` feature dependency on Windows and Linux (Tracy's Vulkan context is compiled against it); nothing is linked, so nothing is distributed |
| vcpkg-cmake, vcpkg-cmake-config, vcpkg-get-python-packages | - | - | no | install nothing outside `share/`; the generator skips ports with no `include/`, `lib/` or `bin/` content |

ONNX Runtime and the ports only it pulled in (onnx, abseil, flatbuffers,
dlpack, protobuf, utf8-range, re2, cpuinfo, eigen3, the boost headers, date,
cxxopts, nlohmann-json, ms-gsl, safeint, optional-lite, wil) were listed here
until R6 removed the hand detector. The generator lists whatever the link
line reaches, so none of them appears now.

**Linux-specific finding [V: port manifests; vcpkg install plan] - resolved.**
`vcpkg.json` used to ask for `opencv4[highgui]` and, on Linux,
`opencv4[gtk]`; the `gtk` feature made vcpkg build `gtk3` and a chain of 55
more ports (`vcpkg install --dry-run --triplet=x64-linux`: 98 ports planned,
42 after the change, none added; measured before ONNX Runtime was removed): GTK3, GLib, Pango, ATK, gdk-pixbuf,
fribidi, gettext (all LGPL-2.0/2.1+), Cairo (LGPL-2.1 or MPL-1.1), D-Bus
(AFL-2.1 or GPL-2.0+), plus the X11 libraries. The only code that included
`opencv2/highgui.hpp` was the vendored M3T, which R6 removed; the app,
`cube::lib` and the tools include only `core`, `imgproc`, `imgcodecs` and
`videoio` (`vcpkg.json` still requests `calib3d`, which nothing includes). The
`highgui`, `gtk` and `win32ui` features were
therefore dropped from `vcpkg.json`. `win32ui` is only a `highgui` window
backend, and the camera backends the app opens (DirectShow and Media
Foundation on Windows, V4L on Linux) belong to `videoio`, which depends on
`imgproc` and `imgcodecs` and not on `highgui` [V: OpenCV 4.12.0
`modules/videoio/CMakeLists.txt` and `modules/highgui/CMakeLists.txt`]. The
Linux plan is now the table above: no GTK, GLib, Pango or X11 ports.

### 2.5 Per-platform differences that change obligations

| platform | what differs |
|---|---|
| Windows | Qt DLLs and vcpkg DLLs ship beside the exe (`windeployqt`; vcpkg's applocal step in a development build, `cmake/InstallRuntimeDependencies.cmake` in the installer). On the 2026-10-14 Qt 6.12.0 build the deployment directory held 38 Qt modules (and QtTest, which the system tests copy there) and 40 Qt plugins and QML plugins, all attributed (13 more, the QML debugger and profiler servers, are a development build's: the installer removes them), **plus four binaries that are neither Qt's nor vcpkg's**: `d3dcompiler_47.dll` and `dxcompiler.dll` + `dxil.dll` (Microsoft's DirectX shader compilers, redistributable under the Windows SDK terms [I]) and `opengl32sw.dll` (Mesa llvmpipe software OpenGL, MIT-family [I]). `windeployqt` copies them by default. They are reported as a generator warning, not an error, because they are not linked dependencies. **Decision for E4a/the maintainer:** drop them with `windeployqt --no-opengl-sw --no-system-d3d-compiler --no-system-dxc-compiler` (the app does not need a software GL fallback or the DirectX compilers if the graphics backend is Direct3D 11/OpenGL on a real GPU), or keep them and add notices - `opengl32sw.dll` needs a Mesa notice, which has not been sourced. Still open at 0.2.0-beta.4 (checked 2026-11-22): the deployment call, `cube_deploy_qt_on_install(... NO_COMPILER_RUNTIME)` in `packaging/windows/ConfigureApp.cmake`, passes none of those options. The installer also carries the MSVC C++ runtime DLLs beside the exe (`InstallRequiredSystemLibraries`, redistributable under Microsoft's terms for Visual Studio [I]); the generated notices do not list them. |
| macOS | The `.dmg` (Apple Silicon) is built by CI and by the release workflow (`macos-clang-release`), and Qt's macOS SBOMs are read into the snapshot; the licence and `THIRD-PARTY-NOTICES.txt` are at the root of the disk image. Not run on a Mac by this paper. Mac App Store adds Apple's terms (section 4). |
| Linux | No GTK3 chain: `opencv4[gtk]` is not requested (above). Qt is installed from `jurplel/install-qt-action` in CI, and the DEB and RPM carry it: the app, Qt, its plugins and QML modules and OpenCV sit in a private directory (`/usr/lib/cubeulator`), and the packages declare only system libraries as dependencies (`SHLIBDEPS` and `AUTOREQPROV` are off; `packaging/linux/Packaging.cmake`), so Qt's source and notice duty is the packager's, not a distribution's [I][L]. The licence and notices are in `/usr/share/doc/cubeulator/`. Qt's Linux SBOMs have not been read, so the Linux notices over-include. |
| Android | Qt is shared (`.so` inside the APK/AAB) so LGPL relinking is mechanically possible [I]. The camera backend is a stub today. |
| iOS | The app builds for an iPhone since 2026-10-21 (`docs/ios-signing.md`) and, since 0.2.0-beta.2, goes to TestFlight for the maintainer's internal testers, not to the release page ([Cutting a release](releasing.md#the-iphone-signing-material)). Qt for iOS is normally linked statically [I - **not stated** in the Qt iOS documentation pages I read; Qt's own tracker has an open request to allow dynamic builds, QTBUG-85974]. The app is linked with Qt's own libjpeg and libpng, which the Qt kit carries, and with none of vcpkg's image codecs: `vcpkg.json` leaves OpenCV's `jpeg`, `png`, `tiff` and `webp` features out on `ios`, because two copies of the same C library in one static link are mixed object by object, and the first TestFlight build crashed in `jpeg_start_decompress` when Qt's JPEG reader called into the other's structures. |
| Web (WebAssembly) | One `.wasm` module with Qt linked in statically and nothing deployed beside it (no installer and no CPack package; a release ships the site as `Cubeulator-<version>-web.zip`, assembled by `packaging/web/make-package.sh` with `LICENSE.txt` and `THIRD-PARTY-NOTICES.txt` beside it, and the script stops on an unresolved notice because no installer is there to refuse; the notices are also embedded in the module as a resource like every build's). From vcpkg it takes fmt and a smaller OpenCV: `vcpkg.json` asks for the `intrinsics` feature alone on `emscripten`, and `cmake/triplets/wasm32-emscripten-threads.cmake` builds only `core`, `imgproc`, `imgcodecs` and `videoio`, so none of the image-codec ports (libjpeg-turbo, libpng, tiff, libwebp, quirc, liblzma) is in it; zlib comes in as OpenCV's own dependency [V: `vcpkg.json`, port manifests]. It also carries the two Noto subsets (2.3) and, from Qt's WebAssembly kit, the DejaVu Fonts 2.37 face the browser build draws Latin, Hebrew and Arabic with (Bitstream-Vera licence [V: `QWasmIntegrationPlugin_Attribution_dejayvu` in the kit's `qtbase` SBOM]). That kit's SBOMs are in `qt-modules.json` (`web` is a verified platform), so the attribution is in the generated notices; see the known gaps in [section 6](#6-how-the-notices-are-produced). |

## 3. The Qt question

**Claim under test (plan, decision 5):** Cubeulator links Qt Quick 3D and Qt
Shader Tools, which Qt publishes under GPLv3 or a commercial licence, not
LGPL.

### 3.1 Qt Quick 3D - holds

Four independent sources agree:

1. **Qt's licensing page, Qt 6.12 documentation.** The section "Modules
   available under GNU General Public License v3" lists Qt Quick 3D (and Qt
   Canvas Painter, CoAP, Graphs, GRPC, HTTP Server, Lottie Animation, MQTT,
   Network Authorization, Qml Compiler, Quick 3D Physics, Quick Timeline,
   Virtual Keyboard, Wayland Compositor). <https://doc.qt.io/qt-6/licensing.html> [V]
2. **The module's own page, Qt 6.12 and 6.8.** "Qt Quick 3D is available under
   the commercial license from The Qt Company. In addition, it is available
   under the GNU General Public License, version 3." LGPL is not mentioned.
   <https://doc.qt.io/qt-6/qtquick3d-index.html>,
   <https://doc.qt.io/qt-6.8/qtquick3d-index.html> [V]
3. **The 6.8.3 source headers.** 59 of the 60 headers in `include/QtQuick3D`
   and every header of `QtQuick3DRuntimeRender`, `QtQuick3DUtils`,
   `QtQuick3DAssetUtils` and `QtQuick3DHelpers` carry
   an SPDX licence identifier of `LicenseRef-Qt-Commercial OR GPL-3.0-only`; the one
   exception in each is the generated `*exports.h` (for example
   `qtquick3dexports.h`), which carries the LGPL-set identifier because it is
   boilerplate. Checked with `grep -rho` over
   `C:/Qt/6.8.3/msvc2022_64/include/*` [V].
4. **The 6.8.3 SBOM** (`sbom/qtquick3d-6.8.3.spdx.json`): `Quick3D`,
   `Quick3DRuntimeRender`, `Quick3DUtils`, `Quick3DAssetImport`,
   `Quick3DHelpers`, `Quick3DParticles`, `Quick3DEffects`, `Quick3DXr` and all
   their QML plugins have `licenseDeclared` = `LicenseRef-Qt-Commercial OR
   GPL-3.0-only`; the import tools carry `... GPL-3.0-only WITH
   Qt-GPL-exception-1.0` [V].

**Re-read on the 6.12.0 install, 2026-11-22** (the one the app is built with).
In `include/`, 64 of the 65 headers of `QtQuick3D`, 79 of 80 of
`QtQuick3DRuntimeRender`, 17 of 18 of `QtQuick3DUtils`, 21 of 22 of
`QtQuick3DHelpers` and 6 of 7 of `QtQuick3DAssetUtils` carry
`LicenseRef-Qt-Commercial OR GPL-3.0-only`; the one in each that does not is the
generated `*exports.h`, as in 6.8.3. All 8 headers of `QtShaderTools` carry the
LGPL set. In `sbom/qtquick3d-6.12.0.spdx.json`, `Quick3D`, `Quick3DRuntimeRender`,
`Quick3DUtils`, `Quick3DParticles` and the rest of its libraries and plugins are
`LicenseRef-Qt-Commercial OR GPL-3.0-only`, and its tools (`balsam`, `shadergen`
and others) add `WITH Qt-GPL-exception-1.0`. In `sbom/qtshadertools-6.12.0.spdx.json`
the `ShaderTools` library is `LicenseRef-Qt-Commercial OR LGPL-3.0-only OR
GPL-2.0-only OR GPL-3.0-only` and `qsb` is `LicenseRef-Qt-Commercial OR
GPL-3.0-only WITH Qt-GPL-exception-1.0` [V].

A search result suggested a distribution package for Qt 6.11 lists LGPL-3.0
for `qt6-quick3d`. I read that as the union of licences found in the whole
repository (the same header is LGPL-licensed in 6.8.3), not as a licence
change, because the Qt 6.12 pages above still say GPL-or-commercial [I].

**Also noted:** the licence is `GPL-3.0-only`, not "or later". Cubeulator's
source files stay "or later", but the combined binary can be conveyed only
under GPL-3.0 [V: SPDX id].

### 3.2 Qt Shader Tools - does not hold

- **Runtime library `ShaderTools`:** *Commercial OR LGPL-3.0-only OR
  GPL-2.0-only OR GPL-3.0-only* [V: SBOM `qtshadertools-6.8.3.spdx.json`; the
  6.8 documentation: "Since Qt 6.3, these free software licenses are the GNU
  Lesser General Public License, version 3, or the GNU General Public License,
  version 2" <https://doc.qt.io/qt-6.8/qtshadertools-index.html>].
- **`qsb` command-line tool:** *Commercial OR GPL-3.0-only WITH
  Qt-GPL-exception-1.0* [V: same two sources]. It is a build-time tool and is
  not shipped. Cubeulator's own build does not call it (there is no
  `qt_add_shaders`): the five `.frag` snippets in `src/app/qt/shaders/` are
  compiled in as resources and baked at run time by the ShaderTools library,
  which is why that library ships [V: `src/app/CMakeLists.txt`;
  `Cube3DView.qml`]. Were a build to call `qsb`, the Qt GPL exception allows
  the output of the tool to be distributed under terms of one's choice, so
  long as the work is not otherwise derived from the tool [I: from my
  recollection of the exception's text; the Qt page lists "Qt GPL exception
  1.0" for its tools, but the exception file itself was not available
  locally] [L].
- It is **not** on Qt's list of GPL-only modules [V: page above].

So Shader Tools adds no copyleft beyond what Quick 3D already forces. Its
bundled **glslang** carries a licence set that includes GPL-3.0-or-later *with
the Bison exception 2.2* (for generated parser files), which lets the output
be distributed under any terms; its notice must still be shipped [V: SBOM].

### 3.3 What else in Qt is GPL-only for this app

Nothing. Of the modules the app links or imports, Quick 3D and its helper
modules are the only ones without an LGPL option [V: section 2.2].

### 3.4 What it implies, combined with `LICENSE-NOTE.md`

`LICENSE-NOTE.md` frames the open question as GPLv3 against Apple's store
terms, for Cubeulator's own code. The Qt finding adds a second, separate
copyright holder to that question.

- **Cubeulator's own code** is the maintainer's alone. GPLv3 lets a copyright holder add
  additional permissions to material they own (section 7: "You may place
  additional permissions on material, added by you to a covered work, for
  which you have or can give appropriate copyright permission" [V:
  `LICENSE` lines 357-359]), and a copyright holder is not bound by their own
  licence when distributing their own work. The maintainer can add a store exception,
  dual-license, or re-license it [I][L].
- **Qt Quick 3D's code** is The Qt Company's. It reaches every recipient
  under GPL-3.0-only unless they hold a commercial licence. Section 10 says
  the distributor "may not impose any further restrictions on the exercise of
  the rights granted" [V: `LICENSE` line 463], and the FSF's published
  position is that Apple's store terms impose such restrictions on any GPL
  code distributed through the store (the "Usage Rules" limit installation to
  a fixed set of devices, and apply "in addition to" any licence the developer
  supplies) [V: FSF, B. Smith, 26 May 2010, written about GPLv2 section 6;
  the FSF says the analysis applies to all versions]. Apple removed GNU Go in
  2010 after the FSF complained; VideoLAN's VLC for iOS went the same way and
  returned in 2013 as a rewrite under MPL-2.0 plus GPL-2.0-or-later, with the
  copyright holders' agreement [V: FSF article; VideoLAN press release,
  reported by Softpedia/Slashdot].
- **GPLv3 added a second hook:** section 6 requires "Installation
  Information" - "any methods, procedures, authorization keys, or other
  information required to install and execute modified versions of a covered
  work" - when object code is conveyed in a "User Product" [V: `LICENSE`
  lines 297-327]. A phone is a consumer product. iOS only runs code signed
  by a key the user does not hold unless they enrol in Apple's programme; the
  extent to which that satisfies the clause is a legal question [L].

**Is a store exception on Cubeulator's own code enough? No** [I][L]. The
exception can only grant permissions over the maintainer's own material (section 7).
The binary Apple would distribute also contains Quick 3D, whose licence
terms the maintainer cannot vary. The recipient receives Quick 3D's code under
GPL-3.0-only, so the conflict the FSF identifies is present for those
parts whatever the maintainer says about Cubeulator's own code. The exception would make Apple's
distribution of *Cubeulator's* code unobjectionable to *its author*; it would not stop The
Qt Company (or any Qt contributor) from objecting about *theirs*. The same
reasoning is why a commercial Qt licence solves it: it is the instrument by
which the owner of that code grants different terms.

Whether The Qt Company would ever object is unknowable; this paper does not
rely on forbearance. The commercial licence page is also clear that GPL-only
parts "require your application or device to be licensed under GPL" [V: Qt
LGPL-obligations page, "Modules Not Available Under LGPL"].

## 4. The distribution channels

| channel | what stands in the way, if anything | status |
|---|---|---|
| **Direct download** (GitHub releases: NSIS installer, DMG, DEB, RPM; the packages are not yet OS code-signed or notarised, see [Cutting a release](releasing.md)) | Nothing beyond GPL-3.0 itself: ship licence, notices, source or an offer | clear [I] |
| **Web** (the app as static files under `/app/` of the company site, iainchesworthlabs.com/cubeulator/, [the web build](web.md)) | Not examined in a primary source. The program, with Qt linked in, is sent to the visitor's browser, so the same GPL-3.0 terms are assumed to apply as to a download [I][L]. The release's zip carries the licence and the notices, and the app in it has the About and Licences screens (since 0.2.0-beta.1; the browser build is not `is_mobile`, so Settings > About Cubeulator is there), which name the licence and the source address and list the embedded notices (section 7). Whether the hosted page itself shows a licence or a source link was not re-checked (sections 2.5 and 6) | **open** [L] |
| **Microsoft Store** | Not examined in a primary source here. GPL apps appear in it [I]. MSIX packaging replaces the NSIS installer. | [L], research needed |
| **Mac App Store** | Same Apple terms as iOS (below) plus the sandbox; a notarised DMG outside the store avoids Apple's *store* terms entirely | blocked by the same question as iOS [I][L] |
| **Linux** (tarball, DEB, RPM, Flathub, Snap) | Nothing found. The project's own DEB and RPM carry Qt (section 2.5); distros that package it supply Qt themselves | clear [I] |
| **Google Play** | No contradiction found. The FSF's objections were to Apple's Usage Rules and DRM, not to Play; Play App Signing means Google holds the signing key, but Android lets a user install their own modified build (sideload), which is what the Installation Information clause asks for [I]. I found no official Google statement on GPLv3. | probably clear [I][L] |
| **F-Droid** | Builds from source and signs with its own key; designed for GPL apps [I] | clear [I] |
| **TestFlight** (the iPhone app, internal testers) | Not examined in a primary source. In use since 0.2.0-beta.2: the app goes to the maintainer's internal testers, not to the App Store, and a release carries no iPhone file. Option D (section 5) lists TestFlight among the routes expected to be possible [I][L]; it is not the store submission the next row is about. The About and Licences screens cannot be reached on an iPhone (section 7) | in use; **not confirmed** [I][L] |
| **Apple App Store (iOS)** | The FSF analysis above; Qt's own page warns that stores "may have rules that are in conflict with LGPL" [V]; Quick 3D is GPL-3.0-only [V]; Installation Information on a locked device [L] | **open - needs a decision** |

## 5. Options

Costs are 2026-10-13 list figures from The Qt Company's own pages [V] and may
have moved; get a written quote.

### Option A - take no decision now; leave iOS off the first release

The plan already orders Windows, macOS and Linux first, Android next, iOS last
(decision 4), and the iOS app does not build today. The first store submission
the plan needs is desktop and, later, Google Play.

- **Cost:** nothing. **Consequence:** keeps GPL-3.0-or-later and Quick 3D
  as they are; no iOS until a route below is chosen.
- **Risk:** none new. Revisit when mobile capture exists (decision 8).
- **Does not cover:** the Mac App Store (the same Apple terms).
- *Since written (2026-11-22):* the iPhone app builds (0.2.0-beta.1) and goes to
  TestFlight (0.2.0-beta.2), so "the iOS app does not build today" no longer
  holds. The Apple stores are still skipped.

### Option B - a Qt commercial licence

- **What it is:** Qt for Application Development, or Qt for Small Business.
  Small Business is **EUR 546 (about USD 618) per developer licence per
  year**, 12-month terms renewed by hand, "max 1 MEUR/USD" annual revenue or
  funding, at most three licences per company bought online, standard
  support limited to five tickets a year, "apps for desktop and mobile"
  included (the Device Distribution licences that embedded products need are
  not required while eligible); eligibility is audited [V: <https://www.qt.io/pricing/qt-for-small-business>].
  Larger-company pricing is a quote.
- **What it covers:** all Qt modules, Quick 3D included, "under a single,
  commercial Qt license" [V: licensing page]. The cube stays in Quick 3D;
  nothing in `src/app/qt` changes.
- **What it asks of the code:** nothing technical. Cubeulator's own code can
  then be distributed under whatever terms the maintainer chooses for the store build:
  GPL-3.0-or-later publicly, plus a store exception or a proprietary licence
  for the store binary (the maintainer owns it). Third-party permissive components still
  need their notices (the generator covers them).
- **Catches to confirm [L]:** (1) The Qt Company's FAQ states that "moving
  code developed under an open source edition to a commercial license
  requires written consent from The Qt Company" [V: <https://www.qt.io/faq>];
  Cubeulator appears to have been developed on the open-source edition (Qt is
  installed with aqtinstall [V: `C:/Qt/aqtinstall.log`]). (2) A commercial
  licence for the store *binary* does not cure GPL obligations the *public*
  GPL release already carries, and the two releases need separate build
  paths. (3) Whether Qt Small Business covers a project whose source is
  public under GPL and that is distributed for free.
- **Consequence for LICENSE-NOTE.md:** it becomes the "dual-licensing"
  option the note lists, with Qt as the part that costs money.
- **Cost:** about USD 600 a year while eligible; a quote above that.

### Option C - draw the cube without Quick 3D

The cube is drawn today by `Cube3DView.qml` and, for the other sizes,
`CubeNView.qml` (both Quick 3D) and by `CubeView.qml` (Canvas 2D). When this paper was written a bgfx stub under `src/render` was
listed here as a third route; it held no rendering code and was removed
(pull request #53), so there is no second renderer to switch to. Without Quick 3D
the app would link only modules that have an LGPL option [V: section 2.2].

- **What it does for the licence:** the Qt set becomes available under
  LGPL-3.0 (or still GPL). Cubeulator's own GPL does not need to change.
- **What it does *not* do:** it does not make Apple's store terms compatible
  with Qt's LGPL. The Qt Company's own LGPL page says "online application
  stores, may have rules that are in conflict with LGPL" and that those
  stores "cannot be used with the LGPL option" [V]; the same page says that
  with static linking the application "may no longer be 'work that uses the
  library'" and may become subject to the LGPL, recommending that one "link
  dynamically, or provide the application source code to the user under LGPL"
  [V]. The LGPL still requires that a user can relink with a modified Qt:
  LGPLv3 4(d) offers (0) the application's object files so the user can
  recombine, or (1) a shared-library mechanism, and 4(e) the Installation
  Information where the GPL would require it [V: gnu.org LGPL-3.0, via
  summary]. On iOS the library is normally static [I], so (0) applies, and
  the Installation Information question is the same one as for GPL [L].
  Cubeulator's source being public makes the object-file duty easy to meet;
  the signing-key question does not go away.
- **Rendering alternatives, none verified here:** keep `CubeView.qml` (2D
  Canvas, already exists); a custom scene-graph item, of which
  `QQuickRhiItem` is the public, supported one (in Qt Quick, which has an LGPL
  option [V: section 2.2]; its header is in the Qt 6.12 install [V], and it
  arrived in Qt 6.7 [I]); bgfx via `QQuickRenderControl` (bgfx is BSD-2-Clause
  [V]; it would be added back from vcpkg, and needs its own device and swap
  chain beside Qt's RHI [I]); or Qt 3D, which is not on Qt's GPL-only list but
  whose maintenance status I did not check [I].
- **Cost:** engineering. It means replacing `Cube3DView.qml`, `CubeNView.qml` and the cel
  shaders, and throws away the shader work.
- **Consequence:** unblocks nothing on iOS by itself, and costs more than
  Option B. It does remove the GPL-only dependency, which matters if the
  project ever wants a non-GPL desktop licence without paying Qt.

### Option D - do not ship on the App Store

The cleanest on licence terms. GPL apps are distributable to iOS users as
source for self-build and sideload, or via TestFlight to named testers, and
on Android via Play and F-Droid [I][L]. The cost is audience.

- **Consequence for LICENSE-NOTE.md:** its question disappears for iOS and
  macOS stores; "intended to ship to app stores" is narrowed to Google Play.

### Option E - other routes found

1. **Dual-license only Cubeulator's own code, ship it with Option B for the
   store build.** This is Option B stated from the code's side; the repository
   needs a contributor policy (a CLA or an explicit "all contributions are
   under a licence permitting this") before anyone else contributes [I][L].
2. **Re-implement the 3D view in a native iOS layer (SwiftUI/Metal) with Qt
   only for the non-3D shell.** Does not remove Qt from the binary; not
   recommended.
3. **A notarised DMG and a Windows installer** cover the desktop audience
   without any store [I].

### Recommendation

**A now, B when iOS or the Mac App Store becomes a goal, C only if B's cost or
the consent point ([L] above) blocks it.** Concretely:

1. Do not let the licensing question block E0-E5. Nothing in the plan's
   desktop or Android work is affected.
2. Before E6 (release readiness), ask The Qt Company's sales team for a
   written answer to two questions: whether the open-source edition's code
   may move to a commercial licence (the consent clause) and whether Qt for
   Small Business covers a free app whose source is public. Their answer
   settles Option B.
3. If B is yes, record it in `LICENSE-NOTE.md` as the resolution (dual
   licence: GPL-3.0-or-later publicly, with a store exception, plus a Qt
   commercial licence for store builds). If B is no, take Option D for Apple
   and revisit C.

This is a recommendation about engineering order and cost, not a legal
conclusion.

## 6. How the notices are produced

```
 vcpkg installed tree  ──┐                  third_party/notices/
 (status, info/*.list,   │                    manifest.json      hand-kept: icons,
  share/<port>/copyright,│                                       fonts, elections
  ports/<port>/vcpkg.json)│                   qt-modules.json   generated from Qt's SBOMs
                          ├─► cube_third_party_notices ──►      licences/<SPDX>.txt
 CMake file API: cube_app │      (cmake -P, before cube_app links)
 resolved link line      ─┤                       │
 QML `import`s            │                       ▼
 (src/app/qt/*.qml)      ─┘     <build>/third_party_notices/third-party-notices.json
                                <build>/third_party_notices/THIRD-PARTY-NOTICES.txt
                                       │                       │
                       qt_add_resources(cube_app)         install(FILES ...) → CPack
                       :/licences/*.json,*.txt            (refuses while unresolved)
```

- **What counts as shipped** is derived, not listed: the app's *resolved link
  line* from the CMake file API (so every static or shared library on the
  real command line, per platform and configuration), its QML imports, and the
  closure of each over vcpkg's `Depends` and Qt's SBOM edges. A vcpkg port
  that installs nothing outside `share/` is a build helper and is skipped.
  Nothing is added by hand for a vcpkg port: when R6 removed onnxruntime,
  M3T, GLEW and GLFW, and bgfx went with the render scaffolding, they and
  their dependency chains disappeared.
- **A static Qt links objects, not libraries.** The browser build (Qt for
  WebAssembly) puts every QML plugin's registration object and every module's
  resource bundle on the link line as a `.o` file, which no SBOM lists by
  name. They are attributed by where they sit under the Qt prefix
  (`cn_qt_object_key` in `cmake/ThirdPartyNoticesGenerate.cmake`:
  `qml/<module path>/objects-<config>/<plugin>_init/` is that plugin,
  `<Target>_resources_<n>/` is that module's library), against the library
  keys `qt-modules.json` lists. An object that matches no pattern is
  *unresolved*, so a change in Qt's layout is a failure to look at.
  `notices_static_qt_selftest` runs the generator over such a link line.
- **What is checked by hand** is only what the build cannot know:
  `manifest.json` lists icons and fonts (and is where vendored code or a model
  would be listed; there is none now), the licence elections (Qt taken under
  GPL-3.0-only; FreeType under FTL), and a table of licences for ports whose
  manifest says `license: null`.
- **The test** (`third_party_notices_complete`, label `licensing`) re-runs the
  generator with the deployment directory added - every Qt module, plugin and
  vcpkg DLL beside the executable, except the test-only Qt modules named in
  `qt.testOnlyModules` (QtTest, which `cube_system_tests` copies into the
  directory it shares with the app) - and fails if anything is unresolved, if a
  component has no licence or text, if a licence text the build needs is not
  in `third_party/notices/licences/`, or if the deployment holds a component
  the embedded notices do not list. It also fails for: a file under `assets/`
  or a directory under `third_party/` that no manifest entry claims; a QML
  `import` with no Qt module mapped; a linked Qt library missing from
  `qt-modules.json`; and a Qt version outside the one the manifests were
  written for.
- **Installing** (`cmake --install`, therefore CPack) runs a check and
  refuses to package while the JSON's `unresolved` list is non-empty, so an
  incomplete notice cannot ship even if the test was not run. This is the
  desktop packages only: the browser build has no install step.
- **The release SBOM** is made from the same JSON, not by scanning the
  installers: the release workflow keeps each platform's
  `third-party-notices.json` and `tools/release/make_sbom.py` turns the three
  into one SPDX 2.3 document (and, from it, a CycloneDX 1.6 one), refusing an inventory with anything unresolved
  ([Cutting a release](releasing.md#what-a-release-holds)). It is only as
  complete as these inventories are.

Maintaining it:

| change | what to do |
|---|---|
| add a vcpkg dependency | nothing; it is picked up. A port with `license: null` needs a `portLicences` line. |
| vendor code under `third_party/` or add a file under `assets/` | add a `components` entry (or `ignoredFiles`) in `manifest.json` |
| add a QML `import` of a new Qt module | add it to `qt.qmlImports`; the module may need to be added to `qt-modules.json` |
| a test copies another Qt module's library beside the executables | add its component id to `qt.testOnlyModules` in `manifest.json`, with the test that copies it; the deployment scan then ignores the file. The attributions bundled in that module are not components either: they ship only if the module does. |
| upgrade Qt | `python -I tools/licences/qt_sbom_manifest.py --sbom <platform>=<qt>/sbom --out third_party/notices/qt-modules.json`, review the diff, update `qt.expectedQtVersion` |
| add the bundled fonts | add the files at the paths the planned manifest entries name (`src/app/qt/fonts/`; the web build's faces are in `src/app/platform/web/fonts/`); the entries start emitting; verify their copyright lines against the fonts. Done for Archivo and IBM Plex Mono; the Noto Hebrew and Arabic files are not in the tree yet (they are on `origin/feature/ax-kit`) |
| add a platform (Linux; iOS has been read since 2026-10-21) | run the Qt SBOM tool on that platform's Qt install (the browser build's kit, `wasm_multithread`, carries its SBOMs too, and `web` is verified since 2026-10-14) and add `--sbom <platform>=<dir>`; its components are over-included until then (the JSON says `qtPlatformVerified:false`). macOS: the kit's SBOMs are inside the `qtbase`, `qtdeclarative`, ... `.7z` archives under `sbom/`; extracting only that folder avoids aqtinstall's symlink step on Windows |

Known gaps, so nobody mistakes them for done:

1. **Licence texts are complete for the Windows closure**, but five of the
   texts were *typed* rather than copied (MIT, BSD-2-Clause, BSD-3-Clause,
   ISC, Zlib - the standard SPDX wording without a copyright line), so a
   reviewer should diff them against <https://spdx.org/licenses/> once. The
   provenance of every text is in `third_party/notices/licences/README.md`.
   Nine texts had to be fetched (FTL, Unicode-3.0, AML-glslang,
   Bison-exception-2.2, X11, HPND, Imlib2, MIT-open-group, HPND-sell-variant);
   they were downloaded from the SPDX licence-list repository with the maintainer's
   approval on 2026-10-13. `IJG` is the "LEGAL ISSUES" section of
   libjpeg-turbo's `README.ijg`, from the source vcpkg unpacked locally.
2. **Only Windows was exercised against a real build here.** A local Windows
   build of 2026-10-30 (Qt 6.12.0, a fastdebug build) generated 81 components:
   11 vcpkg ports (fmt, libjpeg-turbo, liblzma, libpng, libspng, libwebp,
   opencv4, quirc, tiff, tracy, zlib), 6 Qt entries, 60 Qt-bundled components,
   the two icon sets and the two fonts (Archivo, IBM Plex Mono), with 25 licence
   texts and nothing unresolved; the notices file is 443 KB and the JSON 455 KB.
   The earlier result (2026-10-14, Qt 6.12.0, a RelWithDebInfo build): 77 components -
   9 vcpkg ports, 6 Qt entries (4 repositories + Qt SVG + EntryPoint),
   60 Qt-bundled third-party components and the two icon sets - with 25
   licence texts, nothing unresolved, one generator warning (the four files
   above). The notices file is 220 KB and the JSON 228 KB (on 2026-10-13, with
   Qt 6.8.3 and the tracker and detector still in the tree, it was 102
   components). Linux, macOS and Android use the same mechanism, but
   `qt.deployedRoots` has no list for them yet (the generator says so in a
   warning); CI is where they get checked, because the test runs on every
   desktop leg. Linux has not had its Qt SBOM read, so its Qt components are
   over-included.
   The Windows build used an existing vcpkg installed tree, configured with
   `VCPKG_MANIFEST_INSTALL=OFF`. The libraries are the ones the manifest
   resolves to at the pinned baseline (their versions match the port
   manifests there), but a clean vcpkg install was not run end to end for this
   figure; CI's legs do that.
3. **The deployment scan covers shared-library files** (`.dll`, `.so`,
   `.dylib`). It does not look inside an Android APK/AAB or a macOS bundle's
   `Frameworks/`; E4a/E4b extend it when they define those layouts.
4. **Assets drawn in code are covered only by their manifest entry.** The
   Lucide and Feather icon shapes live in `Icon.qml`; nothing stops someone
   deleting the `lucide-icons` entry. Files under `assets/` and directories
   under `third_party/` are enforced; shapes copied into QML are not. A rule
   of review: any new vector or glyph data in QML gets a manifest entry.
5. **QML-only dependencies are by import name.** A QML module that is only
   loaded by string at run time would be missed; there are none today.
6. **The browser build's notices are held by its packaging script, not by
   the test above.** Its kit's SBOMs have been read (`verifiedPlatforms`
   includes `web`, and the snapshot has the WebAssembly platform plugin's
   DejaVu attribution), and the generator attributes a static Qt's plugin and
   resource objects (`notices_static_qt_selftest` runs it over such a link
   line). `third_party_notices_complete` is not built for the browser
   (`CUBEULATOR_BUILD_TESTS=OFF`), so what stops an unresolved notice from
   shipping is `packaging/web/make-package.sh`, which a release runs and CI's
   Web leg does not. The browser's two Noto subsets are in the manifest and do
   reach its notices (2.3).
7. **A font outside `assets/` and `src/app/qt/` is not looked for.** The scan
   that fails on an unclaimed font covers those two trees only; the web
   build's faces under `src/app/platform/web/fonts/` are claimed by manifest
   entries, but a font added anywhere else would ship unlisted.

## 7. About and Licences screens: the requirement

The screens were built in E3b and shipped in 0.2.0-beta.1. This section is what
they must show, with the reason for each. Object names follow the plan's
contract (every interactive item has an `objectName` and an accessible name).

> [!IMPORTANT]
> **Status at 0.2.0-beta.4 (checked 2026-11-22): built, in part.** The About
> screen (`AboutScreen.qml`) and the Licences screen (`LicencesScreen.qml`)
> shipped in 0.2.0-beta.1 (#235; the earlier placeholder About is gone). The
> Licences screen reads the notices embedded as `:/licences/*`, and
> `tests/unit/licences_screen_test.cpp` and `tests/system/licences_test.cpp`
> hold it to listing every component. Against the table in 7.1:
>
> | item | built |
> |---|---|
> | 1 Name and version | The name and `Version <n>`; not the commit hash (the `about.build` string exists, but the screen's own header says the build line is not built) |
> | 2 Copyright | Not on About, which says "Made by Iain Chesworth Labs." (the publisher). The Licences screen's first row, Cubeulator, shows `Copyright (C) 2026 Iain Chesworth` |
> | 3 No warranty | No |
> | 4 Licence grant | Yes, in the words of the 2026-10-30 decision, not the words in the table: "Free software under the GNU General Public License, version 3. Its source code may also be used under any later version." (`about.licence.binary`) |
> | 5 View the licence | In part: the Licences screen's first row opens the full licence text. About has no button of its own and no gnu.org address |
> | 6 Source | Yes: a Source code row opens the repository address |
> | 7 Third-party licences | Yes: the Licences row, with an introduction ("Cubeulator is built with these components. Each is used under its own licence.") |
> | 8 Qt statement | No line of its own: Qt is among the components in the Licences list |
>
> Privacy, the build line with Copy, and Copy details, which
> `docs/design/screens/about.md` specifies, are not built either.
>
> **Reachability.** About is opened from Settings > About Cubeulator on every
> platform, and also from the application menu on a Mac. The Settings row was
> hidden on a phone in 0.2.0-beta.4 and earlier (`visible: !AppState.isMobile` in
> `SettingsScreen.qml`), so neither About nor the Licences under it could be
> opened on the iPhone build, against `docs/design/platforms.md`. The rules below
> ask for items 1 to 6 within two taps from any screen; About is one tap from Settings, and Licences
> one more.
>
> **One thing to look at.** The Licences screen's own row names the licence
> `GPL-3.0-or-later` (`kLicenceId` in `about_info.cpp`, and `project.license` in
> `third_party/notices/manifest.json`), which is the source's, while the About
> paragraph names version 3 for the program.

### 7.1 The About screen

GPL-3.0 section 0 defines **Appropriate Legal Notices**: an interactive
interface must include "a convenient and prominently visible feature that (1)
displays an appropriate copyright notice, and (2) tells the user that there
is no warranty for the work (except to the extent that warranties are
provided), that licensees may convey the work under this License, and how to
view a copy of this License. If the interface presents a list of user
commands or options, such as a menu, a prominent item in the list meets this
criterion" [V: `LICENSE` lines 103-110]. Section 5(d) requires each
interactive interface to display them [V: line 230].

The screen reachable from the menu, in this order:

| # | content | text (English source; translatable) | why |
|---|---|---|---|
| 1 | Name and version | `Cubeulator <version>` and the build's commit hash (`cube/version.hpp`) | the source offer below must name *this* version |
| 2 | Copyright | `Copyright © 2026 Iain Chesworth` | s.0 (1) |
| 3 | No warranty | `This program comes with ABSOLUTELY NO WARRANTY; for details see the licence.` | s.0 (2); s.15-16 |
| 4 | Licence grant | `This is free software, and you are welcome to redistribute it under the terms of the GNU General Public License, version 3 or (at your option) any later version.` | s.0 (2) |
| 5 | View the licence | button opening the full GPL-3.0 text, offline (a Qt resource), with the gnu.org URL beside it | s.0 (2) "how to view a copy" |
| 6 | Source | `Source code: https://github.com/iainchesworthlabs/cubeulator` plus, when the build is a tagged release, the tag; selectable and openable | s.6 Corresponding Source / offer |
| 7 | Third-party licences | button to the Licences screen; one line: `Cubeulator uses Qt and other open-source software; see Licences.` | the other licences' notice duties |
| 8 | Qt statement | `This application uses Qt <version> under the GNU General Public License v3. Qt source: https://download.qt.io/official_releases/qt/` | Qt is conveyed in the package; its Corresponding Source must be offered [L] |

Rules:

- The **publisher string** (store listing, installer metadata, an optional
  "Published by" line) is "Iain Chesworth Labs"; the repository is
  `github.com/iainchesworthlabs/cubeulator`. The **copyright line** in item 2
  is a separate matter: it names the copyright holder, it is not a rename,
  and the repository's `LICENSE` is the plain GPL text with no holder line, so
  who is named there (the maintainer personally, or the Labs) is the maintainer's decision. Until
  that is decided, the generated notices keep "Copyright (C) 2026 Iain Chesworth".
- The text of items 3 and 4 is **not** to be paraphrased: it is the GPL's own
  recommended notice; changing it is a legal decision.
- Items 1-6 must be reachable in **no more than two taps/clicks from any
  screen** and offline.
- The same copyright, warranty, licence and source lines go in the store
  listing's description and the website, because that is where the user
  decides to install.
- If a store build is ever made under another licence (Option B), the About
  text is whatever that licence requires, and this table is revisited with
  the lawyer.

### 7.2 The Licences screen

Reads `:/licences/third-party-notices.json` (schema 1). Per component:

| field | shown as |
|---|---|
| `name`, `version` | row title |
| `license`, `distributedUnder` | licence id; when they differ, "offered under X, used under Y" |
| `copyright` | under the title, verbatim |
| `homepage` | link |
| `via` | "part of ..." |
| `text`, or `licenseRefs` resolved through top-level `licenseTexts` | on tap: the full text, scrollable |
| `notes` | small print |

Also: a header line stating the platform and Qt version the list was generated
for (`platform`, `qtVersion`); `THIRD-PARTY-NOTICES.txt` is shown or exported
as a whole on request; and nothing in the list is hidden by default.

The resource is, in outline (all strings UTF-8; `unresolved` is empty in any
build that can be packaged):

```json
{
  "schema": 1,
  "project": { "name": "...", "license": "GPL-3.0-or-later", "copyright": "...", "homepage": "...", "source": "..." },
  "platform": "windows", "qtVersion": "6.12.0", "qtPlatformVerified": true,
  "components": [
    { "id": "vcpkg/fmt", "name": "fmt", "version": "12.2.0",
      "license": "MIT", "distributedUnder": "MIT",
      "copyright": "", "homepage": "https://github.com/fmtlib/fmt",
      "kind": "vcpkg | qt | qt-bundled | vendored | model | design | font",
      "via": [], "licenseRefs": [], "exemptTexts": [], "missingTexts": [],
      "notes": "", "text": "<the port's copyright file, verbatim>" }
  ],
  "licenseTexts": { "GPL-3.0-only": "...", "MIT": "..." },
  "linked": [ { "input": "...", "kind": "vcpkg|qt|first-party|system|manifest|unresolved", "component": "..." } ],
  "unresolved": [], "warnings": []
}
```

A component's terms are its own `text` when present, otherwise the entries of
`licenseTexts` named by `licenseRefs`; the Licences screen shows the copyright
line first, then those texts.

### 7.3 Per-licence wording the notices must carry

The generator emits these automatically as each component's copyright line and
licence reference. This list exists so a reviewer knows what to look for.

| licence | what it requires in a binary distribution |
|---|---|
| GPL-3.0 (Cubeulator, Qt Quick 3D and the Qt set as elected) | licence text; Corresponding Source or a written offer valid for three years; Appropriate Legal Notices; no further restrictions; Installation Information for User Products |
| LGPL-3.0 (only if elected; not today) | licence text, a prominent notice that the library is used, ability to relink, source or offer |
| MIT, BSD-2/3-Clause, ISC, libpng, libtiff, IJG | the copyright line and licence text in the documentation |
| Apache-2.0 | licence copy, any NOTICE file, state significant changes |
| MPL-2.0 (Qt's psl-data) | notice, and say where the source of those files is |
| FTL (FreeType, if elected) | credit line in the documentation: "Portions of this software are copyright (c) <year> The FreeType Project (www.freetype.org). All rights reserved." |
| Unicode-3.0 | the copyright and permission notice with the data |
| Zlib, BSL-1.0, 0BSD, CC0-1.0 | nothing beyond not misrepresenting authorship (BSL-1.0: no notice in object code) |
| OFL-1.1 (fonts: the browser build's Noto Sans JP and SC subsets today; Archivo and IBM Plex Mono on the other platforms; Noto Hebrew and Arabic planned, not bundled) | licence copy with the fonts; reserved font names |

## 8. Things in the plan that were wrong or missing

1. **Qt Shader Tools** is not GPL-only; its runtime library has an LGPL
   option, and only the `qsb` tool is GPL-3.0-only (with a Qt GPL exception
   for its output) [V]. Quick 3D is the only GPL-only module the app links.
   The plan's three routes are otherwise right, with this correction: Shader
   Tools is not a reason to avoid a route.
2. **"Where linking is static" for iOS** is stated in the brief as a fact; the
   Qt iOS pages I read do not say it. It is the usual configuration of the
   prebuilt Qt for iOS and the 2014 Qt developer-list threads assume it, but a
   shared build is possible and tracked as an open request [I]. It changes
   which LGPL 4(d) option applies, not whether the store question exists.
3. **`cube_corner_pose.onnx` was embedded whenever the file existed**
   (`src/app/CMakeLists.txt`). A developer's local copy of the AGPL-derived
   weights would have been compiled into a package without any other check.
   Closed by R6: the corner detector is gone and nothing embeds the file.
4. **The CPack packages contained nothing**: no `install()` rule existed for
   `cube_app`, and the notices install rule was the only one, so a ZIP/NSIS
   package was just the notices and the licence. Closed by E4a: the NSIS
   installer, DMG, DEB and RPM carry the app, Qt, OpenCV, the licence and the
   notices, and `packaging/check-package.sh` asserts it
   ([Packaging](packaging.md)).
5. **The licence file beside the palm model was an unfilled Apache template**
   (`Copyright [yyyy] [name of copyright owner]`) and the repository did not
   record the holder or the download URL for the model. Closed by R6: the
   model and its licence file are gone.
6. **GTK3 and 55 LGPL/X11 ports used to reach Linux** through
   `opencv4[gtk]`, which existed to back `highgui`, which only the vendored
   M3T used. Resolved by dropping the features once M3T was removed
   (section 2.4).
7. **bgfx was a declared dependency nothing linked**; it would otherwise have
   looked like a shipped component. The inventory distinguishes declared from
   linked because the generator reads the link line. The declaration has since
   been removed from `vcpkg.json`.
8. **The plan's "Qt Quick 3D reading" is a known unknown**, listed under "What
   is not known" - this paper resolves it for Quick 3D (holds) and Shader
   Tools (does not).

## 9. What a lawyer should confirm

Not exhaustive, ordered by how much rides on it.

1. Whether Apple's App Store terms, as they stand today (not the 2010 text the
   FSF quoted), conflict with GPL-3.0 for Qt Quick 3D's code, and whether a
   copyright holder's own agreement (VideoLAN's route) is the only cure.
2. Whether Section 6's Installation Information duty is met on iOS by a user's
   ability to build and sideload with a personal Apple developer account.
3. Whether The Qt Company will agree to move open-source-edition code to a
   commercial licence for the store build (the FAQ's consent clause), and
   whether Qt for Small Business covers a free, publicly-sourced app.
4. Whether shipping Qt's official binaries inside a GPL-3.0 app requires the
   Qt source in the package, or whether a written offer pointing at
   `download.qt.io` meets GPL section 6.
5. Whether the Qt GPL exception 1.0 on `qsb` matters at all: the app's shaders
   are baked at run time by the ShaderTools library, not by `qsb` (section
   3.2). If a build ever calls `qsb`, whether the exception covers the baked
   shaders embedded in the app (I expect yes; the exception file was not
   available locally).
6. Whether the Linux DEB/RPM, which carry their own Qt under
   `/usr/lib/cubeulator` rather than depend on the distribution's, owe the Qt
   source and notices in the same way as the installers (item 4).
7. (Withdrawn: this asked about the OpenCV Zoo palm model's licence chain, and
   R6 removed the model.)
8. Microsoft Store: policy terms for GPL software (not examined here).
9. Whether a contributor licensing policy is needed before accepting any
   outside contribution, if Option B is chosen.
10. Whether the browser build, which sends the program with Qt linked in to the
    visitor's browser, owes a source offer, the licence and the notices on the
    page itself (section 4, Web).
11. Whether the TestFlight route (the iPhone app, internal testers only, in use
    since 0.2.0-beta.2) raises the same questions as the App Store for Quick 3D's
    code and for Section 6's Installation Information (items 1 and 2); section 5,
    Option D, expects that it does not [I].

## 10. Sources

Primary, read in this work:

- Qt Licensing (Qt 6.12), <https://doc.qt.io/qt-6/licensing.html>
- Qt Quick 3D (Qt 6.12 and 6.8), <https://doc.qt.io/qt-6/qtquick3d-index.html>,
  <https://doc.qt.io/qt-6.8/qtquick3d-index.html>
- Qt Shader Tools (Qt 6.8), <https://doc.qt.io/qt-6.8/qtshadertools-index.html>
- Qt LGPL obligations, <https://www.qt.io/development/open-source-lgpl-obligations>
- Qt for Small Business, <https://www.qt.io/pricing/qt-for-small-business>
- Qt FAQ (commercial licence consent), <https://www.qt.io/faq>
- Qt 6.8.3 install: `include/*` headers' SPDX lines and
  `sbom/*.spdx.json` (local, `C:/Qt/6.8.3/msvc2022_64`, and Android)
- Qt 6.12.0 installs: `sbom/*.spdx.json` for Windows, Android and macOS (the
  snapshot, `third_party/notices/qt-modules.json`) and for the WebAssembly kit
  (read for 2.5 only); on 2026-11-22, the SPDX lines of the `include/QtQuick3D*`
  and `include/QtShaderTools` headers and `sbom/qtquick3d-6.12.0.spdx.json` and
  `sbom/qtshadertools-6.12.0.spdx.json` of the Windows install
- GPL-3.0 text: this repository's `LICENSE`; LGPL-3.0, <https://www.gnu.org/licenses/lgpl-3.0.html>
- FSF, "More about the App Store GPL Enforcement" (B. Smith, 2010-05-26),
  <https://www.fsf.org/blogs/licensing/more-about-the-app-store-gpl-enforcement>
- VideoLAN, "VLC for iOS returns", <https://images.videolan.org/press/ios2out.html>
- Lucide `LICENSE`, <https://github.com/lucide-icons/lucide/blob/main/LICENSE>
- vcpkg port manifests and installed `share/<port>/copyright` at baseline
  `5b698638c97b4610044a254347d571f823e56557`; port manifests again at the
  current baseline `9eec002df1cd109f8f104cd966a763daf9751489`

Secondary or not authoritative (marked [I] where used): Qt wiki "Licensing
talk about mobile platforms" and the Qt development mailing list, July 2014
(iOS static linking and relinking); Qt forum threads on iOS and Google Play;
Qt bug QTBUG-85974; Arch Linux package metadata for `qt6-quick3d`.
