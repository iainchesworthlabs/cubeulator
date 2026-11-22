---
template: marketing.html
layout: prose
title: FAQ
title_tag: "FAQ: Cubeulator questions and answers"
description: "Answers to common questions about Cubeulator: which cubes it reads, what it needs, whether anything is uploaded, how many turns a solution takes, and why it is free software."
---
# FAQ

Something not working? [Troubleshooting](guide/troubleshooting.md) goes through what the screen says and what to do about it.

Does Cubeulator actually solve cubes?
:   Yes, on Windows, macOS and Linux, and in a browser: hold each side of a cube up to the camera and it solves it and shows you the turns. It is a pre-release. [What's new](whats-new.md) and the [user guide](guide/index.md#what-this-release-is) say what is and is not yet proven.

Is anything uploaded?
:   No. The camera picture is read on your device and not saved. There is no account, no telemetry, no crash upload and no update check. See [Privacy](privacy.md).

Which cubes does it read?
:   A standard 3×3 with dark (black) plastic between the stickers, in even light. A cube with white or light plastic, or with flush tiles and no visible gap, is not read yet. A 2×2 up to a 5×5 can be scanned too, but the camera is proven on a 3×3 and much less on the other sizes; you can also type a scramble or the colours in ([Another size of cube](guide/other-sizes.md)).

What do I need?
:   A cube, a camera (a laptop's webcam is enough), and steady, even light on the side you are holding. Windows 10 or later (64-bit), a Mac with Apple silicon on macOS 14.4 or later, Debian 13 or later, Ubuntu 24.04 or later, or Fedora, or a current browser with nothing to install. See [the user guide](guide/index.md#what-you-need).

Can I use it on a phone?
:   In the phone's browser, yes: [open it](guide/web.md) and use the rear camera. There is an iPhone app on TestFlight, but only for testers the author has invited ([the iPhone build](guide/iphone.md)). There is no Android app: the build exists, but its camera is not written, so it cannot scan.

Can I type the colours in by hand?
:   Yes. If the camera misreads a sticker, press *Check the colours* after the scan and paint over it, or type a whole side as letters. For a cube you have no scan of, choose *Another size of cube*, then *Enter colours by hand*. See [When the camera misreads a colour](guide/troubleshooting.md#when-the-camera-misreads-a-colour).

Why does it say "Windows protected your PC", or macOS will not open it?
:   The installers are not yet signed with a certificate from Microsoft or Apple. On Windows choose *More info*, then *Run anyway*. On macOS right-click the app and choose *Open*, or use *Open Anyway* in System Settings, Privacy & Security. Whether a file really came from this project is checked with the attestation and the GPG signature: [Verifying a download](guide/verify.md).

It does not read my cube.
:   It reads one side at a time, square on, in even light. Strong backlight (a window behind the cube), a cube with very light plastic and flush tiles, and a side shown at an angle are the cases it is not yet proven on. See [Scanning a cube](guide/scan.md) and [Troubleshooting](guide/troubleshooting.md#it-will-not-read-a-side).

How many turns is the solution?
:   It depends on the method: about 100 for *Beginner* (layer by layer, in steps you can learn), about 50 for *Intermediate* (CFOP, the way fast solvers do it) and about 20 for *Advanced* (the fewest, with no stages to learn). You can change method at any time and it carries on from where the cube stands. See [Following the solution](guide/solve.md#three-ways-to-solve-it).

Is there an update check?
:   No. Cubeulator never contacts a server. To update, download the newer package and install it over the old one; your settings are kept.

Which languages does it speak?
:   Nine: English, Deutsch, Français, Español, Japanese, Simplified Chinese, Hebrew, Arabic and Yiddish. English is complete; the other eight are complete first drafts that no fluent reader has been through, and some words on the solve screen are English in every language. See [Settings](guide/settings.md#language).

How do I report a problem?
:   Open an [issue on GitHub](https://github.com/iainchesworthlabs/cubeulator/issues). [Support](support.md) says what to put in it.

<span id="why-gpl"></span>Why GPL-3.0-or-later, if this is meant to reach app stores?
:   The source is GPL-3.0-or-later and the binaries are GPL-3.0-only (they include Qt Quick 3D, which Qt offers only under GPL-3.0-only or a commercial licence). The tension with the stores is written down, not settled, in [LICENSE-NOTE.md](https://github.com/iainchesworthlabs/cubeulator/blob/main/LICENSE-NOTE.md): the Apple stores are deferred, and the question returns before any Apple submission.
