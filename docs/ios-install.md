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

The workflow is `.github/workflows/ios-device.yml`:

```sh
gh workflow run ios-device.yml --ref develop
```

It builds Qt's static iOS kit and the app on a hosted Mac (an hour or more, the
first time, while vcpkg builds OpenCV), and uploads an artifact with the `.ipa`
and a `manifest.plist` for the over-the-air install.

## Install

Safari installs an ad-hoc app from a link of the form

```text
itms-services://?action=download-manifest&url=<https URL of manifest.plist>
```

The manifest names the app and where its `.ipa` is. Both must be on an
**https** address the phone can reach without signing in: a GitHub release's
files are (the repository is public). So:

1. Upload the `.ipa` and the `manifest.plist` of the workflow's artifact to a
   GitHub release (a pre-release is enough).
2. On the phone, open the link in **Safari** (not in Chrome, and not inside an
   app's own browser): send it to yourself by e-mail or a message and tap it
   there, or open the page that makes it.
3. iOS asks whether to install Cubeulator. Confirm; the icon appears on the Home
   Screen and fills in as it downloads.
4. If iOS says the developer is **untrusted**, go to *Settings, General, VPN &
   Device Management*, choose the developer app and tap **Trust**. A
   distribution certificate normally does not need this.

An update is the same again with a newer build: iOS replaces the app and keeps
its data.

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
| Tapping the link does nothing | Not in Safari. Open it in Safari |
| "Unable to install", or the icon is dimmed and stays on *Waiting* | The phone is not in the profile this build was signed with: [add it](#add-a-device) and build again |
| iOS asks to trust the developer | Step 4 of [Install](#install) |
| It opened before and now closes at once | The profile expired ([above](#when-the-signing-runs-out)) |
| The workflow stops before building, naming a secret | That secret is not set, or the profile does not match the others; the message says which |
