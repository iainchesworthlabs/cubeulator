---
template: marketing.html
layout: prose
title: What's new
title_tag: "What's new in Cubeulator"
description: "What each Cubeulator release changed, and what is not yet proven. The latest is 0.2.0-beta.4, a pre-release."
---
# What's new

The latest release is **0.2.0-beta.4**, of 1 November 2026. Every release so far is a pre-release: download one from the
[Releases page](https://github.com/iainchesworthlabs/cubeulator/releases). The full record, with every change, is the
[changelog](https://github.com/iainchesworthlabs/cubeulator/blob/main/CHANGELOG.md).

<div class="cu-note" markdown="1">
**What is not yet proven.** The scan has been checked on recorded sessions and on a webcam, not across many cameras,
rooms and cubes. The camera is proven on a 3×3 and much less on the other sizes. The iPhone app has been seen to open and
go on to the scan; the scan itself, the camera's colours and the 3D cube's other looks have not been tried on a phone by
anyone but the author. The installers are not signed with a Windows or Apple certificate, so SmartScreen and Gatekeeper
ask before they run one; [checking a download](guide/verify.md) is how to be sure of one.
</div>

## 0.2.0-beta.4 (1 November 2026)

Another build for the iPhone. With Borderlands Cel-Shading chosen, the 3D cube no longer sits on a pale square: it is
drawn on the page again, with the halftone dots and the dark edges still in place.

## 0.2.0-beta.3 (1 November 2026)

A second build for the iPhone. Allowing the camera now takes you to the scan. Before, the app kept saying the camera was
off even after you allowed it.

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
