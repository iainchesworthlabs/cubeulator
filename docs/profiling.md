# Profiling with Tracy

Cubeulator is instrumented with [Tracy](https://github.com/wolfpld/tracy), the
same profiler and the same workflow the rest of the application family uses
(ICL Forge, Aqualink Automate), so a trace from any of them is read, captured
and exported the same way. The instrumentation goes all the way through the
code the application runs: the camera capture thread, the direct scan's read
and the held-cube judging, the recorder, startup, the whole Qt Quick frame
(synchronisation, the render pass, the swap, and the GPU's side of it) and
every stage of the solver.

The Tracy client is **built into every desktop build** (Windows, Linux, macOS)
and is **dormant until the application is told to profile**:

| Build | Profiling starts | How to change it |
| --- | --- | --- |
| **Debug** | at launch, with no flag | `--no-profile`, or `CUBEULATOR_PROFILE=0` |
| **Release** (and every other configuration) | never, unless asked | `--profile`, or `CUBEULATOR_PROFILE=1` - no recompile, on the binary that was shipped |
| `*-profiling` presets | at launch (they exist to take traces) | `--no-profile` |
| web, Android and iOS presets, coverage builds, Windows on ARM (configure it `OFF`) | not built in (the browser has its own [recorder](#in-the-browser)) | |

"Dormant" is meant literally - see [What a shipped build costs](#what-a-shipped-build-costs).

What a capture of the application found, and what was done about it, is in
[Performance](performance.md).

## Turning it on

The switch is read once, at launch, by `main()` before anything else starts
(`cube::profile::Session`, `cube/profile/session.hpp`). In order of priority:

1. **The command line**: `--profile` starts the profiler, `--no-profile` does
   not, `--profile-wait` starts it and [waits for a profiler](#capturing-the-start-up);
   if several appear the last wins.
2. **The environment**: `CUBEULATOR_PROFILE` = `1`, `true`, `on` or `yes`
   starts it; `wait` starts it and waits for a profiler; `0`, `false`, `off`
   or `no` does not (case does not matter).
   Anything else, or empty, is ignored - a typo must not silently open (or
   close) a profiler.
3. **The build's default**: on in a Debug build, off otherwise. It is the
   `CUBEULATOR_TRACY_AUTOSTART` CMake option: `AUTO` (Debug only; the default),
   `ON` or `OFF`.

```sh
# a Release binary from an installer, to diagnose something on a user's machine
Cubeulator.exe --profile                    # Windows
CUBEULATOR_PROFILE=1 cubeulator             # Linux
CUBEULATOR_PROFILE=1 open Cubeulator.app    # macOS (set it in the launching environment)
```

The application logs one line when the profiler starts, naming what switched
it on and the port to connect to:

```
Tracy profiler started (--profile): connect a Tracy 0.14 profiler to 127.0.0.1:8086
```

### Capturing the start-up

Tracy (on-demand) records nothing until a profiler connects, so a profiler
attached by hand a second after launch has missed `app.i18n_install`,
`app.load_qml` and the `startup` section - exactly what start-up work needs.
`--profile-wait` (or `CUBEULATOR_PROFILE=wait`) makes `main()` stop **before it
builds anything** and wait for a client:

```sh
tracy-capture -o startup.tracy -a 127.0.0.1 -f &       # or open the GUI and press Connect
Cubeulator.exe --profile-wait
```

```
Tracy profiler started (--profile): connect a Tracy 0.14 profiler to 127.0.0.1:8086
Waiting up to 30 s for a profiler to connect (--profile-wait, CUBEULATOR_PROFILE=wait)
A profiler connected
```

The wait is **30 seconds at most**: if nothing connects the log says
`No profiler connected: starting anyway` and the application opens as normal, so
the option can never leave a user with a window that does not appear. Start the
capture first (it keeps retrying until the application is listening), or press
Connect in the GUI within the half minute. Without a profiler to wait for (a
build with Tracy compiled out, or `--no-profile`) the option does nothing.

The profiler only starts at launch. Turning it on in a running process is not
supported: Tracy's client is started once (and its thread names, which are
written at thread start, would be missing for threads that already exist).
The graphics backend is chosen at launch too, so a trace names which one it
measured (see [The render pass](#the-render-pass-cpu-and-gpu)).

### The profiler listens on the loopback interface only

A running Tracy client opens a TCP data port (8086; `TRACY_PORT` in the
environment changes it) and answers UDP discovery broadcasts. The client is
built with `TRACY_ONLY_LOCALHOST` (the overlay port, below), so both are
bound to `127.0.0.1`: a profiling session on a user's machine is not visible
to the network, and connecting from another machine is an SSH tunnel
(`ssh -L 8086:127.0.0.1:8086 host`) to the loopback port.

### What a shipped build costs

Tracy would normally start its worker thread and open its listening socket in
a static initialiser, before `main()`, in every process that links it. The
`tracy` port this project builds (`vcpkg-overlay-ports/tracy`, below) turns on
Tracy's **manual lifetime** (`TRACY_MANUAL_LIFETIME`): the client does not exist
until `cube::profiling::start()` runs. So a Release process that was not asked
to profile has

- no Tracy thread, no socket, no broadcast and no allocation;
- one **relaxed atomic load and a not-taken branch** at each zone, frame mark,
  plot, message and thread name (`cube::profiling::active()`; every macro in
  `cube/profiling.hpp` checks it first, because with manual lifetime Tracy's
  own state cannot be touched before it starts);
- no `RenderProfiler`, no log hook and no Qt logging categories switched on:
  `main()` installs those only when the profiler is running.

Once started, Tracy runs in **on-demand** mode: it records nothing until a
profiler connects, so a profiling session nobody is watching costs the same
branch plus Tracy's own connected-check per zone.

The measured cost of both states against a build with Tracy compiled out is in
[Idle cost](#idle-cost).

## Building

Nothing to configure for a desktop build: `CUBEULATOR_ENABLE_TRACY` defaults to
`ON`; the web, Android and iOS presets turn it `OFF`, and so does a coverage build
(the default is settled in the top-level `CMakeLists.txt`, before `project()`,
because it selects a vcpkg feature, and the platform is not known until after
it - which is why the presets, not CMake, name the platforms without Tracy).
Windows on ARM is one: the port does not support it, so configure it with
`-DCUBEULATOR_ENABLE_TRACY=OFF`. That flag opts any build out; the
macros then compile to nothing and Tracy need not be installed at all.

The option adds the `profiling` vcpkg manifest feature, which supplies

- the Tracy **client** the application links (an overlay of the registry's port,
  `vcpkg-overlay-ports/tracy`, which adds two features the registry's lacks:
  `manual-lifetime` and `only-localhost`, each just the Tracy CMake option of
  the same name - both are compile-time switches of the client library, which
  is why they cannot be set from this project), and
- the Vulkan headers (`vulkan-headers`, header-only) on the platforms whose Qt
  can render with Vulkan - Tracy's Vulkan context is compiled against them
  and finds the loader at run time.

For a trace you also want the command line tools (`tracy-capture`,
`tracy-csvexport`): `-DCUBEULATOR_TRACY_TOOLS=ON` adds the `profiling-tools`
feature, which builds them for the *host* (so a mobile cross-build does not try
to build them for the device). The `*-profiling` presets set it and turn the
profiler on at launch:

```sh
cmake --preset windows-msvc-profiling      # or linux-gcc-profiling / macos-clang-profiling
cmake --build --preset windows-msvc-profiling
```

The preset is `fastdebug` (RelWithDebInfo: optimised, with symbols - the right
shape for a profiler) plus Tracy, its tools, autostart and sanitizers off.

Take numbers from an optimised build. A Debug build profiles by default, which
is for finding *where* something happens (a hitch, a missing frame, the order
of two things); AddressSanitizer dominates its timings, so do not read
durations from it.

A new Tracy version is a change to `vcpkg-overlay-ports/tracy` (re-copy the
registry port at the new baseline, re-apply the two feature lines at its top)
and the `version>=` in `vcpkg.json`; the notices follow by themselves (they are
generated from the link line, see [Licence](#licence)). Use the profiler GUI
and tools of the **same version** as the client - the wire protocol changes
between releases.

## Capturing a trace

Use the server tools at the **same version as the client** (0.14.1). Headless,
from the tools vcpkg built into the build tree:

```sh
TOOLS=build/windows-msvc-profiling/vcpkg_installed/x64-windows/tools/tracy   # x64-linux, arm64-osx elsewhere

# start the capture first; it waits for the application to appear
$TOOLS/tracy-capture -o scan.tracy -a 127.0.0.1 -f &
build/windows-msvc-profiling/bin/Cubeulator.exe          # a Release binary: add --profile

# the capture ends when the application exits; then export it
$TOOLS/tracy-csvexport scan.tracy > zones.csv         # per-zone statistics
$TOOLS/tracy-csvexport -u scan.tracy > events.csv     # every zone event
$TOOLS/tracy-csvexport -g scan.tracy > gpu.csv        # every GPU zone event
$TOOLS/tracy-csvexport -m scan.tracy > messages.csv   # the messages
$TOOLS/tracy-csvexport -u -p scan.tracy > plots.csv   # zone events, then the plot samples
```

`tracy-capture`: `-o` output file, `-a` the application's address, `-p` the
port (match `TRACY_PORT`), `-f` overwrite an existing output, `-s <seconds>`
stop after that long. `tracy-csvexport`: by default one row per zone with its
count and total/mean/min/max/standard deviation; `-f <text>` filters zone names
(so `-f solver.` is the solver), `-e` reports self time rather than inclusive,
`-g` reports the GPU zone events (one row per event with the GPU execution time
in nanoseconds; GPU zones are not in the default list), `-u -p` appends the
plot samples (`name,,,ns_since_start,,,value`). That is the neutral
interchange: plain CSV, readable by anything. `tracy-import-chrome` converts a
Chrome-trace JSON the other way, and `tracy-update` upgrades an old capture to
the current format.

Interactively, run the `tracy-profiler` GUI **0.14.x** (a release from
<https://github.com/wolfpld/tracy/releases>, or built from that tag), press
**Connect** and enter `127.0.0.1` (and the port, if `TRACY_PORT` is set), or
pick the application from the discovery list. The GUI is not part of the build.
Reading it, in the order a question usually goes:

- **Frame view** (the bars at the top): the `UI` frame set is the presentation
  cadence; a tall bar is a frame that missed its deadline. Right-click the frame
  set's name to switch between `UI` (the screen), `Capture` (the camera) and
  the unnamed set (the vision pass).
- **Timeline**: threads `main` (the GUI), `camera-capture`, `recorder-writer`
  and the scene graph's render thread, which carries `render.sync`,
  `render.rhi_frame`, `render.frame`, `render.pass`; the GPU's own track,
  `render`, sits beside them with `render.gpu`. Click a zone for its source and
  its value (the solver's zones carry the visited-node count).
- **Plots**: `render.frame_ms`, `capture.interval_ms` and the scene graph's own
  `qsg.*` numbers (below).
- **Messages**: `render.scenegraph_initialized`, `solve.started`,
  `direct_scan.completed`, and whatever Qt logged about a texture upload or a
  shader compile.
- **Sections** (`startup`, `solve`) filter the timeline to one phase.
- **Information** window: the commit, branch and whether the tree was dirty.

## What is instrumented

Zone names are `<layer>.<thing>`, lower case, dotted. They are what the CSV
and the GUI show, so they are the stable interface; rename one deliberately.

### Frames

Three frame sets, because three unrelated real-time cadences on one set would
interleave into a single meaningless graph:

| Frame set | One mark per | Where |
| --- | --- | --- |
| *(unnamed)* | camera frame processed by the direct scan (the vision pass) | `DirectScan::processFrame` |
| `Capture` | camera frame delivered by the device | `OpenCvCaptureLoop::run` |
| `UI` | Qt Quick presentation (`frameSwapped`) | `RenderProfiler` (`src/app/qt/render_profiler.cpp`) |

### Threads

`main` (GUI), `camera-capture`, `recorder-writer`, and the scene graph's
render thread, which shows the `render.*` zones and, on a track of its own, the
GPU's. Pool
threads that run a solve keep their default name - they are shared, and a
name is written once.

### The render pass (CPU and GPU)

`RenderProfiler` (installed by `main.cpp`, only when the profiler is running)
brackets the phases of every Qt Quick frame, all on the scene graph's render
thread:

| Zone | From - to | What it is |
| --- | --- | --- |
| `render.sync` | `beforeSynchronizing` - `afterSynchronizing` | the GUI thread blocked while the scene graph copies the QML scene's state (with the threaded loop; in the basic loop it is the same thread) |
| `render.rhi_frame` | `beforeFrameBegin` - `afterFrameEnd` | the whole RHI frame: begin, recording, submit and present |
| `render.frame` | `beforeRendering` - `afterRendering` | the scene graph's render, inside the RHI frame |
| `render.pass` | `beforeRenderPassRecording` - `afterRenderPassRecording` | recording the main render pass's draw calls |
| `render.gpu` | the same two signals, on the GPU timeline | the GPU executing that pass (below) |

and a "UI" frame mark on `frameSwapped`. `render.frame_ms` plots the time
between presentations, and `capture.interval_ms` the camera's delivered-frame
cadence - the two cadences a frame's work is budgeted against. Messages mark
the scene graph's life: `render.scenegraph_initialized` and
`render.scenegraph_invalidated` (a window created or torn down - with the
graphics backend named in the trace's information window).

#### What the scene graph logs about itself: the `qsg.*` plots

Qt does not expose what the scene graph spends its frame on, other than by
logging it under the `qt.scenegraph.time.*` categories. While the profiler is
running, `qsg_log_bridge.cpp` switches those categories on and installs a Qt
message handler that reads each line (`cube::profile::parse_qsg_timing`, tested
against lines copied from a real run) into a plot or a message; the lines are
kept off the console unless `QT_LOGGING_RULES` names them.

| Plot | From | Why you would look |
| --- | --- | --- |
| `qsg.polish_ms`, `qsg.sync_ms`, `qsg.render_ms`, `qsg.swap_ms` | `time.renderloop`, per frame | the split of a frame; `swap` is the present - a vsync wait, or the compositor |
| `qsg.renderer_ms`, `qsg.renderer_preprocess_ms`, `qsg.renderer_updates_ms`, `qsg.renderer_rendering_ms` | `time.renderer`, per frame | **preprocess is where Quick 3D renders its offscreen passes and where the first frame builds its shaders and pipelines** - the first-frame hitch shows here (one run: 143 ms against a steady 2 ms) |
| `qsg.glyph_ms`, `qsg.glyph_upload_ms` | `time.glyph` | text being prepared and uploaded to the glyph atlas |
| `qsg.texture_ms`, `qsg.texture_upload_ms` | `time.texture` | a texture uploaded (the camera preview is one, every frame it changes) |

`time.texture` and `time.compilation` lines are also sent as **messages**, text
and all, because they are rare and what someone chasing a hitch wants named.
(Those two categories logged nothing in the runs this was developed against -
the demo screen has no camera preview and the pipeline cache was warm - so
their wording is the part of the reading that has been tested only on Qt's
documented format; a line it cannot read still arrives as a message.)

#### The GPU's side: `render.gpu`

A GPU zone is a pair of timestamps the GPU writes as it executes, so they have
to be put **in the stream the scene graph's draw calls are in**, and how that is
done differs by graphics API. The window's backend is chosen at launch (the
Settings screen's graphics backend picker, `QSG_RHI_BACKEND`, or Qt's default
for the platform), and the trace gets the GPU zone of whichever it is:

| Backend | GPU zone | How |
| --- | --- | --- |
| **Vulkan** | yes | Tracy's Vulkan context (`tracy_gpu_vulkan.cpp`) on the instance, device and queue Qt renders with; timestamps are written into the command buffer Qt gives out for the pass - a secondary buffer, which is why the zone is the pass and not the frame - and the previous frames' are read and reset at `beforeRendering`, outside any pass. Verified on an NVIDIA RTX 4090 |
| **Direct3D 11** (Windows' Qt default) | yes | Tracy's D3D11 context (`windows/tracy_gpu_d3d11_windows.cpp`) on the device and immediate context. Qt's D3D11 backend records commands and replays them when the pass ends, so the timestamps are issued between `QQuickWindow::beginExternalCommands()` and `endExternalCommands()`, which replay what has been recorded first and put them in order |
| **OpenGL** (Windows, Linux) | yes | Tracy's OpenGL context (`src/lib/src/profile/tracy_gpu.cpp`) over the entry points resolved from the window's `QOpenGLContext`, with the same external-commands bracket |
| **Direct3D 12** | no (CPU zones only) | Tracy 0.14 has a D3D12 backend, but a D3D12 timestamp is a command written into the *command list*, and Qt's `QSGRendererInterface` returns null for the D3D12 command list (checked on Qt 6.12). The list is reachable only through QRhi's private API; that is not worth a dependency on `Qt6::GuiPrivate` for a backend nobody ships on |
| **Metal** (macOS, iOS) | no | Tracy's Metal zones are sampled by the render pass *descriptor* (`MTLCounterSampleBuffer` attachments), which Qt creates internally and does not hand out. Nothing here has been built on a Mac |
| software rendering, **WebAssembly** | no | nothing to time on (see [In the browser](#in-the-browser) for the browser) |

On a window with no GPU zone the application sends the message
`render.gpu_unavailable` once and keeps every CPU zone and plot. The GPU track
lines up with the CPU one through Tracy's clock calibration (Direct3D 11 and
OpenGL) or a one-off sync at context creation (Vulkan: Qt does not enable the
calibrated-timestamps extension, so a long trace can drift by a few
milliseconds; compare a GPU zone with its neighbours, not with a CPU zone
across a minute).

### Sections

Execution sections (Tracy 0.14) mark the app's high-level phases, so the GUI
can filter a capture to one of them:

| Section | Bounds |
| --- | --- |
| `startup` | `main()` to the loaded main window |
| `solve` | `SolveController::beginSolve` to the solve's result landing |

### Zones

| Layer | Zones |
| --- | --- |
| **capture** | `capture.read` (the blocking device read: its length *is* the camera's frame interval), `capture.publish`, `capture.start`, `capture.stop`, `capture.enumerate_devices`, `capture.photometry_lock`, `capture.photometry_restore` |
| **direct scan** | `direct_scan.poll_frame`, `direct_scan.process_frame`, `direct_scan.record`; `vision.prepare_frame` (the working-width reduction, threshold and outlines both readers start from, once per frame), `vision.direct_scan.read_side` (the per-frame sticker read), `vision.corner_view.read_corner` (the two-sides read, only on a frame with no square-on side), `vision.corner_view.fit_corner` (one such read held against one candidate cube, only when the six sides make more than one; one for each set of faces a read shows, not for each candidate: `fit_corner_each` fits the candidates that agree on those once), `vision.direct_scan.add` (fusing a read into the scan), `vision.direct_scan.scanned` and `vision.direct_scan.diagnose` (colour resolution and assembly: one judgement of the scan, which the app makes whenever a held side's fused colours moved), `vision.direct_scan.assemble` (putting the named sides together into a cube: one event for each time it is done, which a scan whose sides make no cube does not repeat while what it rests on is as it was) |
| **held cube** | `held_cube.on_frame_watched`, `held_cube.judge`, `vision.held_cube.fit` |
| **colour and recording** | `vision.resolve_colors`, `vision.analysis_downscale`, `vision.recorder.push` (the GUI-thread clone) and `vision.recorder.write_job` (the writer thread's disk write) |
| **solver** | `solver.solve`; per stage `solver.cross`, `.corners`, `.middle_edges`, `.oll`, `.pll`, then `solver.merge_adjacent_moves`; the other two methods are `solver.cfop` and `solver.two_phase` (the latter's lookup tables are built once: in `solver.two_phase.prepare` when asked for ahead of time, otherwise inside the first `solver.two_phase`); underneath, `solver.piece_search` (the first-two-layer searches, run on piece positions) and `solver.algorithm_search` (a template, so each stage's instantiation is its own row in the statistics) carry the **visited-node count** as a zone value (the first-two-layer searches' real cost, which is why the [slow tests](testing.md#slow-tests) are slow); `solver.bfs_search` is the same search over whole cubes, which only a cube that cannot be turned from solved ever reaches; `solver.assemble_scan` and `solver.assemble_all` (the one cube the six sides make, and every cube they make) |
| **nxn** | `nxn.solve` (the whole reduction solve), then per stage `nxn.plan`, `.parity`, `.centres`, `.plan_edges`, `.edges`, `.core` (and `.corners` for a 2x2); `nxn.assemble_scan` puts six scanned sides together as a cube of any size |
| **app** | startup: `app.i18n_install`, `app.load_qml` (the largest single startup cost); `app.solve_stepped` (the worker), `app.solve.apply_result` and inside it `app.solve.take_result`, `.emit_solution` and `.emit_state` (the screen change made inside it); `app.frame_image.request` (the preview's `cv::Mat` to `QImage` copy) |
| **render** | `render.sync`, `render.rhi_frame`, `render.frame`, `render.pass` (the render thread's per-frame CPU zones) and `render.gpu` (the GPU's); see above |

### Messages

| Name | Meaning |
| --- | --- |
| `direct_scan.completed` | the scan produced a cube |
| `solve.started`, `solve.finished` | the solve worker's bounds |
| `render.gpu_unavailable` | the window's graphics backend has no GPU zone (once per run) |
| `render.scenegraph_initialized`, `render.scenegraph_invalidated` | the window's scene graph created / torn down |
| `qt.scenegraph.time.texture: ...`, `qt.scenegraph.time.compilation: ...` | a texture upload / a shader compile, as Qt logged it |

The plots are `render.frame_ms`, `capture.interval_ms` and the scene graph's
`qsg.*` numbers (see [The render pass](#the-render-pass-cpu-and-gpu)); `CUBE_PLOT`
remains available for the next per-frame statistic worth correlating with the
zones.

The trace's **information** window carries the build (`cube::version_details()`:
version, commit, branch, dirty flag), so a capture says what it measured.

### What is not instrumented

Code with no caller in the application carries no zones, which could never
fire. The old scan pipeline's observation, orientation-tracking and
capture-routing code was of that kind until it was removed (step R6 of the
[recovery plan](recovery-plan.md)); if something like it is wired back in,
instrument it then.

## Adding instrumentation

Include `<cube/profiling.hpp>` (it is on `cube::lib`'s public include path, so
every target already has it) and use the macros. Nothing else in Tracy's API
is available, on purpose: the macros are what compiles to nothing.

| Macro | Use |
| --- | --- |
| `CUBE_ZONE_SCOPED_N("layer.name")` | a zone for the rest of the C++ scope. The default choice. `CUBE_ZONE_SCOPED()` names it after the function |
| `CUBE_ZONE_BEGIN(var, "layer.name")` / `CUBE_ZONE_END(var)` | a span that is not one scope - several stages that share variables in one long function. Nothing needs it today; it is here, tested, because this family's other apps have it. Exception safe: the zone closes on unwind if `END` was not reached |
| `CUBE_ZONE_VALUE(var, n)` / `CUBE_ZONE_SCOPED_VALUE(n)` | attach an unsigned number (a count, a size) to the zone |
| `CUBE_FRAME_MARK()` / `CUBE_FRAME_MARK_NAMED("Name")` | frame boundary on the default / a named set |
| `CUBE_PLOT("name", n)` | a number on a timeline; integer, floating point or bool |
| `CUBE_MESSAGE("name")` | a discrete event; the name is a literal |
| `CUBE_MESSAGE_DYNAMIC(view)` | the same for text built at run time (Tracy copies it). Pass a variable, not a braced initialiser: the macro would split it at its comma. Only the Tracy variant records it |
| `CUBE_THREAD_NAME("name")` | name the calling thread, **once** per thread - every call adds a table entry, so never in a per-frame or per-task path |
| `CUBE_APP_INFO(text)` | free text for the trace's information window |
| `CUBE_SECTION_ENTER("name")` / `CUBE_SECTION_LEAVE(id)` | an execution section (Tracy 0.14): a named region the GUI can filter by. Tracy copies the name; keep the id and hand it to `LEAVE` |

The render thread's two tools are classes, not macros, so they can span two
signal handlers (the same surface exists, as no-ops, in the other variants), and
the switch's is a class too:

| Tool | Use |
| --- | --- |
| `cube::profiling::ZoneSpan` | a CPU zone whose begin and end are not one C++ scope - `begin("render.frame")` in `beforeRendering`, `end()` in `afterRendering`. Exception safe and idempotent like a manual zone |
| `cube::profiling::GpuContext` | the Tracy GPU context behind the render pass (`create_opengl(gl_gpu_functions)`, `create_d3d11(device, device_context)`, `create_vulkan(vulkan_gpu_handles)`); `collect`, then `begin_zone`/`end_zone` around the pass, all on the thread that created it. The implementations are `src/lib/src/profile/tracy_gpu.cpp` (OpenGL, or `tracy_gpu_opengl_none.cpp`), `src/lib/src/profile/tracy_gpu_vulkan.cpp` (Vulkan, or `tracy_gpu_vulkan_none.cpp`), and `windows/tracy_gpu_d3d11_windows.cpp` or `posix/tracy_gpu_direct3d_none.cpp` - CMake compiles exactly one of each pair, the way the platform fragments pick per-OS files - all built only in a Tracy build. `RenderProfiler` finds the handles; `render_profiler_vulkan.cpp` does it for Vulkan |
| `cube::profile::Session` / `decide_startup` | the run-time switch (`cube/profile/session.hpp`): whether this process starts the profiler. Only `main()` and the perf tier's listener construct one |

Rules that keep it honest:

- **Names are string literals.** Tracy stores the pointer, not a copy.
- **Arguments are not evaluated when profiling is off.** Keep what goes into
  `CUBE_PLOT` / `CUBE_ZONE_VALUE` free of side effects, and never let it be the
  only use of a variable (an unused-variable warning is an error under
  `-Werror`, in exactly the build nobody profiles).
- **Put a zone where there is work, not on a tight inner loop.** A zone costs
  about 25 nanoseconds while a profiler is attached and a nanosecond or so
  otherwise ([Idle cost](#idle-cost)). A pass is milliseconds; a per-cell zone inside it is noise.
- **Zones that can return early on the idle path** (a poll that finds an empty
  mailbox) go *after* the early return, or the trace fills with zero-length
  zones.
- **A manual zone is for the one function that needs it.** Prefer the scoped
  form, and wrap a statement in a lambda or block when you need to isolate it.

## Idle cost

Measured with `cube_perf_tests "[.perf-report-zone-cost]"` (a Release build,
Windows, MSVC; the fastest of seven runs of 20 million iterations, minus the
same loop without the macro):

| State | `CUBE_ZONE_SCOPED_N` | `CUBE_PLOT` | `CUBE_FRAME_MARK_NAMED` |
| --- | --- | --- | --- |
| Tracy compiled out (`-DCUBEULATOR_ENABLE_TRACY=OFF`) | 0 (the macro is empty; the 0.2 ns the report shows is the loop's own noise) | 0 | 0 |
| **compiled in, dormant** (a Release build nobody asked to profile) | **0.7 ns** | 0.06 ns | 0.2 ns |
| started, no profiler attached (`CUBEULATOR_PROFILE=1`) | 1.7 ns | 0.9 ns | 1.3 ns |
| started, a capture attached and recording | 24 ns | 14 ns | 30 ns |

A dormant zone is about two cycles. The performance tier's own timing report
(`[.perf-report-timing]`: the solver, the n x n solver, the reader and the
scan, as ratios to the calibration workload) was run five times alternately
against each state and the medians compared: every difference was inside the
run-to-run noise of the same binary (about 5%, and in both directions), with
the solver - the most densely instrumented code - at +4.6% dormant, +9.3%
started and +12.3% recording on `advanced_solve` and below zero on the others.
The honest reading is "not measurable by that instrument"; the table above is
the measurement.

The rest of a dormant process is what is *not* there: no Tracy thread, no
socket and no broadcast (checked: a Release `Cubeulator.exe` with no switch has
36 threads and no UDP endpoint; with `--profile` it has 39 threads, the TCP
listener on the loopback address only, and a UDP endpoint), no `RenderProfiler`,
no Qt logging category turned on. A connection from the machine's LAN address
to the data port is refused with the profiler running.

What a Release build pays that a Tracy-less one does not is **size**: the
Tracy client library (`TracyClient.dll` on Windows, 139 KB) is shipped, and each
instrumented function carries the check. That is the price of being able to
turn profiling on in a shipped build without a rebuild; if it ever matters,
`-DCUBEULATOR_ENABLE_TRACY=OFF` for the release preset gives it back, and the
alternative is a second, profiling-enabled release artifact built in CI.

## Holding a zone to a baseline

The zone names above are the interface two more things read. The performance
tier's budgets count what the solver and the reader *do* (search nodes, pixel
buffers) with no profiler in the loop, and `tools/perf/tracy_check.py` runs the
headless `cube_perf_tests` under `tracy-capture` and compares each zone's call
count and fastest call, as a multiple of the test binary's own
`perf.calibration` zone, with `tools/perf/tracy_baseline.json`. Renaming a zone
is therefore a deliberate act that updates that file as well. Both are
described in [Performance tests](testing.md#performance-tests).

## In the browser

Tracy cannot be used on the web build, and not for want of a port: a Tracy
client **opens a TCP listener** and answers UDP discovery, and a browser tab can
do neither. What takes its place is a recorder inside the page.

The same macros (`CUBE_ZONE_SCOPED`, `CUBE_PLOT`, `CUBE_FRAME_MARK_NAMED`, ...)
have a third implementation, chosen the same way as the other two: by CMake,
with `-DCUBEULATOR_ENABLE_PROFILE_RECORDER=ON`, which puts
`src/lib/variants/profiling-recorder/` on the include path.
Zones, frames, messages and plots then go to `cube::profile::Recorder`
(`cube/profile/recorder.hpp`): a fixed ring of lock-free slots, two clock reads
and a few relaxed atomic stores per zone, nothing allocated and nothing locked
while recording. It is plain C++ and is built into `cube::lib` on every
platform, so it is unit-tested natively (`profile_recorder_test.cpp`); only its
export to the page is web code (`src/app/platform/web/profile_web.{cpp,js}`).

```sh
cmake --preset web-emscripten-diagnostics
cmake --build --preset web-emscripten-diagnostics
```

That preset is Release with function names kept (`--profiling-funcs`: a trace
reads `cube::solver::detail::bfs_search`, not `wasm-function[4171]`, without the
size of full DWARF), the recorder, and the read-only driver the
[browser tests](testing.md#browser-tests) use. It is for measuring; the site
that is published is the plain `web-emscripten-release`.

### Reading it

The page gets `window.cubeulatorProfile`:

| | |
|---|---|
| `recording` | `true` in this build; `false` in any other, where everything below is empty |
| `summary()` | per zone: `count`, `total_ms`, `mean_ms`, `min_ms`, `max_ms`, `stddev_ms`; and how many frames and messages of each name |
| `trace()` | Chrome's Trace Event Format, as text |
| `csv()` | zone statistics in `tracy-csvexport`'s columns, so `tools/perf/tracy_check.py` and any script written against a Tracy export read it unchanged |
| `clear()` | forget what has been recorded (the ring holds 131 072 events, about a quarter of an hour of a scan, then overwrites the oldest and counts them in `dropped`) |
| `mark(text)` | a message on the timeline and a `performance.mark` of the same name: where a step begins |
| `now()` | the recorder's clock, microseconds |
| `memory()` | the wasm heap's size, and in Chromium the JavaScript heap's |
| `download('trace' \| 'csv' \| 'summary')` | saves it as a file |

From the DevTools console:

```js
cubeulatorProfile.summary().zones['solver.solve']
copy(cubeulatorProfile.trace())      // or download('trace')
```

A trace opens in **DevTools > Performance > Load profile**, in
[Perfetto](https://ui.perfetto.dev), and, through Tracy's `tracy-import-chrome`,
in the Tracy GUI - so the one place the family's habit of reading a timeline in
Tracy still works.

### What to keep in mind

- **The clock is `performance.now()`.** A page that is cross-origin isolated
  (this one must be, for its threads) gets it at 5 microseconds; a zone shorter
  than that reads as zero.
- **DevTools open slows the page down**, and a page not in front is throttled.
  Record with it closed and read the numbers from `summary()`, or record from a
  script (Playwright's `page.evaluate`, which is what the suite does).
- **The first runs are not the steady state**: WebAssembly is compiled by a
  baseline tier first and optimised in the background. A number worth keeping is
  taken after a warm-up, as the tests do.
- **A headless runner draws in software** (SwiftShader), so frame times there
  measure the CPU renderer; the solver and reader zones are CPU in any case.
  A test that reads a frame time asserts the renderer first.
- **Threads are Web Workers**: each shows as its own `thread N` track; the main
  thread is the page's.
- **Nothing here is shipped.** The ring is not allocated unless something
  records into it, but the build is a development build like the Tracy one.

The performance tests that read this - budgets on how long a solve and a
scan take in the browser, and the report each run leaves - are in
[Browser tests](testing.md#browser-tests).

## Licence

Tracy is BSD-3-Clause. Since the client is built into the desktop applications
it **is** a shipped component: it is linked into `Cubeulator` (on Windows as
`TracyClient.dll`, which the installer's runtime-dependency step copies like
OpenCV's DLLs) and appears in the generated
[third-party notices](licensing.md#6-how-the-notices-are-produced) with its
licence text - nothing is added by hand; the notices are derived from the
application's link line, so a version change shows up in them by itself
(`tracy 0.14.1#2`: the port version is part of the string). `capstone`, `ppqsort`
and `zstd`, which the port depends on for its command-line tools, are not
dragged in: the tools are a separate `profiling-tools` feature that a shipped
build does not select. `vulkan-headers` is header-only and links nothing.

## Why it is built this way

- **CMake chooses the header, not an `#ifdef`.** `cube/profiling.hpp` exists
  three times, under `src/lib/variants/profiling-tracy_{enabled,disabled}/` and
  `profiling-recorder/`, and `src/lib/CMakeLists.txt` puts one directory on the
  include path - the
  mechanism ICL Forge uses for the same seam, and the one
  [architecture.md](architecture.md) already requires of platform
  differences. A unit test (`profiling_test.cpp`) pins the variant against the
  option, so the wrong directory cannot be selected unnoticed. The same
  principle decides which graphics backends are built: the platform fragment
  lists the Direct3D 11 source (Windows only) and says whether the platform has
  Vulkan (`CUBEULATOR_PLATFORM_PROFILING_VULKAN`); no source tests the OS.
- **Built into every desktop binary, dormant until asked.** The request was a
  profiler that can be turned on in a *shipped* release build to diagnose an
  issue, with no recompile and no cost while idle. The only way to have that is
  to ship the client and decide at launch. Tracy's *manual lifetime* is what
  makes the "dormant" real: without it the client starts a thread and opens
  a socket in a static initialiser, in every process, whether or not anyone
  asked. The price is a vcpkg overlay port for the two Tracy options the
  registry port does not expose (`vcpkg-overlay-ports/tracy`: `manual-lifetime`
  and `only-localhost`, each a single line in a copy of the registry's
  portfile); when the vcpkg baseline moves, re-copy it.
- **The switch is the environment and the command line, not a settings
  toggle.** A setting would need a running application to change it, and a
  user with a problem is not going to find a menu; an environment variable can
  be put in a shortcut or a service file, a flag in a command, and both can be
  asked of a user in one sentence. Both are read once, so there is no state to
  go stale.
- **Loopback only.** A profiler that a shipped binary can be made to open must
  not be reachable from the network; `TRACY_ONLY_LOCALHOST` is a compile-time
  switch of the client, so it is built in. Remote machines use an SSH tunnel.
- **An opt-in vcpkg feature, not a base dependency.** `opencv4` already makes
  a cold build heavy; `-DCUBEULATOR_ENABLE_TRACY=OFF` never resolves Tracy, and
  the tools (`capstone`, `zstd`, ...) are a second feature for the people who
  take traces.
- **Tracy's crash handler is off** (`default-features: false` drops the port's
  `crash-handler`). On Windows it installs an unhandled-exception filter *when
  a profiler connects* - after `main()` has installed the app's own minidump
  writer - which replaces it, suspends every other thread and does not chain
  to what it replaced. The app already owns crash reporting, and a hard fault
  while someone is profiling is exactly when `cube_app_crash.dmp` has to be
  written. A fault still ends the capture; you lose Tracy's crash callstack,
  not the minidump.
- **On-demand.** Zones are only recorded while a profiler is attached, which
  keeps an idle instrumented build close to a normal one and means the
  instrumentation never has to be removed to run it.
- **The profiler is shut down last.** `cube::profile::Session` is the first
  object in `main()`, so it is destroyed after every worker thread has stopped
  (a small guard waits for the global thread pool first): Tracy must not be torn
  down while a thread can still write a zone.
- **No sanitizers, no coverage** in a build you take numbers from. Both would
  be what you measured.
