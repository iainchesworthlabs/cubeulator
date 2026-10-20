## Installing

**Windows.** Run `@WINDOWS@`. The installer is not code-signed yet, so Windows SmartScreen may say "Windows protected your PC": choose *More info*, then *Run anyway*.

**macOS.** Open `@MACOS@` and drag Cubeulator to Applications. The app carries an ad-hoc signature and is not notarised, so the first time, right-click it and choose *Open*, or use *Open Anyway* in System Settings > Privacy & Security.

**Linux.** `sudo apt install ./@DEB@` (Debian, Ubuntu) or `sudo dnf install ./@RPM@` (Fedora). The Linux packages need X11 or XWayland.

**Web.** Nothing to install: Cubeulator runs in a current Chrome, Edge, Firefox or Safari, at [the project's site](https://iainchesworthlabs.github.io/cubeulator/app/). `@WEB@` is the same app as static files to host yourself: unzip it behind any https server (`HOSTING.txt` inside says what the host has to do), or `python3 -m http.server` in the directory to try it on `localhost`. See [Try it in your browser](docs/web.md).

**Why Windows and macOS ask, and how to check a download instead.** The installers are not signed with a Windows (Authenticode) or Apple (Developer ID) certificate, so SmartScreen and Gatekeeper cannot vouch for them and ask before they run; that is expected for this release. What vouches for them instead: `SHA512SUMS` with GPG signatures from the project key, a build-provenance attestation for each package, and SPDX and CycloneDX SBOMs, all of which you can check yourself. [How to check each](docs/SECURITY.md#verifying-build-authenticity).
