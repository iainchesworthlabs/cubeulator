# Performance

Where Cubeulator's time goes, measured with the [Tracy instrumentation](profiling.md)
on a real build and ranked by what a person waits for - what was done about the
worst of it, and what was left alone and why.

Everything here is one desktop. A phone has not been measured; where this page
says what a phone would see it is a multiple of a desktop figure, marked as an
estimate.

## Budgets

| Path | Budget | Where it comes from |
|---|---|---|
| One camera frame, `direct_scan.process_frame` | 33 ms, all of it, on the GUI thread | `DirectScan` polls at about 30 Hz (`kPollIntervalMs`); a pass longer than the interval delays the next frame and everything else on that thread |
| One solve, `solver.solve` | seconds on a phone, for every input | the solve screen waits for it; no search path may be unbounded (`solver.hpp`) |
| One solve of a cube of any size, `nxn::solve` | seconds on a phone, for every input; under a quarter of a second on a desktop at every size to 9x9 | the same: the solve screen waits for it, and the work is a fixed pipeline of table lookups with no unbounded search ([Cubes of any size](#cubes-of-any-size)) |
| The one-off setup for a size, `nxn::prepare(size)` | seconds on a phone, off the UI thread, while a person scans or enters the cube | the geometry, the cycle library and the centre search's actions are built once per size on first use; the solve itself runs on a worker already |
| Launch to a drawn first screen | no figure is committed; under about 2 s on a desktop is proposed here | nothing waits on it but the person |
| One UI frame | 16.7 ms (60 Hz) | the display |
| The models in the 3x3 cube | 80 at the Physically-Based and Glow looks, 106 at the Cel look; none of them switched off | a model costs creation, sync and memory whether or not it draws ([Screens](#screens)); held by `the cube makes the models it draws and none it hides` |
| 3D cubes made by the launch screen | 1, the layout on show, after its first frame | the same; held by `Home makes the one cube it shows once its menu has been drawn` |

## How the numbers were taken

- **Build.** `windows-msvc-profiling` (RelWithDebInfo, Tracy in on-demand
  mode), MSVC 19.51, Windows 11 x64, 32 cores. Qt 6.12.0, the version the
  application ships on, for everything that runs Qt.
- **Machine.** Shared with other builds, so wall time carries noise of tens of
  percent and the tails (p99, max) are not to be read. What holds is the
  ratio: before and after were run alternately on the same corpus, at High
  priority, with the same binary layout.
- **Capture.** `tracy-capture`, then `tracy-csvexport -u` (one row per zone
  event, the visited-node count as its value), the workflow in
  [profiling.md](profiling.md). Percentiles are over those events.
- **Solves.** `cube_solve_bench` (`tools/solve_bench`): seeds 0 to 27, 25-move
  scrambles. It prints a digest of every solution, so "the same solutions" is a
  comparison of one number.
- **The camera frame.** `cube_scan replay` over 200 frames of a locally
  recorded scan (the repository is public and recorded frames are never
  committed), at the recorder's 640-wide analysis size and at the 1920x1080 the
  camera delivered. In the app itself, the journey case of the system tests
  under Tracy: a real window, the fake camera's drawn 640x360 frames.
- **Startup.** The launch screen, four warm runs, medians, with timestamps
  taken in QML and C++ in experiment builds that are not committed.
- **Not measured:** the camera capture thread (no camera was involved) and a
  phone.

## Findings

Zone names are in [profiling.md](profiling.md#zones). Each row is `develop` as
it was when the measurement was taken.

### Pass of 2026-10-18 (feature/scan-edge-and-square base, Tracy 0.14.1)

Taken on this branch (`feature/tracy-profiling`, built on
`feature/scan-edge-and-square`) with the Tracy 0.14.1 upgrade, the first
capture after the render-thread and NxN instrumentation landed. Same machine
and method as below, so the numbers compare with the tables that follow.

#### The 3x3 solver

`cube_solve_bench`, Beginner, 28 solves, seeds 0-27, 25-move scrambles:

| Zone | Per solve | Share | |
|---|---|---|---|
| `solver.middle_edges` | 70 ms | 75% | |
| `solver.cross` | 13.7 ms | 15% | |
| `solver.corners` | 8.4 ms | 9% | |
| `solver.oll`, `.pll`, `.algorithm_search`, `.merge_adjacent_moves` | under 0.1 ms | 0% | |
| **`solver.solve`** | **96 ms** mean, p95 131, max 202 | | 95.9 ms mean unattached (bench digest `1bc470006f988409`) |

All of it is `solver.piece_search`: 22.8 million nodes over the corpus,
948 000 per solve, at **99 ns a node** - the 2.4 us a node of the old table
above is gone (the arena, the open addressing and the packed signatures).
What remains is the node *count*: `middle_edges` alone visits about
three quarters of them.

Opportunities, in order of value:

- **Do not expand a turn of the face the parent turned.** Two same-face
  quarter turns in a row are always redundant or reachable through the merged
  multi-turn, so skipping those children is provably safe and cuts the
  expansions by about a quarter (12 children to 9) - roughly 20 ms of a
  Beginner solve, for one line of pruning in `piece_search`.
- `map_codes` walks the packed codes through `FaceletMap::at()`, a
  bounds-checked `std::array` access, several times per node on a provably
  masked index; an unchecked read (with a comment and the perf test to hold
  it) is the other piece of the remaining per-node cost.
- The stage postconditions are checked by scanning `goal.keys` before the
  dedup insert; nothing found today, but a hash of the goal keys would be
  the next cut if the searches ever grow.

#### Cubes of any size

`cube_nxn_bench solve 2 9 10`, unattached: mean ms per solve at each size -
2x2 0.0, 3x3 19.9, 4x4 134, 5x5 173, 6x6 334, 7x7 412, 8x8 616, **9x9 742**.
A 9x9 is over the quarter-second budget this page states above; 6x6 to 9x9
all are. The new per-stage zones say where:

| Zone | Total over the run | Share |
|---|---|---|
| `nxn.centres` | 4771 ms | 66% |
| `nxn.core` (the 3x3 stage) | 2447 ms | 34% |
| `nxn.edges` | 38 ms | 1% |
| `nxn.plan`, `nxn.plan_edges`, `nxn.parity` | under 1 ms | 0% |

The centre solver (`solve_centre_stickers`) is the hotspot for big cubes and
grows with the cube; the wing-edge reduction is already negligible. The core
stage is the two-phase solver's fixed 39 ms and only matters at the sizes
where the centres are still cheap.

#### The render thread (new instrumentation)

A `CUBE_SOLVE_DEMO` session, 447 frames in 18 s on the solve screen:
`render.frame` mean 4.1 ms, but the first painted frames are **154 ms** -
startup painting, not QML loading (`app.load_qml`), is where the first
picture's cost sits, and it is the number to watch on slower hardware.
`render.frame_ms` and `capture.interval_ms` plots carry the cadences now, so
a capture shows frame gaps without exporting events.

GPU zones: on OpenGL (this box's software GL) 402 frames resolved, about
1-2 us each for the demo scene - the bracket works end to end and the GPU
timeline appears. On Direct3D 11 the context and its per-frame Collect run,
but the timestamps stalled after the first frames on this machine with no
Tracy diagnostic logged; that path is in place but is not yet trusted on
this box. Vulkan, Metal and D3D12 have no GPU zone (see
[profiling.md](profiling.md#the-render-pass-cpu-and-gpu)).

#### The baseline

`tools/perf/tracy_baseline.json` was re-baselined for this branch (33 zones;
the scan-edge "looks" added ten new ones, and `solver.assemble_all`,
`vision.direct_scan.scanned` and `vision.resolve_colors` measured 3-20x
faster than the old baseline's, which is the scan-edge work showing).

### The solver

The default method (`Beginner`), 28 solves, one capture.

| Zone | Per solve | Share | Visited nodes per solve | Cost per node |
|---|---|---|---|---|
| `solver.middle_edges` | 1818 ms | 76% | 758 000 | 2.4 us |
| `solver.cross` | 337 ms | 14% | 99 000 | 3.4 us |
| `solver.corners` | 219 ms | 9% | 129 000 | 1.7 us |
| `solver.oll`, `.pll`, `.merge_adjacent_moves` | under 0.1 ms | 0% | | |
| **`solver.solve`** | **2387 ms** (capture attached) | | 985 000 | |

Unattached, alternated with the fix below: **1.77 to 1.86 s** a solve, 3.0 s the
worst of 28. All of it is the breadth-first search over the pieces of the first
two layers. A visit cost microseconds because a node was a 54-sticker cube:
turning it copied the cube, its signature was a dozen `locate_*` scans, and
every visited signature was a heap allocation in an `unordered_set`.

The other methods are not a problem: `Intermediate` 2.3 ms a solve, `Advanced`
70 ms (and 40 to 110 ms, once, for its tables, which the app builds on a worker
at startup).

A cube that is physically valid and not solvable (a twisted corner, a flipped
edge) took **2.5 to 6 s** to say `no_solution`: three searches, each run to its
node budget. Nothing in the app produces one, but `solve()` is public and
promises bounded work on every input.

### The camera frame

200 frames of a scan in progress: about two thirds are a side held still, the
rest the cube being turned. p50 per frame.

| | 640 wide | 1080p |
|---|---|---|
| a side held still: `read_side` | 2.0 ms | 4.0 ms |
| the cube turned: `read_side` and then `read_corner` | **4.2 ms** (2.0 + 2.2) | **8.2 ms** (4.0 + 4.2) |

Inside `read_side` at 640 wide: the adaptive threshold 0.57 ms, the contours
0.74, the blur 0.16, the shape filter 0.15, the nine patches 0.14, the lattice
fit 0.03; at 1080p the area resize adds 1.25 ms. `read_corner` began with the
same resize, threshold and contours again, which is why it cost about what
`read_side` did. The rest of the per-frame work is small: `SideScan::add`
0.007 ms; assembling a cube (`vision.direct_scan.scanned`) 1 to 3 ms but only
when the sides change, with 0.5 ms for each of up to 16 assembly attempts.

A frame with six or more sticker-sized outlines that make no side takes a
second look (`read_side`: the threshold and the contours again against a
narrower block, `narrow_contours`). In the 1449-frame warm-lamp session that is
173 frames (12%): those frames' `read_side` went from 0.15 ms to 1.4 to 1.6 ms
at p50 (2.0 to 2.4 ms at p90), and the session's mean `read_side` from 0.38 to
0.57 ms (the shared `prepare_frame` before it is 1.7 ms), at 640 wide with
nothing else on the machine. A frame with no cube in a room with that many
outlines (the 1080p `empty_frame` of `tests/perf`) allocates 113 buffers where
it did 36.

In the application, with the fake camera's 640x360 frames
(`direct_scan.process_frame`, 2085 frames): p50 2.2 ms, p99 2.9 ms. About 0.4 ms
of that is not the vision pass: the signals to the screen, the preview, the
held-cube judge. `app.frame_image.request` (the preview's `cv::Mat` to `QImage`
copy) is 0.07 ms at this size.

### The UI frame

Qt's own render timing (`QSG_RENDER_TIMING`) on the launch screen: a steady
frame is 2 ms (p50) on a 16 ms interval, 8 ms at the 99th percentile. The
expensive frames are the first two: 492 ms (487 of it scene-graph sync, the 3D
views' models) and 347 ms (332 of it creating pipelines).

Tracy's `UI` and `Capture` frame sets record the same cadence but
`tracy-csvexport` does not export frame times, so they are read in the GUI.

### The 3D view of a cube of any size

`CubeNView.qml` draws a cube that is not a 3x3 (`Cube3DView.qml` is the 3x3's and is
unchanged). It is **three scene objects at every size**: the cube at rest is one
mesh, and while layers turn it is two, the layers on a pivot and the rest, with solid
plastic where they part (`cube::nxn::build_mesh`, `CubeNGeometry`). Nothing is built
per cubie or bound per sticker, so a move costs the same at 2x2 and 9x9: picking
layers up and putting them down is two or three mesh builds, 0.1 to 0.2 ms. A whole
cube is 12 N^2 quads (108 at 3x3, 972 at 9x9).

Measured in the prototype it came from (`tools/cube3d_prototype`, 2026-10-16, on the
development machine, scene-graph work per frame, mean): 0.2 ms native and 0.4 ms in a
browser at every size from 3x3 to 9x9, 60 fps throughout. The same cube built the way
the 3x3's is, a model per cubie and a binding per sticker, costs 0.6 ms at 3x3 and 6.7
ms at 9x9 natively and misses 60 fps in a browser from 7x7 (31 fps at 7x7, 20 at 9x9).
**A phone has not been measured**; the estimate is a few times the desktop's figure,
which a thousand quads leave far inside a frame.

The shader styles cost it nothing it did not already have. Instructional Glow and
Borderlands Cel-Shading read each sticker's colour from the mesh's vertices
(`shaders/toon_mesh.frag`, `cel_mesh.frag`; the 3x3's `toon.frag` and `cel.frag` take it
from a uniform per sticker), so a style is a different material on the same meshes and
changing it rebuilds nothing. Cel-Shading's ink outline is one more model per mesh,
made only while that style is on (the 3x3's is 26, one per cubie). Its vignette and
halftone need `ExtendedSceneEnvironment`, which is the environment only while Cel-Shading
is: the same environment with every effect off costs a frame about 0.05 ms of render
time (0.09 to 0.14 ms at 6x6) and moves the edges' anti-aliasing, so the other two looks
keep the plain one. Measured with the gallery's `CUBE_GALLERY_SETTLE_MS` (a lifted
layer, which animates throughout; render is the CPU's time recording the frame, in the
window's own callbacks, normal priority, base and branch alternated four times,
2x2 and 6x6): the default look 0.09 ms render and 0.025 ms sync on both; Glow 0.09 ms;
Cel-Shading 0.17 ms; the 3x3's Cel-Shading (`Cube3DView`) 0.79 ms. Making one more view
costs about 0.5 ms more than before (four more materials, a second environment and the
three meshes as objects), once. GPU time was not measured.

`tests/unit/nxn/mesh_test.cpp` holds the mesh to the library with no GPU (every
sticker's quad is where the library says that sticker is; a lifted layer is the
stickers its turn moves), and `tests/unit/cube_n_view_test.cpp` loads the view and
reads every sticker back out of it at every size, after every kind of move.

### Startup

From the start of `app.load_qml` to its end, four warm runs at High priority,
medians:

| Step | ms |
|---|---|
| process start to the first QML singleton (`app.i18n_install` is 5 to 12) | 163 |
| creating the launch screen | 803 |
| showing the window: native window, graphics device, first sync | 577 |
| **`app.load_qml`** | **1544** |
| then the first frame renders | about 350 |

What the 803 ms is. Each row is an experiment build; none of it is committed.

| Launch screen | creation | `app.load_qml` |
|---|---|---|
| as it is | 803 | 1544 |
| one of its two 3D cubes removed (the layout not on show) | 650 | 1480 |
| both 3D cubes removed | 526 | 1274 |
| `Theme.fontHeading` and `fontMono` set to families that are installed | 241 | 1051 |
| the six font files `feature/ax-kit` bundles, loaded with `addApplicationFont` (20 ms) | 200 | 995 |

So the launch screen's cost is mostly **two font families that are named and
not bundled** (`Archivo`, `IBM Plex Mono`: the font lookup for a family that is
not there is about 550 ms, once, at the first text), then about **140 ms for
each `Cube3DView` it creates**, of which it creates two - the narrow and the
wide layout are both built, and one is hidden - and each builds four `Model`s
and materials per sticker, of which one is ever drawn.

When the solve finishes, `app.solve.apply_result` (the result landing on the GUI
thread, which includes creating the solve screen and its cube) was 57 ms and
168 ms in two solves: a hitch of three to ten frames as the screen appears.

### The light probe

The cube is lit by six lights and by a sky used as an image-based light - a
*light probe*. Qt Quick 3D wants a probe as a **prefiltered** cube map, one mip
level for each roughness, and given a plain image it makes that itself, on the
first frame of every `View3D`: 512 x 512 faces, six levels, 1024 samples a texel
(`QSSGBufferManager::createEnvironmentMap`; the face is `max(512, height / 2)`,
so a smaller image is not cheaper, and neither is `ProceduralSkyTextureData`'s
`Low` quality). Every return to a screen with the cube is a new `View3D`.

Where a GPU draws it that is not noticed. Where the CPU draws, it is most of
the first frame. The time from showing the launch screen to its fifth swapped
frame, for each time a screen with the cube comes up
(`cube_system_tests "[probe-cost]"`, three repetitions, two or three rounds):

| Renderer | Probe prefiltered at run time (as it was) | No probe | Prefiltered file (as it is) |
|---|---|---|---|
| Direct3D 11 on a GPU (RTX 4090) | 0.25 to 0.35 s; 1.0 to 1.5 s for the first cube of the process | the same | the same |
| Mesa llvmpipe, Qt's bundled `opengl32sw` (what CI draws with) | 10 to 16 s | 3.4 to 5.8 s | 3.3 to 6.0 s (once 0.3 s) |

(The process's own CPU time for the same interval was 18 to 23 s with the probe
prefiltered at run time and 3 to 5 s in the other two.)

And in a browser without a GPU - headless Chromium on SwiftShader, the
`diagnostics` build, three runs, on a 32-thread machine shared with other
builds:

| | Prefiltered at run time | No probe | Prefiltered file |
|---|---|---|---|
| The 30th UI frame after navigation | 34 to 38 s (48 s pinned to four cores) | 2.2 to 2.3 s | 2.2 to 3.8 s (4.6 s on four cores) |
| The next page, after this one is closed | 35 s; the GPU process spends 315 to 321 CPU-seconds | 0.9 s (7.5 CPU-seconds) | 1.2 to 1.5 s (9 to 10) |

The page paints its first five frames in about two seconds either way and then,
with the probe prefiltered at run time, stands still for half a minute; closing
it costs the same again, and what is waiting behind it (the next test of the
suite) waits too. That is where `tests/web`'s thirty seconds a test came from:
its `perf` project, which runs the app as shipped, took 3.9 minutes with one case
failed (a press the stalled page did not answer in 30 s) and takes 56 seconds
with the file, every case passing: start-up 30 s to 2.4 s, scan and solve 39 s
to 11 s, the three methods 60 s to 4 s, walking a solution 96 s to 35 s.

**What was done.** `src/app/qt/lightprobe/sky.ktx` is the prefiltering's
result - Qt's own baker over the same sky the cube made at run time - and a KTX
file with the baker's key is used by Qt as it is. `tools/lightprobe` makes it,
and says why it is 128 x 128 texels a face and not 512: 1 MB, not 17. Qt then
does no prefiltering at all.

**The look is kept.** The pictures of the screenshot tour in the three styles
(Physically-Based Realism, Instructional Glow, Borderlands Cel-Shading), light
and dark, with the file and with the probe made at run time: at 512 and 256
texels a face they differ by one level of one channel at most; at 128 by two,
in under 0.01 % of the picture's pixels, mean 0.02 to 0.04 of 255. A picture
taken twice of the same cube differs by one. The unit test `the light probe is
a prefiltered cube map with the sky above and the ground below` holds the file
to being what Qt takes as prefiltered (without the key it would be prefiltered
again, silently) and the right way up (the first bake was upside down, which
only a picture showed).

**What is left, and not the probe.** With no probe at all, the first frames of a
cube still take 3 to 6 s on llvmpipe and 2 s to the 30th frame in the browser:
the cube's materials and its MSAA on a CPU rasteriser. It is the same with and
without the probe, so it is not looked into here.

**The harness.** `App::Scene3D::cheap` in `tests/system` takes the probe off for
the cases that are not about how the cube looks. With the probe a file it saves
nothing measurable (the llvmpipe row), so the walk could ask for the cube the
app ships and the distinction could go: `app.hpp`, `app.cpp`, three callers and
the time-budget part of [testing](testing.md). Not done here; `tests/web` no
longer takes the probe out.

### Screens

How long it takes to arrive on a screen, and what the screen costs once it is
there, from the hidden `[screen-cost]` case of the system tests
(`tests/system/screen_cost_test.cpp`, the real application with the fake
camera, Direct3D 11 on an RTX 4090, `PriorityClass` High, base and branch
alternated four times, medians over the repetitions after the first). `call` is
how long the GUI thread was held by the navigation itself - the screen being
made - and `tenth` the time to the tenth swapped frame after it; both in ms.

| Screen | `call` before | after | `tenth` before | after | idle frames a second |
|---|---|---|---|---|---|
| Home (launch) | 22 | 2 | 224 | **169** | 0 (the cube turns when the setting is on) |
| Settings (wide; the narrow layout makes no cube now) | 52 | 25 | 275 | **184** | 0 |
| Solve, 3x3 | 13 | 12 | 176 | **155** | 1 |
| Solution, 3x3 | 11 | 11 | 181 | **165** | 0 |
| Entry, History | 3 to 6 | 3 | 135 to 143 | 136 to 145 | 0 |
| Scan | 4 | 4 | 137 | 136 | 22 (the camera) |
| Solve, 6x6 (`CubeNView`) | 9 | 9 | 142 | 143 | 28 (the layer nudging) |

What is in it.

- **A sticker was four models and is one.** The 3x3's cube built, for each of its
  54 stickers, a panel for each of the three looks and one for the skeleton
  placeholder, and switched three of them off: 270 models and as many
  materials, 26 bodies and 26 cel-look outlines among them. Each is a
  scene-graph node that is created, synced and kept whether or not it is
  drawn. A sticker now makes the one panel its look asks for (a `Loader3D` over
  four components), and the outline exists only in the Cel look: **80 models
  at the first two looks and 106 at the third**, with the same picture. That is
  the 25 to 35 % off arriving on Home and Settings and 10 % off the solve
  and solution screens, which already made one cube.
- **Home built both layouts' cubes**, one never drawn. It makes the one that is
  on show (the earlier measurement put this at 150 ms of the screen's creation,
  [Startup](#startup)), and only after the menu's first frame has been swapped:
  the first frame of a 3D view is the longest the GUI thread is held at
  start-up, and the buttons are what a person waits for. The real application on
  Vulkan, eight alternated launches, medians from the process's first log line:
  the menu is drawn at **1.14 s instead of 1.24**, everything settled at **1.36
  s instead of 1.46**. The screen fades in over 280 ms as before; the cube arrives
  inside that fade.
- **Per-frame cost was never in it.** A hidden model is not drawn: the render
  thread's `render.frame` is 0.5 to 1.1 ms on the launch and solve screens
  before and after (Vulkan, Tracy, three alternated captures of 8 s each), at 46
  and 28 frames a second. What the extra models cost is the time to make them
  and the memory they hold, which is what a phone is short of.
- **The first 3D frame in a process is Qt's, not ours.** The first `View3D` to
  draw takes 0.6 to 1.3 s on Direct3D 11 here (`worst_gap` of the launch
  screen's first repetition), whatever the scene: a bare `View3D` with one cube
  and one light, built to find out (`qml.exe`, Direct3D 11), takes 0.5 to 0.95 s,
  and 81 cubes with four lights, the probe and MSAA take 0.4 to 0.5. Qt Quick 3D's
  own persistent shader cache (`QtQuick3DEditorHelpers::ShaderCache::
  setAutomaticDiskCache`, a private function) was tried in the system tests and
  the app: it writes a 130 to 400 KB file and the first frame took the same
  time, so it is not used. A cube that is not a 3x3 stalls once more, 130 to 350 ms,
  for its own materials (vertex-coloured plastic); warming that up before it is
  asked for would move the stall to the Home screen, and is left.
- **Backends** (the same harness, the first 3D frame in the process, two runs
  each): Direct3D 11 650 and 640 ms, Direct3D 12 580 and 490 (and 50 to 100 ms gaps
  in steady frames, which D3D11 does not have), OpenGL 390 and 250,
  **Vulkan 350 and 150** - the second run is faster because Qt saves a pipeline
  cache between runs (`qtpipelinecache` under the cache directory), which the
  Direct3D 11 backend has nothing to save for. The app's default on Windows is
  Direct3D 11; Vulkan is the person's choice in the settings and was the one the
  real-application runs above used. Whether Vulkan should be the default is a
  question of which drivers are in the field, not of this machine.
- **The render loop**: the platform default (threaded on Windows) against
  `QSG_RENDER_LOOP=basic`, eight alternated launches, settled 2.327 s and 2.335 s
  on a busy machine: no difference, so there is nothing to gain from changing
  it, and the harness's pin to `basic` stays a test-only measure
  ([testing.md](testing.md)). Vertical sync off brings the first frames in
  earlier (settled 1.32 s to 1.15 s) and is not a thing to ship.
- **When the solve lands** the GUI thread is held 24 to 29 ms
  (`app.solve.apply_result`, three captures of the 3x3 demo; 30 ms at 6x6).
  Taking the result from the worker is 0.01 to 0.03 ms and the solution signal
  0.005; **the state signal is 24 to 28 ms, and that is the solve screen being
  made**, because the screen change is made inside that signal (`AppState`'s
  `showSolutionWhenReady`, and the demo's own slot). The controller's getters
  total 0.15 ms of it, and taking the cube's sticker colours or the list of
  stages out changed nothing. So the cost is the one `call` measures above, 11
  to 13 ms warm, plus the signal's other listeners. A hitch of one or two frames
  on a desktop; three to five on a phone if it scales like the rest. The three
  child zones are in the tree to see it with.
- **The scan screen at rest** (the fake camera's drawn 640 x 360 frames, 66
  frames of the system tests under Tracy, Direct3D 11): `direct_scan.process_frame`
  is **8.5 ms** a frame on average, 7.0 at best and 13.0 at worst, a quarter of
  the 33 ms budget (a phone at 3 to 5 times a desktop core would be over it on
  average). Where it goes: `read_side_glare_look` 3.5 ms (almost all of it
  `contours_without_glare`, 3.3), `read_side` 2.9 over two calls, `prepare_frame`
  1.1 over two, the pitch look 1.0, the second look 0.9. The glare look runs on
  every frame that has an outline too big to be a sticker in it, glare or not.
  The preview's `QImage` copy (`app.frame_image.request`) is 0.065 ms: the video
  upload is not a cost.
  - **Where the glare look's 3.3 ms went** (`cube_scan glare-cost`, 640 x 360
    working frame, fastest of three runs): **2.4 ms was `channels_of`**, the loop
    that takes the brightest and darkest channel of every pixel, which read each
    one through `Mat::at<Vec3b>` and wrote two planes through `Mat::at<uchar>`.
    The same arithmetic by row pointers is 0.35 ms (OpenCV's `split` and `max` /
    `min` is 0.3 and allocates five planes; `v_load_deinterleave` is 0.04 and is
    not used, to keep the reader free of per-platform intrinsics). The rest is
    the box-filter threshold (0.5), the trace (0.5 to 1), the blur (0.08), the
    mask and the open (0.1 each). `chroma_contours`, the chroma look, shared the
    loop and gains the same. The look's outlines, per frame it is taken on: 2.77
    to 1.51 ms on a 3x3 session, 3.75 to 1.71 on a 4x4 (960 px wide), 4.96 to 1.70 on
    a 5x5; `timing.glare_look.640x360` 0.52 to 0.27 of the calibration workload.
    Then the look stopped redoing what `prepare_frame` had done: it takes the
    blurred brightness the frame already has (the look blurred the same maximum
    again), finds the pixels to cut out in one pass that reads a pixel's colour
    only where the blurred brightness is 252 or more, hands back the first
    look's own outlines when there are none (67 of 108 glare-look frames of a
    4x4 session, none of a daylight-backlit 3x3 session, where a window is
    clipped), and otherwise starts from the first look's threshold, which the
    prepared frame now keeps (a 640 x 360 plane, 0.23 MB more allocated by a
    read). Per frame it is taken on: 2.77 to 0.82 ms on the 3x3 session, 3.75 to
    0.40 on the 4x4, 4.96 to 0.79 on the 5x5; `timing.glare_look.640x360` 0.52 to
    0.09 (0.03 with nothing clipped). `read_side` of the same sessions, a frame in
    all: 2.35 to 1.67, 3.40 to 1.63, 3.66 to 1.83 ms. 235 recorded sessions (98,440 frames, 52,614 reads, 216 of them by this look)
    give the same digest before and after, `cube_scan digest`: every blob count
    and outline count per look, the lattice's residuals, and each read's centres,
    pitch, found flags and Lab colours, bit for bit.
  - One thing the look does that reads as an oversight and is kept, because a
    read depends on it: `contours_without_glare` blurs the brightness in place
    and then judges "clipped" (252 and up) on the *blurred* value, and
    "colourless" on the unblurred chroma. A highlight has to fill its 5 x 5
    neighbourhood to be taken out.
- **Idle.** Home spins its cube when the setting is on, as designed, and draws
  every frame for it (46 a second, 0.5 to 1 ms of render-thread time each); the
  solve screens nudge the layer to move, which is the same; History, Entry,
  Settings and the solution draw nothing at rest. Nothing is drawing that should
  not be.
- **Not changed, measured: Qt Quick Controls' style.** Setting
  `QT_QUICK_CONTROLS_STYLE=Basic` moved the first sync by 12 ms in six alternated
  launches. Declaring the screens as `source:` URLs in `Main.qml` instead of as
  `Component`s (so the engine resolves a screen's types when it is used)
  moved it by 40 ms in eight, which was not worth the changes to
  `SolutionScreen`'s bindings; neither is in the tree.

The case is a measurement and asserts nothing about time, because time on a
shared machine is not a number a test can hold. What holds are the two counts in
the [budgets](#budgets), as unit tests.

## Cubes of any size

`cube::nxn` solves a cube from 2x2 to 9x9 by reduction ([nxn-solver.md](nxn-solver.md)).
What a person waits for is two things: the **setup** for a size, built once per
process on first use (`nxn::prepare(size, method)`: the turn tables, the library
of 3-cycles the wings and centres are built from, and the centre search's
actions), and the **solve** that follows it. The app does the first on a worker
while a person is scanning or entering a cube, and the second on the solve's
worker thread already.

`cube_nxn_bench time` measures both (setup alone first, then ten seeded
scrambles with the setup in place), in the build it is given. `tools/nxn_bench/
standalone.py` compiles it straight from the sources for a target the build does
not make it for (no OpenCV, no Qt): WebAssembly to run under Node, or an arm64
Android executable to `adb push` to a phone.

Taken 2026-10-16, 10 scrambles of 60 + 10 N layer turns per size, one thread:

| Size | Moves | Native setup | Native solve (median / max) | Wasm setup | Wasm solve (median / max) |
|---|---|---|---|---|---|
| 2x2 | 8 | 0 | 3.2 / 9.3 ms | 0 | 3.2 / 4.5 ms |
| 3x3 | 19 | 0 | 31 / 33 ms | 0 | 39 / 44 ms |
| 4x4 | 134 | 23 ms | 42 / 50 ms | 26 ms | 48 / 51 ms |
| 5x5 | 172 | 47 ms | 41 / 47 ms | 33 ms | 50 / 55 ms |
| 6x6 | 333 | 109 ms | 63 / 71 ms | 81 ms | 72 / 86 ms |
| 7x7 | 412 | 172 ms | 73 / 80 ms | 139 ms | 89 / 98 ms |
| 8x8 | 616 | 310 ms | 115 / 125 ms | 248 ms | 144 / 155 ms |
| 9x9 | 742 | 462 ms | 164 / 187 ms | 379 ms | 203 / 228 ms |

- **Native**: `windows-msvc-fastdebug` (RelWithDebInfo), MSVC 19.51, the 32-core
  development machine, shared with other builds (so noise of tens of percent).
- **Wasm**: the same sources through `emcc -O2` (Emscripten 5.0.5, the one the Qt
  wasm kit needs), run under Node 24.19 (V8, the engine Chromium uses), on the
  same machine. Not a browser tab, but the same engine and the same compiler; the
  browser's own cost (a page that is busy, a throttled background tab) is not in
  it. The 3x3 method's tables (`Advanced`, built once for any size) are 26 ms
  native and 117 ms in wasm, and are charged to nobody above.
- **Memory**: the peak working set of the whole process, setup and solves, is
  22 MB native for every size to 7x7 and 36 MB at 9x9 (the process itself, with
  the 3x3 tables, is 22); under Node it is 52 to 67 MB (Node's own is about 50).
  The tables are not a memory concern at any size.
- **A refused cube is not free, at 2x2.** Handed an unsolvable cube (a twisted
  corner), `solve()` refuses a 3x3 at once and a 4x4 to 7x7 in 3 to 40 ms, but a
  2x2 only after about a second: its search is exhaustive, against 8 ms for one
  that can be solved. Nothing in the app hands it one (hand entry, a scan's
  assembly and a history entry are validated first, by `nxn::validate`), but
  `solve()` is public and promises bounded work on every input; asking the
  oracle first would make the refusal instant.

**On a phone** this is an estimate until a phone has run it: a phone's big core is
about a third to two thirds of this machine's for code like this, a mid-range
one a quarter, so a 7x7's setup is about 0.3 to 0.7 s and its solve 0.15 to 0.3 s,
and a 9x9's 1 to 2 s and 0.4 to 0.8 s: seconds, at the very worst, for the
sizes the app is meant to offer. `standalone.py android` builds the executable
for the measurement. When it has been taken the figures replace this paragraph.

**Held by the performance tier** (`cube_perf_tests`, [testing.md](testing.md)):
the moves per size over a six-scramble corpus and the cycles each size's library
holds, counted and exact on every machine, a quarter above what they measure
(`budgets.hpp`, `kNxnSizes`); and, opt-in with the other wall-time checks, a
5x5, 7x7 and 9x9 solve against the calibration workload (10.4, 15.6 and 39.8
times it now; budgets 40, 60, 150). The setup is not timed there: it can be made
once per process.

## Ranking

By what a person waits for, and how far it is from its budget, as found:

1. **The solve**: 1.8 s on the desktop for the default method, 3 s the worst.
   Several seconds on a phone, over a budget of "seconds".
2. **Startup**: 1.5 s on a fast desktop before the first frame, more than half
   of it fonts that are not installed and a second 3D cube nobody sees.
3. **A turned cube in the camera pass**: 4 ms of 33 at 640 wide, 8 ms at
   1080p; times a phone's slower core, the one place the frame budget could be
   in danger.
4. **An unsolvable cube**: 2.5 to 6 s, for an input the app cannot produce.
5. Everything else is far inside its budget: a steady frame 2 ms, `add`
   microseconds, the last layer and the other two methods.

## What was fixed

| | Before | After | |
|---|---|---|---|
| The first-two-layers searches follow pieces, not cubes | 1.77 to 1.86 s a solve, 3.0 s worst | **0.12 s**, 0.21 to 0.24 s worst; the same 985 000 visits and the same solutions (digest `1bc470006f988409`) | [#75](https://github.com/iainchesworthlabs/cubeulator/pull/75) |
| An unsolvable cube is answered at once | 2.5 to 6 s | under 0.1 ms, the same `no_solution` | [#74](https://github.com/iainchesworthlabs/cubeulator/pull/74) |
| A frame is prepared once for both readers | 4.2 ms turned, 8.2 ms at 1080p | 2.2 ms, 3.6 ms; the same reads | [#78](https://github.com/iainchesworthlabs/cubeulator/pull/78) |
| The cube's light probe is prefiltered once, not on every `View3D`'s first frame | llvmpipe 10 to 16 s a screen with the cube; a browser without a GPU stands still for 35 s at the start and 35 s at the close | 3 to 6 s (the cube itself); the 30th frame in 2 to 4 s; the close 1 s | this change, [above](#the-light-probe) |
| A sticker of the 3x3 cube is the one model it draws; Home makes the one cube it shows, after its menu | Home 22 ms to make and 224 ms to the tenth frame; Settings 52 and 275; the cube 268 models | 2 and 169; 25 and 184; the cube 80 models (106 in the Cel look) | this change, [above](#screens) |

The solver's cost per visit fell from 1.7 to 3.4 us to 0.15 to 0.29 us; what it
did not change is the number of visits, and that is the one lever left (below).
Over 2000 further scrambles the new search had no failure, a mean of 104 ms, a
95th percentile of 186 ms and a worst of 341 ms; 120 more gave the digest the
old search gave.

## What was not fixed

- **The fonts.** The cost is the fonts being named and not shipped, which
  `feature/ax-kit` already fixes by bundling six font files and loading them.
  Measured here with that branch's files: **-550 ms of startup**, and the
  kit's loader (`bundled_fonts.cpp`) costs about 20 ms of it back. The thing to
  check when the kit lands is the startup timeline above, not just the glyphs.
- **Two cubes per screen, four models per sticker** were this entry until the
  [Screens](#screens) change: Home made the one cube it shows and each
  sticker one model. The other screens that build both a narrow and a wide
  layout still build both, without a cube in either (the Settings screen made
  its cube for the narrow layout too, where it is hidden, and now does not).
  The controls of Settings are in both layouts twice over; that was not
  measured and is not changed.
- **The first 3D frame**, 0.4 to 1.3 s depending on the backend, is Qt's
  (`Screens`). The one thing it could be moved off is the person's attention:
  the menu now draws before it, and a window-sized splash would not buy more.
- **Showing the window and the first frame**, 577 ms and about 350 ms: the
  native window, the graphics device and the first pipelines. It does not
  change with the 3D cubes removed (the experiment above), so it is Qt and the
  driver; the graphics API picker in the settings is the one lever and it is
  the person's.
- **The solver's visit count.** About a million visits a solve, 24 bytes each
  in an arena. The searches could be made to visit a small fraction of that
  with the same solutions: a ball around the goal, to a few moves, gives an
  exact lower bound on the distance left, and a search that skips a node whose
  depth plus that bound exceeds the length of the answer returns the first
  answer a plain breadth-first search does, because it only skips what cannot
  be on a path of that length (the bound never overstates, and it changes by at
  most one a move). It would be a five to twenty fold cut in visits. It is not
  done because a solve is already 0.12 s on a desktop, which is under the
  budget with room for a phone several times slower, and the change would need
  its own proof; the golden digests in `piece_search_test.cpp` are the check it
  would be held to.
- **Assembly retries, the median in `SideScan::add`**: microseconds to a
  couple of milliseconds, and only on frames that change a side.
- **A phone and the capture thread**, which were not measured. The estimate for
  a phone, 3 to 5 times a desktop core: a solve 0.4 to 0.6 s; a turned cube at
  1080p 11 to 18 ms of the 33. Both are inside their budgets with that
  multiple, which is the thing to confirm on the first device.

## Reproducing

```sh
cmake --preset windows-msvc-profiling
cmake --build --preset windows-msvc-profiling

TOOLS=build/windows-msvc-profiling/vcpkg_installed/x64-windows/tools/tracy
BIN=build/windows-msvc-profiling/bin

# the solver: a seeded corpus, zones by stage, a digest of every solution
$TOOLS/tracy-capture -o solver.tracy -a 127.0.0.1 -f &
$BIN/cube_solve_bench --method beginner --count 28 --seed 0 --moves 25
$TOOLS/tracy-csvexport -u solver.tracy > solver-events.csv     # value = visited nodes

# the camera frame: recorded frames through the reader the app uses
$TOOLS/tracy-capture -o scan.tracy -a 127.0.0.1 -f &
$BIN/cube_scan replay <session-dir>            # <session-dir>/fullres for 1080p

# the application, the journey case of the system tests
$TOOLS/tracy-capture -o journey.tracy -a 127.0.0.1 -f &
$BIN/cube_system_tests "[journey]"

# Qt's own frame timings: launch the app with
#   QSG_RENDER_TIMING=1  QT_LOGGING_RULES=qt.scenegraph.time.renderloop=true

# the screens: arriving on each (call, tenth frame, idle frames a second)
CUBE_SCREEN_COST=1 $BIN/cube_system_tests "[screen-cost]"

# the light probe: the time to the fifth frame of a screen with the cube,
# with the probe and without it, on the software renderer the CI uses (Mesa)
CUBE_PROBE_COST=1 QT_OPENGL=software QSG_RHI_BACKEND=opengl \
    $BIN/cube_system_tests "[probe-cost]"
# and in a browser: window.cubeulatorProfile.summary().frames.UI is the
# count of frames drawn (testing.md, "Browser tests"); read it every 50 ms
# from navigation, and time the next page after one is closed
```

Startup is read from the `app.load_qml` zone of a capture of the application
itself (`Cubeulator.exe` on Windows; see
[profiling.md](profiling.md#capturing-a-trace)). The pieces of it in the table
above need timestamps inside QML, which the experiment builds added and the
tree does not carry.
