# iPhone build: signing and releasing

How the iPhone build is made, signed and shipped, and the Apple developer portal steps
behind it. The page for the person installing it is [Installing on an iPhone](ios-install.md).

Cubeulator is not in the App Store or TestFlight. The iPhone package is an **ad-hoc**
build: an `.ipa` signed for the devices registered with Apple, installed from Safari
over the air. It is built by `.github/workflows/ios-device.yml`, which the Release
workflow calls (`release.yml`), so that the package has the release's version,
checksums, signature, SBOM and attestation like the others.

!!! warning "What is verified and what is not"
    Verified on a hosted Mac (CI): the app configures, builds, links with Qt's static
    iOS kit, is signed with the Apple Distribution certificate and the ad-hoc profile,
    and the checks in `packaging/ios/verify_ipa.sh` pass on the signed `.ipa` (bundle
    identifier, version, camera text, icon, arm64, nothing linked outside the system, a
    valid signature, an ad-hoc profile for this App ID).

    **Not verified until someone taps it on a phone:** that the manifest is accepted when
    it is reached through GitHub's release-asset redirect, that the app installs, that it
    opens, and everything the app does on the phone, the camera included.

## What the build is

- iOS 18 or later (the deployment target; Qt 6.12's floor), iPhone only for now.
- A static Qt: the program is one executable with the QML modules and Qt's plugins
  linked in (`packaging/ios/ConfigureApp.cmake` names the plugins).
- `CFBundleShortVersionString` is the plain `X.Y.Z` (iOS accepts no label; the label
  stays in the file name), `CFBundleVersion` is the workflow run's number.
- The files of a release: `Cubeulator-<version>-ios.ipa` and
  `Cubeulator-<version>-ios-manifest.plist`, the over-the-air manifest, which names the
  `.ipa` as the release's own asset.

## How the install link works

Safari installs an ad-hoc app from a link of the form

```text
itms-services://?action=download-manifest&url=<https URL of the manifest>
```

GitHub's release-notes renderer removes links with a scheme it does not know, so the
notes link to the documentation site's [install page](install-ios.html), which makes the
link in the browser from the release's tag (`install-ios.html?tag=v0.2.0-beta.1`), or
from the newest release when it is given none.

The manifest and the `.ipa` are the release's own assets, reached through GitHub's
redirect to its download host. **Whether iOS accepts the manifest through that redirect
has not been tried on a phone.** If it does not, the fix is to serve the manifest from the
documentation site (`https://iainchesworthlabs.github.io/cubeulator/...`), which has no
redirect, and the install page is the one place that changes.

!!! warning "The package is public, and so is the phone's identity"
    The release is public, and an ad-hoc `.ipa` carries its provisioning profile, which
    lists the **UDID of every registered device** and the team ID. Anyone can read them.
    Register only devices whose identifiers you are content to publish. Apple allows 100
    devices of each type per membership year, and removing one frees its place only at the
    year's renewal. The package installs only on the registered devices, and the app is
    open source, so the file itself gives nothing away.

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


## Building it

The Release workflow calls `ios-device.yml` with the version, the tag and `signed: true`;
a missing secret then fails the release before the long Qt download. The workflow also
runs by itself on a push to a `feature/ios-*` or `bugfix/ios-*` branch (the way the build
is worked on before the workflow is on `main`, from where it can be dispatched by hand):
with the five secrets it signs, with no profile it builds unsigned and proves nothing
else. The signed result is the artifact `package-ios`. The certificate is imported into a
keychain made for the job and removed at the end, whatever happened.

## Add a device

A phone can install only builds signed with a profile that lists it, so a new
device (the iPad, a second phone) needs a **new profile and a new build**:

1. Register its UDID ([step 2](#2-register-the-iphone)).
2. *Profiles*, open `Cubeulator Ad Hoc`, **Edit**, tick the new device, **Save**,
   **Download**.
3. Set `IOS_PROVISIONING_PROFILE_BASE64` again from the new file
   ([step 4](#4-hand-the-profile-over)). The name does not change.
4. Build again (a release, or a push to a `feature/ios-*` branch) and install from the new build.

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
| "Unable to install", or the icon is dimmed and stays on *Waiting* | The phone is not in the profile this build was signed with: [add it](#add-a-device) and build again |
| iOS asks to trust the developer | See [Installing on an iPhone](ios-install.md) |
| It opened before and now closes at once | The profile expired ([above](#when-the-signing-runs-out)) |
| The workflow stops before building, naming a secret | That secret is not set, or the profile does not match the others; the message says which |
