# In your browser

Cubeulator runs in a web browser, as WebAssembly, alongside the Windows, macOS and Linux apps. It is the same app: the
same solver, the same camera reader and the same screens, built from the same source.

[Open Cubeulator](../../app/){ .md-button .md-button--primary target=_blank rel=noopener }

It opens in its own tab, because it wants the whole window and, on a phone, the whole screen.

No camera to hand? [Open a scrambled cube](../../app/?demo=7){ target=_blank rel=noopener } to see the solve screen, with a
solution already worked out. Any number after `demo=` is a different scramble, and `&size=5` makes it a 5x5 (2 to 9).

What the screens are and how to use them is in the rest of the [user guide](index.md); this page is about running it in a
browser.

## What you need

- A current Chrome, Edge, Firefox or Safari. The page needs WebAssembly
  threads, WebAssembly SIMD and WebGL 2; Safari has had all three since 16.4.
- A camera, if you want to scan a cube. A phone's rear camera is the one to
  use; a laptop's webcam works too.
- A secure page. The camera is only available to `https://` pages (and to
  `localhost`), which is how this one is served.
- Service workers allowed for the site. Some browsers turn them off in
  private windows; the page says so when it cannot start (see
  [below](#how-it-works-in-short) for why it needs one).

The first visit downloads the app and then reloads the page once. Later visits
start from the browser's cache.

## Privacy

The page is static files. There is no server for the app to talk to, so
nothing it sees leaves your device: the camera frames are read, and the cube
solved, inside the page, in your browser. The browser asks for camera access
the first time the app opens the camera, shows its own indicator while the
camera is on, and you can withdraw the permission from the site settings in
the address bar at any time. The app's settings are kept in your browser's
local storage for this site, and nowhere else.

## Differences from the desktop apps

| | Desktop apps | In the browser |
|---|---|---|
| **Camera permission** | The OS asks the first time a camera is opened | The browser asks the same way. It also hides the cameras' names until you have allowed one, so the list says *Back camera* and *Front camera* until then. On a phone the scan opens the back camera, and a **Switch camera** button on the scan screen changes to the front one and back |
| **Exposure and white balance** | Locked for a scan where the camera allows it (Windows) | Locked for a scan where the browser and the camera both allow it, and not otherwise. Chrome and Edge ask the camera to hold them when it lists a manual exposure or white balance, and check that it did; Firefox and Safari do not offer a page these controls, and whether a given webcam or phone camera does is up to its driver. Where it is not held the camera's own automatics run and colours can drift between sides under changing light, so scan under steady, even light. What the camera allowed is in the [diagnostics](https://github.com/iainchesworthlabs/cubeulator/blob/develop/docs/diagnostics.md) record |
| **Settings** | Kept between launches | Kept in the browser's local storage for this site: they survive a reload and are lost with the site's data |
| **Text** | The system's fonts | A browser has no system fonts, so the app carries its own Japanese and Simplified Chinese faces (Noto Sans JP and SC, cut down to the characters of the Japanese and Chinese translations and the language names: about 350 and 270 kilobytes). Everything else, accented German, French and Spanish letters included, is drawn with the face Qt bundles |
| **Speed** | Native | The solver and the reader are WebAssembly. Expect the same results, somewhat slower |

## How it works, in short

The app is built for the browser with [Emscripten](https://emscripten.org/) and Qt for WebAssembly, and drawn with WebGL. The
camera is the browser's `getUserMedia()`. Threads are Web Workers on shared memory, which browsers allow only for a page
that is served with two headers that isolate it from other sites. A static host cannot always send them, so the app
carries a small service worker that adds them to the app's own files and reloads the page once; its scope is the `/app/`
folder, so nothing else on this site is touched. The scan's reader runs on the page's main thread, so a slow frame is one the
page does not draw. How the build is made and measured is in the
[developer notes](https://github.com/iainchesworthlabs/cubeulator/blob/develop/docs/web.md).

## Hosting it yourself

Each release carries the same site as `Cubeulator-<version>-web.zip`: the
files above and nothing else, with the licence, the third-party notices and a
`HOSTING.txt` that says what a host has to do. Unzip it behind any static
server, over https:

```sh
unzip Cubeulator-<version>-web.zip
python3 -m http.server 8000 --directory Cubeulator-<version>-web
# then open http://localhost:8000/
```

The zip is built by the same workflow, from the same commit, as the
installers, and is covered by the release's `SHA512SUMS`, GPG signatures and
provenance attestation (see [Verifying a download](verify.md)).
It is what the hosted copy serves, so the two behave alike.
