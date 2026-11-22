# Architecture

## Source layout

```
src/
  lib/            cube:: — GUI-free core: solver (3x3), nxn (the other sizes),
                  vision, diagnostics, profile, graphics (the choice of
                  graphics API); i18n is a placeholder
    variants/       the headers CMake chooses between: cube/profiling.hpp
                    (Tracy on, Tracy off, or the in-process recorder) and the
                    reader's channel kernels (simd-generic, simd-vector128)
    fuzz/           the fuzz harnesses, with their seeds and regressions
  app/
    qt/            Qt 6 Quick/QML shell, shared by every platform
    desktop/        the shell's start-up manners (window geometry, keep-awake,
                    process identity, the graphics-API guard)
    i18n/           the language list and the translation catalogues
    platform/       one directory per OS: its implementations of each
                    interfaces/ header, and the platform.cmake that selects them
    interfaces/     camera.hpp, paths.hpp, platform_info.hpp,
                    crash_handler.hpp, desktop_shell.hpp, graphics_probe.hpp
tests/
  unit/
  integration/
  system/         draws the real app in a window
  perf/           budgets on the solver and the reader
  i18n/
  platform/       test cases only one OS can run
  web/            Playwright tests of the browser build
tools/            cube_scan, cube_solve_bench, cube_nxn_bench, the table
                  generators, the release scripts and the CI checks
```

`src/lib/` is Qt-free and platform-free by design: independently unit-testable,
reusable behind any shell. Platform differences are isolated to one directory
per OS under `src/app/platform/`; `cmake/Platform.cmake` picks the directory
once, and its `platform.cmake` names the files compiled and the libraries
linked. No source file, QML file or other CMake file branches on the
platform — see [Platform isolation](platform-isolation.md) for the rule, how a
platform is added, and the CI check that holds it.

## C++23 patterns in use

- **`Result<T> = std::expected<T, CubeError>`** ([error.hpp](../src/lib/include/cube/error.hpp))
  for every fallible operation: an expected failure comes back as a value the
  caller checks (or composes with `and_then`/`transform`/`or_else`), not as an
  exception.
- **`std::mdspan`** views of a cube's stickers (`cube/nxn/cube.hpp`,
  `cube/nxn/state.hpp`), which is why the toolchain has to ship `<mdspan>`
  ([Getting started](getting-started.md#requirements)).
- Build-time git version stamp (`cmake/GenerateVersion.cmake` +
  `src/lib/include/cube/version.hpp.in`), re-run on every build so it tracks
  new commits without a reconfigure.

## GUI shell strategy

Every platform — desktop (Windows/Linux/macOS), mobile (Android/iOS) and the
browser (WebAssembly) — shares the same Qt 6 Quick/QML shell
(`src/app/qt/`). There is no native-per-platform shell split: Android is
packaged via `androiddeployqt`, iOS via Qt's Xcode integration, and the web
build is Qt for WebAssembly's one `.wasm` module, all loading the same
`Main.qml`. The desktop packages, the browser build and the iPhone app (sent to
TestFlight: [iPhone build, signing and releasing](ios-signing.md)) come with a
release; CI also builds Android as an APK, which is not released. How each is
built is in [Building & packaging](building.md).

## What's implemented today vs. planned

`src/lib/`'s solver and vision modules are both real, and wired
together end to end. `cube::solver::solve()` solves a cube via the classic
beginner (layer-by-layer) method - cross, white corners, middle edges,
2-look OLL, 2-look PLL - the same staged structure the app teaches. The
first two layers are found by targeted per-piece search; the last layer
composes the standard 2-look algorithms (edge flip, Sune, A-perm, U-perm),
which keeps every solve bounded to seconds for interactive mobile use (see
`src/lib/src/solver/`). The stages' moves are then tidied in one linear pass,
so no two neighbouring moves turn the same side - including across a stage
boundary, where a stage can end up with no moves at all
(`merge_adjacent_moves`). That is `SolveMethod::Beginner`, the default; two
shorter methods sit beside it behind the same entry point - `Intermediate`
(CFOP, about 52 moves) and `Advanced` (Kociemba's two-phase search, about 20) -
described, with their measurements, in [Solve methods](solver-methods.md).
A cube of another size (the app offers 2x2 to 5x5, from *Another size of cube*)
is `cube::nxn`'s: its own scan assembly, solution plan and 3D mesh, beside the
3x3's.

`cube::vision` reads a cube one side at a time: `read_side` finds the N x N
sticker lattice of a side held roughly square to the camera and samples its
colours (nine on a 3x3), `read_corner` does the same for two or three sides
that touch in one view, `SideScan` groups the reads into sides by their own
content, and `resolve_colors` and `cube::solver::assemble_scanned_faces` (for a
3x3; `cube::nxn` has its own) turn six sides into a cube (see
`src/lib/src/vision/` and `src/lib/src/solver/scan_assembly.cpp`). There is no
pose tracker and no learned model. The image pipeline end to end - capture, the
photometry lock, the reader, the side grouping, and where colour decisions are
made - is described in [Vision pipeline](vision-pipeline.md), together with its
known limits.

The scan controller (`src/app/qt/controllers/direct_scan.hpp`) reads the
cube's sides with `cube::vision::read_side`, and once six sides make exactly
one valid cube hands a `cube::solver::Cube` to
`src/app/qt/solve_controller.hpp` (`solveCube`), which solves it on a worker
thread. Which of the three methods it uses is `SolveController.method`,
chosen on the solve screen and in settings (`SolveMethodPicker.qml`) and
kept by `SettingsState.solveMethod`; changing it mid-solve solves on from
the cube as it stands, off the UI thread, and keeps the hold the camera has
seen (`solutionContinuesCube`). The screens word each stage by its
`cube::solver::Stage` value, since the methods have different stages. A scan
that does not yet make a valid cube is never solved: the scan screen keeps
asking for the sides again, and *Check the colours* opens `ColoursScreen.qml`,
which shows every sticker that was read, lets a person correct them by tap or
drag (the same screen is where a cube is typed in a colour at a time), and keeps
Solve off until the colours can be a real cube. The solve screen draws the cube in 3D
(`Cube3DView.qml`, or `CubeNView.qml` for another size): the layer the next move
turns nudges a little way in the direction it has to go and back (`guideFace`),
and stepping turns it all the way (`playMove()`). `CubeView.qml`, the flat
technical-drawing cube, is what the completion screen draws. `cube::i18n` in
`src/lib` is still a placeholder (`not_yet_implemented()`); translations are
loaded by the app shell's `LanguageManager` (`src/app/qt/i18n/`), described in
[Translations](i18n.md).

Still open from the [Recovery plan](recovery-plan.md): the live gate, fresh
scrambles at the desk from camera to correct solve steps with no manual fix.
The other two items it listed are in the app: the review and hand-entry screen
(the colours screen above) and reading a side that is not held square-on
(`quad_fit.cpp` places the lattice through the side's own four corners).

## GPU rendering

There is one renderer, and it is Qt's. The app shell's own 3D cube
(`src/app/qt/Cube3DView.qml` for a 3x3, `CubeNView.qml` for another size; shown
on the launch screen, in Settings and on the solve and solution screens) and its
three render styles - Physically-Based Realism, Instructional Glow, Borderlands
Cel-Shading - are implemented entirely in Qt Quick 3D. Quick 3D's own
`CustomMaterial`/`Effect`/`ExtendedSceneEnvironment` extensibility covered all
three (hard-banded cel shading, inverted-hull ink outlines via a front-face-culled
duplicate mesh, and a halftone/vignette post-processing pass); the flat
technical-drawing cube is the QML Canvas in `CubeView.qml`. Which graphics API
runs underneath (Direct3D 11/12, Vulkan, OpenGL, Metal) is Qt's RHI. Cubeulator
chooses it once at launch, from the command line, the environment, the Settings
screen or the platform's own chain, and guards the choice against a machine that
cannot use it: see [Graphics API](graphics-api.md).

The progress overlay and step animation that were once pencilled in for a
separate bgfx pipeline are UI animation, which belongs in QML (move animation is
`Cube3DView.playMove()`). bgfx itself, a declared vcpkg dependency that nothing
linked, was removed ([Dependency strategy](dependency-strategy.md#bgfx-removed)).
If custom GPU drawing is ever needed inside the Qt Quick scene, the supported
route is `QQuickRhiItem` (Qt Quick, in the Qt 6.12 install), which draws with
Qt's own RHI instead of bringing a second graphics abstraction and swap chain.
