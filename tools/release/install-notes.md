## Installing

**Windows.** Run `@WINDOWS@`. The installer is not signed with a certificate Windows trusts yet, so Windows SmartScreen may say "Windows protected your PC": choose *More info*, then *Run anyway*.

**macOS.** Open `@MACOS@` and drag Cubeulator to Applications. The app carries an ad-hoc signature and is not notarised, so the first time, right-click it and choose *Open*, or use *Open Anyway* in System Settings > Privacy & Security.

**Linux.** `sudo apt install ./@DEB@` (Debian, Ubuntu) or `sudo dnf install ./@RPM@` (Fedora). The Linux packages need X11 or XWayland.

**iPhone.** The iPhone app is not a download. It goes to the maintainer's testers through TestFlight, Apple's own beta service, so there is no file to install and no device to register. To try it, ask the maintainer to add your Apple ID to the test group, then install it from the TestFlight app. [The iPhone build](https://iainchesworthlabs.com/cubeulator/guide/iphone/) has the steps.

**Web.** Nothing to install: Cubeulator runs in a current Chrome, Edge, Firefox or Safari, at [the project's site](https://iainchesworthlabs.com/cubeulator/app/). `@WEB@` is the same app as static files to host yourself: unzip it behind any https server (`HOSTING.txt` inside says what the host has to do), or `python3 -m http.server` in the directory to try it on `localhost`. See [In your browser](https://iainchesworthlabs.com/cubeulator/guide/web/).

**Why Windows and macOS ask, and how to check a download instead.** The installers are not signed with a certificate that Windows (Authenticode) or Apple (Developer ID) vouches for, so SmartScreen and Gatekeeper cannot vouch for them and ask before they run; that is expected for this release. What vouches for them instead: `SHA512SUMS` with GPG signatures from the organisation's release key, a build-provenance attestation for each package, and SPDX and CycloneDX SBOMs, all of which you can check yourself. [How to check each](https://iainchesworthlabs.com/cubeulator/guide/verify/).
