# First run on an iPhone: what to check

The steps for a tester to install the app, and the table of what goes wrong when it does not install, are in the user guide:
[The iPhone build](guide/iphone.md). This page is the list of things to check on the first install of a new build, because
nothing about the camera had been seen on a real phone when it was written.

## What to check

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


How the build is made, signed and uploaded is
[iPhone build: signing and releasing](ios-signing.md).
