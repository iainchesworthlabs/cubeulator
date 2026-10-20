# Graphics API

A Qt Quick window draws through one graphics API for the whole life of the
process (Vulkan, Direct3D 11 or 12, OpenGL, Metal). Cubeulator chooses it once,
at launch, and the choice is **guarded**: a machine that cannot use the API it
asked for goes on to the next one instead of failing to start.

- [Choosing the API](#choosing-the-api): the person's controls and the order
  they are read in
- [What the default is](#what-the-default-is) on each platform
- [The guard](#the-guard): how a launch that cannot use its API falls back
- [Diagnosing](#diagnosing)
- [Why Vulkan and why not](#vulkan-against-direct3d-11-measured): the
  measurements behind the Windows default
- [Where it is not verified](#what-has-not-been-verified)

## Choosing the API

In order, strongest first (`cube/graphics/backend_choice.hpp`):

| Where | How | Lasts |
| --- | --- | --- |
| The command line | `Cubeulator.exe --graphics-api=vulkan` (`--graphics-api auto` also works) | that launch |
| The environment | `CUBEULATOR_GRAPHICS_API=vulkan` | that launch |
| Qt's own variable | `QSG_RHI_BACKEND=vulkan` | that launch |
| **The Settings screen** | *Rendering*: Auto, then what the platform offers (not `software`) | every launch, from the next |
| The platform's automatic chain | nothing asked | - |

The keys are `d3d11`, `d3d12`, `vulkan`, `opengl`, `metal` and `software`
(each platform offers the ones that exist on it; the Settings screen lists them, except `software`, which is the floor of a chain and for the command line).
`auto` at any of the first three stops the search there, which is how one launch
overrides the picker. A key the platform does not offer - a typo, or `d3d11` on
Linux - is ignored with a line in the log, never obeyed.

`software` is the platform's own software rasteriser. On Windows it is Direct3D 11
on WARP (`QSG_RHI_PREFER_SOFTWARE_RENDERER`); Qt Quick 3D, which draws the cube,
has no software scene graph of its own, so this is the only way to get a window on
a machine with no working GPU driver. It is not a chip in Settings: a chip row
that long does not fit the wide Settings screen in Hebrew without drawing over
the next row, and a person who needs it is not in a position to open Settings.

The API is read at launch. Changing the picker takes effect on the next start,
and the Settings screen says so when what is running differs from what is chosen.

## What the default is

Each platform lists, in `platform_info::current()`
(`automatic_graphics_backends`), the chain "auto" tries, best first. The first
that can start here is used; each later one is where the application falls back.

| Platform | Automatic chain | Notes |
| --- | --- | --- |
| Windows | Vulkan, Direct3D 11, software | Direct3D 12 is left out on purpose ([measured](#vulkan-against-direct3d-11-measured)) |
| Linux | Vulkan, OpenGL | not run on a Linux machine yet |
| macOS, Android, iOS, web | *(Qt's own default)* | not guarded: Metal, OpenGL ES and WebGL are the only sensible choice there |

A platform with an empty chain behaves as it did before this existed. The last
entry of a chain is the **floor**: nothing recorded can remove it, because there
is nothing below it.

## The guard

The API cannot be changed once the window exists, and Qt does not always fail
cleanly when an API is unavailable - so the choice is checked three ways
(`src/app/desktop/graphics_backend.cpp`):

1. **A probe before anything is created.** `interfaces/graphics_probe.hpp`
   (one implementation per platform) asks whether a backend can start, without
   Qt and without a window: on Windows it loads `vulkan-1.dll` and creates a
   Vulkan instance and counts its GPUs, and creates (and discards) a Direct3D 12
   or 11 device; on Linux it opens `libvulkan.so.1` the same way. A backend that
   says no is skipped, down the chain. This is the check that matters most: asked
   for Vulkan with no loader, or a loader with no driver, Qt 6.12 logs "Failed to
   create platform Vulkan instance" and then **crashes** (an access violation,
   no error signal) before the first frame. The probe turns that into a
   fallback with no crash.
2. **A window that reports it cannot be created.** If the scene graph reports
   an error before the first frame (`QQuickWindow::sceneGraphError`) the
   application records the failure and starts itself again, which then goes
   past that backend (`--graphics-skip=`, added to the relaunch, makes that hold
   even when the API was named on the command line).
3. **A launch that never draws.** Each launch writes the API it set out with
   under `rendering/guard/pending` and clears it when the first frame has been
   presented or the application quits normally. A launch that finds it still set
   knows the last one crashed, hung or was killed before it could draw. That
   is a strike against that API; the **second strike in a row** enters it in
   `rendering/guard/failed`, and every later launch goes past it. Two strikes,
   not one, because a second copy of the application started while the first is
   still opening its window looks identical.

A recorded failure is **forgotten** when

- the application is another build (a new version, a new commit, a new Qt);
- seven days have passed (a driver update is the usual cure);
- the person changes the Settings picker (they have said what they want, so it
  is tried again); or
- the API draws a frame.

An API named on the command line or in `CUBEULATOR_GRAPHICS_API` is tried
whatever is on file, which is how a person retries one by hand.

Where the settings live: `HKEY_CURRENT_USER\Software\Cubeulator\Cubeulator\rendering`
on Windows (the picker is `graphicsBackend`, the guard is under `guard`), the
usual `QSettings` place elsewhere. Deleting `guard` forgets everything.

## Diagnosing

The application logs one line for the API it set out with and one for every
backend it skipped, and why:

```
graphics: skipped 'vulkan': it cannot start on this machine
graphics: d3d11 (the Settings screen)
```

(On Windows the program has no console; run it with `QT_ASSUME_STDERR_HAS_CONSOLE=1`
and its standard error redirected, or look at Debug output.) `QSG_INFO=1` adds
Qt's own account of the device it created: the adapter, the swap chain, the
pipeline cache. The Settings screen shows the API the window really uses.

## Vulkan against Direct3D 11, measured

An RTX 4090 (Nvidia driver 617.42, Windows 11), Qt 6.12.0, a profiling build of
the application (RelWithDebInfo), `PriorityClass` High, runs alternated between
the APIs. The machine was shared with other sessions' builds, which is what the
spread in the first numbers is. Raw runs are kept with the PR; they are
single-machine numbers.

**Start-up** (the real application, Tracy, six alternated launches of each of
three screens, first one dropped; time from the start of `main()`):

| | Direct3D 11 | Vulkan |
| --- | --- | --- |
| first frame presented, best / median of 15 | 1.11 s / 1.18 s | 1.25 s / 1.36 s |
| the first frame itself (shaders, pipelines) | 115 to 147 ms | 6 to 12 ms (warm cache); 63 ms (very first run) |
| frames over 8 ms in the first 3 s, summed, median (Home / solve 3x3 / 6x6) | 440 / 362 / 295 ms | 303 / 464 / 341 ms |

Vulkan arrives **130 to 180 ms later** at the first frame - the device creation
the performance notes put at about 200 ms - and then draws that first frame in a
tenth of the time, because Qt saves a pipeline cache between runs
(`qtpipelinecache/qqpc_vulkan`, 300 KB) and Direct3D 11 has nothing to save. But
the application's heavy frames after it are the cube's first 3D frames, 120 to
165 ms each on either API, and they are not shorter on Vulkan, so **in the
application there is no start-up saving to set against the later first frame**:
the menu and the cube are on screen about 130 ms later on Vulkan. (The saving
exists in the harness: the `[screen-cost]` case, a fresh window drawing the whole
scene at once, takes 1.88 s to the tenth frame of the launch screen on Direct3D
11 and 0.45 s on Vulkan, median of the 8 alternated runs. The application builds
the cube a frame after the menu, which is not what the harness measures.)

**Steady frames** (`render.rhi_frame`, 3 s after the first frame, medians): Home
1.30 ms Direct3D 11 / 1.12 ms Vulkan; solve 3x3 1.08 / 1.20; solve 6x6 0.39 /
0.49. GPU time per frame (`render.gpu`) is lower on Vulkan in all three
(0.027 / 0.051 / 0.046 ms against 0.063 / 0.080 / 0.110). The frame cost is
the same to within what the machine's noise allows; Vulkan does draw more
frames on the Home screen's turning cube (about 60 a second against about 40)
for a reason not looked into.

**Screens** (`[screen-cost]`, the real window with the fake camera, medians of
the 8 alternated runs, ms): arriving on a screen and ten frames after it are
within 5 % either way except History (tenth frame 137 ms Direct3D 11 against 164
Vulkan) and the 6x6 solve screen (185 against 228); Settings is 252 against 222. The scan screen,
which uploads the camera's picture every frame, can draw **60 frames a second
on Direct3D 11 and 81 on Vulkan** when every swap asks for the next. The harness
camera is a drawn 640 x 360 picture, not a webcam's 1080p, so this is the
direction of the upload cost and not its size.

**Memory** (the process after 10 s): working set the same (about 410 MB);
private bytes 60 to 100 MB higher on Vulkan (572 against 513 MB on Home); GPU
dedicated memory 90 to 100 MB higher (138 against 46 MB on Home, 176 against 75
on the 3x3 solve screen).

**Direct3D 12** is worse than either and is not in the chain: the tenth frame
after arriving on the launch screen comes 3.0 s in (3.7 s for the process's
first, 6.5 s in an earlier run), on Settings 4.3 s, on the 3x3 solve screen 3.1
s, with gaps of 1.6 to 2.7 s between frames (Direct3D 11: 0.17 to 0.28 s). It stays in the Settings picker for anyone who wants it.

**Software (Direct3D 11 on WARP)** works as the floor: in the harness's small
window the launch screen's tenth frame came 172 ms after arriving and every
screen was usable. It is a CPU rasteriser; a full-size window will be slower.

### Driver coverage

What happens when the API's own pieces are missing, tried here by making them
missing:

| Situation | Without the probe | With it |
| --- | --- | --- |
| `vulkan-1.dll` missing or not loadable | Qt logs "Failed to load vulkan-1", then **access violation** before the first frame (reproduced with a stray invalid `vulkan-1.dll`) | Vulkan skipped, Direct3D 11 used |
| Loader present, no Vulkan driver (`VK_DRIVER_FILES` pointing at nothing, which is what a machine with a loader and no GPU driver looks like to the loader) | the same crash | Vulkan skipped |
| A driver or implicit layer that crashes inside the probe or the first frame | the application crashes at every launch | two strikes, then skipped for seven days or until the build changes |

A Windows machine usually has `vulkan-1.dll` because the GPU vendor's driver
installs it; the ones that do not are virtual machines, remote sessions that
use a basic display adapter, Server editions without a GPU, very old integrated
GPUs (Vulkan arrived on Intel with Skylake and on Nvidia with Kepler), and
Windows on ARM, where the Vulkan driver for Qualcomm GPUs is an optional store
package that translates Vulkan to Direct3D 12. Every one of those is the probe
saying no, and the application opening on Direct3D 11.

### What has not been verified

One machine, one GPU (RTX 4090) and one driver. Not tried: AMD, Intel or older
Nvidia GPUs, an integrated GPU, a laptop with switchable graphics, a remote
desktop session, a virtual machine, Windows on ARM, or any Linux or macOS
machine (the Linux probe is written against the same code but was not run). The
guard's first and third layers are tested here only by simulation (a missing
loader, a loader without a driver, a process killed 0.4 s into its start).
