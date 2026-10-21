# Installing on an iPhone

Cubeulator is not in the App Store or TestFlight. The iPhone build is an **ad-hoc**
build: it installs only on iPhones whose identifiers have been registered with Apple
by the maintainer, and not on anyone else's. It is a limited build, for trying
Cubeulator on a phone.

!!! warning "Read this first"
    - **Only registered devices.** An iPhone that is not in the build's provisioning
      profile cannot install it, and nothing here can change that. If yours is not
      registered, use the [web version](web.md) or a desktop.
    - **The profile is public.** The release is public, and the build carries a profile
      that lists the registered devices' identifiers (UDIDs). Register a device only if
      you are content for its UDID to be published. Apple allows 100 of each type a year.
    - **iOS 18 or later.**
    - **Unproven on a phone.** The build is checked on a hosted Mac, not on a phone: the
      install, the launch and the camera are what the first person to try it finds out
      ([what to check](#first-run-what-to-check-on-the-phone)).

## Install

1. On the iPhone, open the **install page** in **Safari** (not Chrome, and not a link
   inside an app: the `itms-services` link only works from Safari):

    [iainchesworthlabs.github.io/cubeulator/install-ios.html](install-ios.html)

    With no tag it offers the newest release that has an iPhone build, a pre-release
    included. A release's notes link to it with that release's tag
    (`install-ios.html?tag=v0.2.0-beta.1`).
2. Tap **Install on iPhone** and confirm iOS's question. The icon appears on the Home
   Screen and fills in as it downloads.
3. If iOS says the developer is **untrusted**, go to *Settings, General, VPN & Device
   Management*, choose the developer app and tap **Trust**. A distribution certificate
   normally does not need this.
4. Open Cubeulator. The first time you scan, iOS asks whether it may use the camera;
   allow it. If you said no, *Settings, Cubeulator, Camera* turns it back on.

An update is the same again with a newer release: iOS replaces the app and keeps its data.

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
| The page shows no button, or says no release has an iPhone build | No published release carries one yet, or GitHub refused the page's lookup; open the page with `?tag=<tag>` |
| Tapping the button does nothing | Not in Safari. Open the page in Safari |
| "Unable to install", or the icon is dimmed and stays on *Waiting* | The phone is not registered in the profile this release was signed with |
| iOS asks to trust the developer | Step 3 above |
| It opened before and now closes at once | The signing profile expired (it lasts a year); a newer release fixes it |
| The link opens but the install never starts | The manifest could not be reached the way iOS needs it: say so, with the iOS version |

How the build is made and signed, and how a device is registered, is
[iPhone build: signing and releasing](ios-signing.md).
