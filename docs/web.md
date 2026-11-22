# The web build

How Cubeulator runs in a browser, as WebAssembly: how it is built, why it needs a service worker, and how it is measured and
tested. For what a person using it needs and sees, and how to host the files, read
[In your browser](guide/web.md) in the user guide.

The app is the same app as the desktop ones: the same solver, the same camera reader and the same screens, built from the
same source.

## How it works

The app is built for the browser with [Emscripten](https://emscripten.org/)
and Qt for WebAssembly, and drawn with WebGL. In source it is one more
platform in `src/app/platform/` (`web/`), selected by CMake like the others -
see [Platform isolation](platform-isolation.md). What differs from the
desktops is small:

- **The camera** is the browser's `getUserMedia()`. A `<video>` element is
  drawn to a canvas, scaled to the 640 px the reader works at, and copied into
  the same BGR frame every other platform hands over.
- **Threads** are Web Workers on shared memory, which browsers only allow for
  a page that is *cross-origin isolated*: served with
  `Cross-Origin-Opener-Policy: same-origin` and
  `Cross-Origin-Embedder-Policy: require-corp`. GitHub Pages cannot send
  headers, so the app ships a small service worker
  (`cubeulator-isolation.js`) that adds the two to the app's own files and
  reloads the page once. A host that sends the headers itself needs none of
  it, and the worker steps aside. Its scope is the `/app/` directory: nothing
  else on this site is touched.
- **Nothing else** is platform code: the solver, the vision pipeline and the
  QML are the shared ones.

One consequence is worth knowing: the scan's reader runs on the page's **main
thread** (the app's GUI thread is the browser's), so a frame that takes longer
than the camera's 33 ms is one the page does not draw, and the longest ones
freeze it. What a frame costs there, measured, is on
[the investigation page](investigations/2026-10-15-web-scan-frame-cost.md); it
is held by a ceiling in the browser performance tests
([Performance in the browser](testing.md#performance-in-the-browser)).

## Building it yourself

See [Building & packaging](building.md#webassembly-browser) for the preset,
the Emscripten and Qt versions it needs, and how to serve the result locally.

## Measuring it and testing it

The browser cannot be profiled with Tracy (a Tracy client opens a TCP listener
and a tab has none to offer), so the build has its own: the `diagnostics`
preset records the same zones in the page and hands them over as a Chrome trace
(`cubeulatorProfile.trace()`), a CSV and a summary - see
[Profiling](profiling.md#in-the-browser). What the app does in a page, and that
the library is right as WebAssembly, is tested on every change by the suites in
[Browser tests](testing.md#browser-tests).
