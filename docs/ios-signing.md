# iPhone build: signing and releasing

How the iPhone build is made, signed and sent to TestFlight, and the Apple steps behind
it. The page for the person installing it is [Installing on an iPhone](ios-install.md).

The iPhone build is an **App Store** build, sent to **TestFlight** for internal testers.
It is made by `.github/workflows/ios-device.yml`, which the Release workflow calls
(`release.yml`), with the shared `import-ios-signing`, `export-ios-ipa` and
`upload-testflight` actions of `icl-common-infra` (`docs/ios-signing.md` there is the
general description). The IPA is not a release file: an App Store IPA installs on no
device, and a workflow artifact of a public repository can be downloaded by any
signed-in GitHub account, so nothing keeps it.

Internal testing needs no Apple review. External testing and a store listing are not
planned: they wait on the licence position in [Licensing](licensing.md).

!!! warning "What is verified and what is not"
    Verified on a hosted Mac (CI): the app configures, builds, links with Qt's static
    iOS kit, is signed with the Apple Distribution certificate and the App Store profile,
    the checks in `packaging/ios/verify_ipa.sh` pass on the exported IPA (bundle
    identifier, version, build number, export compliance, camera text, icon, arm64,
    nothing linked outside the system, a valid signature, an App Store profile for this
    App ID), and Apple validates the build.

    **Not verified until someone installs it from TestFlight:** that the app opens and
    everything it does on the phone, the camera included.

## What the build is

- iOS 18 or later (the deployment target; Qt 6.12's floor), iPhone only for now.
- A static Qt: the program is one executable with the QML modules and Qt's plugins
  linked in (`packaging/ios/ConfigureApp.cmake` names the plugins).
- `CFBundleShortVersionString` is the plain `X.Y.Z` (iOS accepts no label),
  `CFBundleVersion` is the number of the Release workflow run. Apple refuses a build
  whose number is not higher than every earlier one for the same version, and the
  run number only grows, so a rebuilt release uploads. Only the Release workflow
  uploads, so there is one counter.
- `ITSAppUsesNonExemptEncryption` is `false` in the property list, so TestFlight does
  not ask the export-compliance question for every build.

## What is set, and where

| Name | Kind | Holds |
| --- | --- | --- |
| `SIGN_IOS_UPLOAD` | repository variable | `true` signs the build and sends it to Apple; unset or `false` builds unsigned and proves the build and nothing else |
| `IOS_DIST_CERT_P12_BASE64`, `IOS_DIST_CERT_PASSWORD` | organisation secrets | the Apple Distribution certificate with its private key, a `.p12`, and its password |
| `APPSTORE_API_KEY_ID`, `APPSTORE_API_ISSUER_ID`, `APPSTORE_API_PRIVATE_KEY` | organisation secrets | the App Store Connect API key (role App Manager) |
| `IOS_PROVISIONING_PROFILE_BASE64` | repository secret | the App Store provisioning profile of this App ID, as base64 |

The organisation secrets are limited to the repositories that build for iOS; this
repository is on the list. The team ID and the profile's name are read out of the
profile, so they are not secrets.

## Set up once: Apple

Done by the account holder, in a browser.

1. *Identifiers* (developer.apple.com, Certificates, Identifiers & Profiles): an
   explicit App ID, `com.iainchesworthlabs.cubeulator` (`CUBEULATOR_APP_ID` in
   `cmake/AppIdentity.cmake`). No capabilities.
2. *Profiles*, then **+**, then *Distribution*, **App Store Connect**: for that App ID and
   the Apple Distribution certificate. It lists no devices. Download the
   `.mobileprovision`.
3. App Store Connect: *Apps*, **New App**, iOS, the bundle identifier above, a name that is
   unique across the App Store, and any SKU. In the app's *TestFlight* tab make an
   *Internal Testing* group and add the testers.
4. Set the profile as a repository secret, from the file and not by typing it:

    ```sh
    base64 -w0 < Cubeulator_App_Store.mobileprovision | gh secret set IOS_PROVISIONING_PROFILE_BASE64 -R iainchesworthlabs/cubeulator
    ```

5. Set the repository variable `SIGN_IOS_UPLOAD` to `true`.

The certificate and the API key are the organisation's and are shared with the other
apps; how they are made is in `icl-common-infra`.

## Building it

The Release workflow calls `ios-device.yml` with the version. A real release (a tag, or a
dispatch with `dry_run` unticked) uploads the build to TestFlight; a dry run has Apple
validate it and makes no build for anyone. With `SIGN_IOS_UPLOAD` unset the app is built
unsigned and nothing is sent. The first step with `SIGN_IOS_UPLOAD` set imports the
certificate and the profile and names any secret that is missing, before the Qt download.

The workflow also runs by itself on a push to a `feature/ios-*` or `bugfix/ios-*` branch
(the way the build is worked on before the workflow can be dispatched by hand). Such a
run signs and has Apple validate the build; it never uploads.

The certificate is imported into a keychain made for the job and removed at the end,
whatever happened.

## When the signing runs out

- **The profile lasts a year.** After its expiry date no new build can be signed with it.
  Edit it in the portal and save (that makes a fresh one), download it, and set
  `IOS_PROVISIONING_PROFILE_BASE64` again. The import step checks the profile before the
  build and says when it has expired or does not fit the bundle identifier.
- **The distribution certificate has its own expiry** (the portal shows it). It is the
  organisation's: renewing it is done once for every app, and the profile is regenerated
  against the new certificate.
- **A TestFlight build lasts 90 days.** Testers install a newer release.

## If the build fails

| What happens | Likely cause |
| --- | --- |
| The first step names a secret | That secret is not set, or does not reach this repository |
| The first step says the profile does not fit | Wrong App ID, expired, or it lists devices (it is an ad-hoc profile) |
| Apple validation fails | The message is Apple's: the icon, the property list, the signature or a privacy declaration |
| Apple refuses the upload because of the build number | A build with a higher number for this version already exists; run the release again |
