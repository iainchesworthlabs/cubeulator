# The iPhone build

Cubeulator is not in the App Store. The iPhone app goes to **TestFlight**, Apple's own beta service, for testers the author
has invited. There is no file to download and no device to register: you install the TestFlight app, and the build appears
in it.

!!! warning "Read this first"
    - **Invited testers only.** Internal testing is for people the author has added to the App Store Connect team, up to
      100. There is no public test link and no external testing. If you are not on the list, use
      [the browser version](web.md), which works on a phone, or a desktop.
    - **iOS 18 or later.**
    - **Still new on the phone.** The app opens and goes on to the scan once the camera is allowed. The scan itself, the
      camera's colours and the 3D cube's other looks have had little use on a phone so far, so please tell us what you
      see on your first scans.

## Install

1. Ask the author to add your Apple ID to the TestFlight test group. An invitation arrives by email.
2. On the iPhone, install **TestFlight** from the App Store and sign in with that Apple ID.
3. Open the invitation, or open TestFlight, and tap **Install** next to Cubeulator.
4. Open Cubeulator. The first time you scan, iOS asks whether it may use the camera; allow it. If you said no,
   *Settings*, *Cubeulator*, *Camera* turns it back on.

If you said no, Cubeulator says *Camera access is off* and offers to open the phone's
settings, where *Camera* can be turned back on. It cannot scan without the camera.

A new release appears in TestFlight as soon as Apple has processed it, usually within half an hour. TestFlight offers the
update, and iOS replaces the app and keeps its data. A TestFlight build expires after 90 days, so install a newer one
before then.

## If it does not install

| What happens | Likely cause |
| --- | --- |
| No invitation arrives | Your Apple ID is not in the test group yet; ask the author, and check the spam folder |
| TestFlight does not list Cubeulator | The invitation was not accepted, or the build is still processing |
| "This beta is no longer accepting testers", or the build is greyed out | The build is more than 90 days old; install a newer one |
| It will not open on your phone | iOS is older than 18 |

## Using it

The screens are the same as on a computer, in the narrow layout. See [Scanning a cube](scan.md) and
[Following the solution](solve.md). Cubeulator asks the camera to hold its exposure and white balance during a scan, and says
on the screen if it could not.

## Next

[Scan a cube](scan.md).
