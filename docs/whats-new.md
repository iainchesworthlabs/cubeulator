---
template: marketing.html
layout: prose
title: What's new
title_tag: "What's new in Cubeulator"
description: "What each Cubeulator version changed. The latest is 1.0.0-rc.1, the release candidate for 1.0."
---
# What's new

The latest version is **1.0.0-rc.1**, of 25 November 2026, the release candidate for 1.0. Download it from the
[Releases page](https://github.com/iainchesworthlabs/cubeulator/releases). The full record, with every change, is the
[changelog](https://github.com/iainchesworthlabs/cubeulator/blob/main/CHANGELOG.md).

## 1.0.0-rc.1 (25 November 2026)

The release candidate for 1.0. If nothing turns up that needs fixing, 1.0 follows with these changes.

- **The whole app works from the keyboard.** <kbd>Tab</kbd> moves between the buttons, choices and switches, <kbd>Enter</kbd> or <kbd>Space</kbd> presses one, and a ring shows where you are. <kbd>Esc</kbd> goes back on Settings, cancels a scan and says *Not now* on the camera permission screen.
- **Screen readers get names and headings for every screen and control,** and are told when the scan's guidance, a turn of the solve or a mistake in a typed scramble changes.
- **The app moves less when you ask it to.** Settings has a **Motion** row (the same as this device, reduced, or full), and by default the app follows the device's own request.
- **Every word of the scan and solve screens is in the language you chose,** in all nine languages. The solve screen's stages are named *Yellow top* and *Top layer in order*, as the solution screen's are.
- **About and Licences can be opened on the iPhone,** and the About screen no longer says Cubeulator is for the 3x3 only.
- **Cancelling a scan as the camera opens lets go of the camera,** and the scan size row marks only Auto while it is finding the size.
- **Allowing the camera on the iPhone takes you to the scan.** Before, the app kept saying the camera was off even after you allowed it.
- **Borderlands Cel-Shading no longer draws a pale square behind the cube.** The cube is drawn on the page again, with the halftone dots and the dark edges still in place.
- **Downloads are signed and can be checked.** Every file carries a GPG signature, and each release has build attestations and bills of materials. The signing key is published with the source; [checking a download](guide/verify.md) says how.

## 0.2.0-beta.2 (31 October 2026)

- **A side held the wrong way up is found and turned back for you** on the colours screen, and the screen offers the
  fewest turns that make a cube.
- **The scan screen marks a corner** when it sees two or three sides at once, with a coloured dot on each sticker.
- **The camera stays on a cube whose colours you corrected.**
- **A noisy camera frame no longer stalls the scan.**
- **The Linux package starts on Ubuntu 24.04 and Debian 13** again.
- **The iPhone app no longer closes as it opens**, and goes to TestFlight for the author's invited testers.
- **The About screen says version 3 of the GPL**, because the program includes a part that Qt offers only under version 3.

## 0.2.0-beta.1 (22 October 2026)

- **Another size of cube.** A 2×2 to a 5×5 can be scanned, or typed in as a scramble or a colour at a time, and followed
  turn by turn on a 3D cube drawn at its size. The camera follows a bigger cube's turns as it does a 3×3.
- **Put right what the camera misread.** *Check the colours* opens what it read, side by side, from the scan or from the
  solve screen, instead of making you scan again.
- **About and Licences are real screens**, in all nine languages, and the app carries its own type.
- **Screens with the 3D cube come up about a third faster**, and the scan finds a bigger cube's sides in more frames.
- **The browser version opens a phone's back camera** and can switch cameras.
- **A first iPhone build**, for the author's registered devices.

## 0.1.0-beta.1 (15 October 2026)

The first release. Hold each side of a scrambled cube up to the camera and it works out the cube, solves it and shows you
the solution turn by turn on a 3D cube, on Windows, macOS and Linux, and in a browser. Red and orange are told apart under
warm light by letting the cube decide: only one naming of the colours makes a cube that can exist.
