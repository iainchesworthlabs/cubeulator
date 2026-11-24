---
template: marketing.html
layout: prose
title: What's new
title_tag: "What's new in Cubeulator"
description: "What each Cubeulator release changed, and what is not yet proven. The latest is 1.0.0-rc.1, a release candidate."
---
# What's new

The latest release is **1.0.0-rc.1**, of 25 November 2026, a release candidate for 1.0.0. Every release so far is a pre-release: download one from the
[Releases page](https://github.com/iainchesworthlabs/cubeulator/releases). The full record, with every change, is the
[changelog](https://github.com/iainchesworthlabs/cubeulator/blob/main/CHANGELOG.md).

<div class="cu-note" markdown="1">
**What is not yet proven.** The scan has been checked on recorded sessions and on a webcam, not across many cameras,
rooms and cubes. The camera is proven on a 3×3 and much less on the other sizes. The iPhone app has been seen to open and
go on to the scan; the scan itself, the camera's colours and the 3D cube's other looks have not been tried on a phone by
anyone but the author. The installers are not signed with a Windows or Apple certificate, so SmartScreen and Gatekeeper
ask before they run one; [checking a download](guide/verify.md) is how to be sure of one.
</div>

## 1.0.0-rc.1 (25 November 2026)

The first release candidate for 1.0.0. If nothing is found that needs fixing, 1.0.0 follows with these changes.

- **The keyboard works everywhere.** <kbd>Tab</kbd> moves between the buttons, choices and switches, <kbd>Enter</kbd> or <kbd>Space</kbd> presses one, and a ring shows where you are. <kbd>Esc</kbd> goes back on Settings, cancels a scan and says *Not now* on the camera permission screen.
- **A screen reader is given names, roles and headings,** and is told when the scan's guidance, a turn of the solve or a typed scramble's mistake changes. This has been checked by tests, not yet with a real screen reader.
- **The app moves less when you ask it to.** Settings has a **Motion** row (the same as this device, reduced, or full), and by default the app follows the device's own request.
- **Every word of the scan and solve screens is in the language you chose.** The eight other languages are still machine-made first drafts that no fluent reader has been through. The solve screen's stages are named *Yellow top* and *Top layer in order*, as the solution screen's are.
- **About and Licences can be opened on the iPhone,** and the About screen no longer says Cubeulator is for the 3x3 only.
- **Cancelling a scan as the camera opens lets go of the camera,** and the scan size row marks only Auto while it is finding the size.
- **Allowing the camera on the iPhone takes you to the scan.** Before, the app kept saying the camera was off even after you allowed it.
- **Borderlands Cel-Shading no longer draws a pale square behind the cube.** The cube is drawn on the page again, with the halftone dots and the dark edges still in place.
- **Releases are made by the organisation's shared release workflow.** The key that signs them is now in the repository (`docs/signing-key.asc`) and no longer attached to each release; [checking a download](guide/verify.md) says how.

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
