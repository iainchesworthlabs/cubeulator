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

You do not need a Mac. The phone installs the app from a page in Safari, over the air;
all the PC does is get the build onto a web address.

Safari installs an ad-hoc app from a link of the form

```text
itms-services://?action=download-manifest&url=<https URL of manifest.plist>
```

The manifest names the app and where its `.ipa` is, and both must be on a plain **https**
address (no redirect, no sign-in) that the phone can reach. A release's files are served
through a redirect, so the build is served by the project's own GitHub Pages site
instead, from a folder that nothing links to.

!!! warning "Who can download it"
    The site and the repository are public. The folder has a random name 32 or more
    characters long, is linked from nowhere, is left out of the site map and asks search
    engines to leave it alone, but **anyone who has its address can download the file**,
    and the `.ipa` holds the provisioning profile (the team ID and the phone's UDID). It
    installs only on the registered phone, and the app is open source anyway; even so,
    do not pass the address on, and take the files down after the install
    ([below](#take-it-down)).

### What happens

1. The workflow signs the build and uploads the artifact `cubeulator-ios-adhoc`: the
   `.ipa`, a `manifest.plist` that names the folder, and an `index.html` with an
   **Install** button that makes the link.
2. Two repository variables name what the site carries: `IOS_TEST_PATH` (the random
   folder) and `IOS_TEST_RUN_ID` (the run whose artifact to publish). While both are
   set, **every** publish of the site, the release's own included, carries the folder,
   because a Pages deployment replaces the whole site and would otherwise wipe it.
3. `.github/workflows/ios-test-site.yml` publishes the site: the documentation and the
   web app **as they are on `main`** (so the released site is not replaced by unreleased
   pages) plus the folder. It runs when `.github/ios-test-site.trigger` is changed on a
   `feature/ios-*` branch. Nothing is committed to the repository.

### The one setting to change

GitHub Pages deploys only from the branches the `github-pages` environment allows, and
that is `main` alone. To publish before the release, add the work branches once:
*Settings*, *Environments*, *github-pages*, *Deployment branches and tags*, **Add
deployment branch or tag rule**, ref type *Branch*, name pattern `feature/ios-*`. Only
the repository owner can change this. Remove the rule again when the iPhone work is
merged to `main`; the workflow then runs from there.

### On the phone

Open this address in **Safari** on the phone (send it to yourself in a message or an
e-mail; the maintainer is told the real one, which is not written here):

```text
https://iainchesworthlabs.github.io/cubeulator/<the random folder>/
```

1. Tap **Install on iPhone** and confirm iOS's question. The icon appears on the Home
   Screen and fills in as it downloads.
2. If iOS says the developer is **untrusted**, go to *Settings, General, VPN & Device
   Management*, choose the developer app and tap **Trust**. A distribution certificate
   normally does not need this.
3. The first time the app opens the camera, iOS asks; allow it.

An update is the same again after a new build and a new publish: iOS replaces the app and
keeps its data.

### Take it down

When the install is done:

```sh
gh variable delete IOS_TEST_RUN_ID --repo iainchesworthlabs/cubeulator
```

and publish the site again (change `.github/ios-test-site.trigger` on a `feature/ios-*`
branch, or let the next release's publish do it): the folder is gone. Even if nobody does,
the artifact expires **14 days** after the build, and a publish after that date leaves the
folder out with a warning. Set a reminder for the day after the install. The profile
itself lasts until 2027-07-16 ([below](#when-the-signing-runs-out)).

Other routes exist and none is needed: Apple Configurator and Xcode's *Devices* window
install an `.ipa` over a cable but only run on a Mac, and Windows tools such as Sideloadly
install by signing the app again with an Apple ID, which makes the ad-hoc profile pointless.

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
| Tapping the button does nothing | Not in Safari (an e-mail app's own viewer, or another browser). Open the address in Safari |
| "Unable to install", or the icon is dimmed and stays on *Waiting* | The phone is not in the profile this build was signed with: [add it](#add-a-device) and build again |
| iOS asks to trust the developer | Step 4 of [Install](#install) |
| It opened before and now closes at once | The profile expired ([above](#when-the-signing-runs-out)) |
| The workflow stops before building, naming a secret | That secret is not set, or the profile does not match the others; the message says which |
