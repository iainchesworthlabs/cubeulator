# Installing on an iPhone

Cubeulator is not in the App Store or TestFlight. This page is for the
maintainer's own iPhone: an **ad-hoc** build, an `.ipa` signed for the devices
registered with Apple, made by a GitHub Actions workflow and installed from
Safari. It is a limited build:

- **The camera does not scan yet.** The iPhone has no camera backend
  (`src/app/platform/ios/camera_ios.cpp` reports no camera); the screens run, and
  a cube is entered by hand.
- It runs on **iOS 18 or later** (the build's deployment target, which is what
  Qt 6.12 supports).
- It installs **only on the devices the provisioning profile lists**. Adding one
  means a new profile and a new build ([below](#add-a-device)).

!!! warning "This has not run on a device yet"
    The workflow, the signing and the install were written without a Mac to
    try them on. What a hosted runner has built is in the pull request that
    added them; what only a phone can confirm is whether the app opens.

## Set up once: the Apple developer portal

Everything here is done by the account holder in a browser, at
[developer.apple.com/account](https://developer.apple.com/account) under
*Certificates, Identifiers & Profiles*. Nothing here needs a Mac. The
distribution certificate already exists (the same one the other apps are signed
with); the App ID, the device and the profile are new.

### 1. Make the App ID

1. *Identifiers*, then the blue **+**.
2. Choose **App IDs**, **Continue**, then **App**, **Continue**.
3. *Description*: `Cubeulator`. *Bundle ID*: choose **Explicit** and enter
   exactly

    ```text
    com.iainchesworthlabs.cubeulator
    ```

    (it is `CUBEULATOR_APP_ID` in `cmake/AppIdentity.cmake`, and it cannot change
    once an App ID or a profile names it).
4. Leave every capability unticked; the app needs none. **Continue**, **Register**.

### 2. Register the iPhone

1. Find the phone's **UDID**. Connect it to a computer and trust the computer
   when the phone asks. On Windows, open the *Apple Devices* app (or iTunes),
   select the phone, and click the serial-number line under its name until it
   shows **UDID**, then copy it. (On a Mac: Finder, select the phone, click the
   line under its name.) A phone made since 2018 has a UDID like
   `00008110-001A2B3C4D5E6F78`, with the dash after the eighth character; keep
   it exactly as shown.
2. *Devices*, then **+**. *Platform*: **iOS, iPadOS, tvOS, watchOS**. *Device
   name*: anything you will recognise (`Iain's iPhone`). *Device ID (UDID)*:
   paste it. **Continue**, **Register**.

### 3. Make the Ad Hoc profile

1. *Profiles*, then **+**.
2. Under *Distribution* choose **Ad Hoc**, **Continue**.
3. *App ID*: **Cubeulator** (`com.iainchesworthlabs.cubeulator`). **Continue**.
4. *Certificate*: tick your existing **Apple Distribution** certificate.
   **Continue**.
5. *Devices*: tick the iPhone. **Continue**.
6. *Provisioning Profile Name*: type exactly

    ```text
    Cubeulator Ad Hoc
    ```

    The build checks the profile's name against a repository secret and stops if
    they differ, so this is the one string to get right.
7. **Generate**, then **Download**. You get `Cubeulator_Ad_Hoc.mobileprovision`
   (the file name does not matter; the profile *name* does).

### 4. Hand the profile over

Put the downloaded file in `C:\temp\ios-signing\`. Whoever sets the repository
secrets reads it from there; the secrets are:

| Secret | Holds | Where it comes from |
| --- | --- | --- |
| `IOS_DIST_CERTIFICATE_BASE64` | the Apple Distribution certificate **with its private key**, a `.p12`, as base64 | Keychain Access, *Export*, once, on a Mac that holds the key |
| `IOS_DIST_CERTIFICATE_PASSWORD` | the password that `.p12` was exported with | the same export |
| `IOS_PROVISIONING_PROFILE_BASE64` | the `.mobileprovision` of step 3, as base64 | this step |
| `APPLE_TEAM_ID` | the ten-character team ID | the portal, *Membership details* |
| `APPLE_PROVISIONING_PROFILE_NAME` | `Cubeulator Ad Hoc` | step 3.6 |

They are repository secrets, set from a file or standard input so that no value
is typed into a terminal or printed:

```powershell
# from a PowerShell prompt, in the folder that holds the files
[Convert]::ToBase64String([IO.File]::ReadAllBytes("Cubeulator_Ad_Hoc.mobileprovision")) |
    gh secret set IOS_PROVISIONING_PROFILE_BASE64 --repo iainchesworthlabs/cubeulator
```

The workflow never prints them. Keep the `.p12`, its password and the profile
somewhere safe outside the repository, which is public.

## Build it

The workflow is `.github/workflows/ios-device.yml`. It builds Qt's static iOS kit and
the app on a hosted Mac (an hour or more the first time, while vcpkg builds OpenCV;
about twenty minutes after), signs the app when the five secrets are set, and
uploads an artifact named `cubeulator-ios-adhoc` holding the `.ipa` and a
`manifest.plist`. It runs on every push to a `feature/ios-*` or `bugfix/ios-*`
branch; a dispatch (`gh workflow run ios-device.yml --ref <branch>`) works once the
workflow is on `main`.

## Install from a Windows PC

You do not need a Mac. The phone installs the app from a link in Safari, over the
air; all the PC does is carry two files to an address on the web.

Safari installs an ad-hoc app from a link of the form

```text
itms-services://?action=download-manifest&url=<https URL of manifest.plist>
```

The manifest names the app and where its `.ipa` is. Both must be on an **https**
address the phone can reach without signing in. A GitHub release's files are (the
repository is public; the `.ipa` is useless to anyone else, because it installs
only on the devices in the profile). One release, `ios-test`, is made once and reused
for every test build:

```sh
# Git Bash or PowerShell, in a clone of the repository; <run> is the workflow run's id
gh run download <run> --repo iainchesworthlabs/cubeulator -n cubeulator-ios-adhoc -D ios-dist
gh release create ios-test --repo iainchesworthlabs/cubeulator --prerelease \
    --title "iOS test build" --notes "Ad-hoc build for registered devices. Not for anyone else." \
    ios-dist/*.ipa ios-dist/manifest.plist
# next time:  gh release upload ios-test ios-dist/* --clobber --repo iainchesworthlabs/cubeulator
```

Then on the phone:

1. Put this line in an e-mail to yourself (or a message, or a note) and open it on the
   phone:

    ```text
    itms-services://?action=download-manifest&url=https://github.com/iainchesworthlabs/cubeulator/releases/download/ios-test/manifest.plist
    ```

    Tap the link. The documentation site's `install-ios.html` page makes the same link
    with a button (`?tag=ios-test` names the release), once the site has been
    published from `main`; the e-mail needs nothing published.
2. iOS asks whether to install Cubeulator. Confirm; the icon appears on the Home Screen
   and fills in as it downloads.
3. If iOS says the developer is **untrusted**, go to *Settings, General, VPN & Device
   Management*, choose the developer app and tap **Trust**. A distribution certificate
   normally does not need this.
4. The first time the app opens the camera, iOS asks; allow it.

An update is the same again with a newer build: iOS replaces the app and keeps its data.

Other routes exist, and none is needed: Apple Configurator and Xcode's *Devices*
window install an `.ipa` over a cable but only run on a Mac; Windows tools such as
Sideloadly install by signing the app again with an Apple ID, which makes the ad-hoc
profile pointless.

!!! note "If the link does nothing, or the download fails"
    GitHub sends a release file through a redirect, and iOS is fussier about the
    manifest than a browser is. If the link opens but the install never starts, the
    manifest (not the signing) is the first suspect: say so, and the files can be put
    at a plainer address.

## Add a device

A phone can install only builds signed with a profile that lists it, so a new
device (the iPad, a second phone) needs a **new profile and a new build**:

1. Register its UDID ([step 2](#2-register-the-iphone)).
2. *Profiles*, open `Cubeulator Ad Hoc`, **Edit**, tick the new device, **Save**,
   **Download**.
3. Set `IOS_PROVISIONING_PROFILE_BASE64` again from the new file
   ([step 4](#4-hand-the-profile-over)). The name does not change.
4. Run the workflow and install from the new build.

The app is built for the iPhone only for now; an iPad runs it in a scaled
window. Supporting the iPad properly is a change to the build's device family
(`packaging/ios/ConfigureApp.cmake`) and a look at every screen on it.

## When the signing runs out

- **The profile lasts a year.** After its expiry date an installed copy stops
  opening and no new build can be signed with it. Regenerate it in the portal
  (editing it and saving makes a fresh one), set `IOS_PROVISIONING_PROFILE_BASE64`
  again, build, and reinstall. The workflow checks the profile before it
  builds and says when it has expired.
- **The distribution certificate has its own expiry** (the portal shows it).
  When it is renewed, export the new certificate with its private key as a
  `.p12`, set `IOS_DIST_CERTIFICATE_BASE64` and `IOS_DIST_CERTIFICATE_PASSWORD`,
  and regenerate the profile against it.

## If it does not install

| What happens | Likely cause |
| --- | --- |
| Tapping the link does nothing | Not in Safari (an e-mail app's own viewer, or a link in another browser). Open it in Safari |
| "Unable to install", or the icon is dimmed and stays on *Waiting* | The phone is not in the profile this build was signed with: [add it](#add-a-device) and build again |
| iOS asks to trust the developer | Step 4 of [Install](#install) |
| It opened before and now closes at once | The profile expired ([above](#when-the-signing-runs-out)) |
| The workflow stops before building, naming a secret | That secret is not set, or the profile does not match the others; the message says which |
