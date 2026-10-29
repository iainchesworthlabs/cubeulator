# Installing on an iPhone

Cubeulator is not in the App Store. The iPhone build goes to **TestFlight**, Apple's
own beta service, for the maintainer's internal testers. There is no file to download
and no device to register: you install the TestFlight app, and the build appears in it.

!!! warning "Read this first"
    - **Internal testers only.** Internal testing is for people the maintainer has
      added to the App Store Connect team, up to 100. There is no public test link and
      no external testing. If you are not on the list, use the [web version](web.md) or a
      desktop.
    - **iOS 18 or later.**
    - **Unproven on a phone.** The build is checked on a hosted Mac, and Apple checks it
      again when it is uploaded. The install, the launch and the camera are what the first
      person to try it finds out ([what to check](#first-run-what-to-check-on-the-phone)).

## Install

1. Ask the maintainer to add your Apple ID to the TestFlight test group (an invitation
   arrives by email).
2. On the iPhone, install **TestFlight** from the App Store and sign in with that Apple
   ID.
3. Open the invitation, or open TestFlight, and tap **Install** next to Cubeulator.
4. Open Cubeulator. The first time you scan, iOS asks whether it may use the camera;
   allow it. If you said no, *Settings, Cubeulator, Camera* turns it back on.

A new release appears in TestFlight as soon as Apple has processed it, usually within
half an hour. TestFlight offers the update, and iOS replaces the app and keeps its data.
A TestFlight build expires after 90 days; install a newer one before then.

## First run: what to check on the phone

Nothing about the camera has been seen on a real phone, so the first install is a test.
Go through these and say which fail (a photo of the screen helps):

1. **It opens.** The launch screen shows the paper colour (light or dark, as the phone
   is set), then the app. It does not close by itself.
2. **The camera question.** On the first scan the system asks to use the camera, and its
   text is in the phone's language (English, Deutsch, Francais, Espanol, Japanese,
   Chinese, Hebrew, Arabic or Yiddish; any other language gets English). Allow it.
3. **The back camera first.** The preview shows what is in front of the phone, not the
   person holding it, and is **not mirrored**: writing held up to the lens reads the
   right way round.
4. **Switching.** The scan screen has a button that changes camera. The front camera
   shows a mirrored preview, as a selfie does; the back camera goes back to unmirrored.
5. **Upright.** With the phone held upright, the picture is upright (not sideways or
   upside down). Turn the phone to landscape: the picture stays level with the room.
6. **A scan.** Hold a cube in good light and scan it. The colours it reads match the
   cube, and the scan finishes as it does on the desktop. Anything that drifts, such
   as a face that is read twice differently, is worth a note: the app asks the camera
   to hold its exposure and white balance, and says on the screen if it could not.
7. **After a refusal.** Turn the camera off for Cubeulator in the phone's *Settings*,
   open the app and try to scan: it says the camera is off and offers to open the
   settings, and the button opens them.
8. **Away and back.** Press the side button during a scan, or switch to another app, and
   come back: the preview resumes.

## If it does not install

| What happens | Likely cause |
| --- | --- |
| No invitation arrives | Your Apple ID is not in the test group yet; ask the maintainer, and check the spam folder |
| TestFlight does not list Cubeulator | The invitation was not accepted, or the build is still processing |
| "This beta is no longer accepting testers" or the build is greyed out | The build is more than 90 days old; install a newer one |
| It will not open on your phone | iOS is older than 18 |

How the build is made, signed and uploaded is
[iPhone build: signing and releasing](ios-signing.md).
