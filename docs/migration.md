# Aligning cubeulator with the ICL standard

This is the plan for bringing cubeulator in line with the C++23 project standard (`docs/standard.md` in `iainchesworthlabs/icl-common-infra`, version 0.13). cubeulator is an existing project and one of the repositories the standard was drawn from, so the work is an alignment of a working project. It is kept up to date until the last item is done, then trimmed to a short record.

> [!NOTE]
> **Status at 0.2.0-beta.4, checked 2026-11-22: in progress.** The repository side is done: the branch model and its rulesets, vcpkg from the labs' fork, the toolchain pins, the written conventions, hardening, fuzzing, the `CI Status` gate, the licence wording, the SPDX headers and the move of the iPhone app to TestFlight. What is left is mostly the release side (the workflow, the runbook, `main`, the signing records), the coverage ratchet and a few records in other documents.
>
> **Done, with the evidence**
>
> - **Steps 1 to 7, 9 and 10.** The plan and its decisions (#250, #251, #262); the back-merge #246 (`main` is an ancestor of `develop`, by `git merge-base --is-ancestor`); the Dependabot updates #247 to #249; the toolchain pins (#252: `.github/toolchain-versions.json`, `cmake/icl/` with its `SOURCE`, `tools/ci/sync_cmake.py`, `repin.py`); vcpkg from the fork (#254: no `deps/` and no `.gitmodules`, `cmake/icl/IclVcpkgRoot.cmake`, the `setup-vcpkg` action, no `gitsubmodule` entry in `dependabot.yml`, and a baseline that is a commit of the fork, moved by #267); the written conventions (#253: `.clang-tidy`, `.editorconfig`, `.git-blame-ignore-revs`); hardening, the preprocessor check and the fuzz harnesses (#259, #258, #275: `src/lib/fuzz/`, `fuzz.yml`); the repository settings of 2026-10-29 ([Branch protection](branch-protection.md)); the licence wording (#277) and the SPDX headers with `REUSE.toml` (#280).
> - **Steps 8, 11, 12 and 13, in part.** Each is marked below with what landed: `CI Status` (#257), `Policy` (#263) and the draft-pull-request rule (#274); the glibc floor (#260) and the organisation's public key in `docs/signing-key.asc` (#266); `AGENTS.md`, `CONTRIBUTING.md` and the root `SECURITY.md` (#273); the iPhone app to TestFlight (#270, [0.2.0-beta.2](../CHANGELOG.md#020-beta2---2026-10-31)).
> - **Also done:** the Android application id (#255), the `zh_CN` rename (#256), and the tags `v0.2.0-beta.2`, `.3` and `.4`, which sit on `develop` commits.
>
> **Left**
>
> - **The shared workflows.** Only `policy.yml` is called from `icl-common-infra`; `zizmor`, `docs`, `docs-deploy`, `dependabot-auto-merge`, `dependency-review` and the nightly report are still this project's own files, beside the shared `setup-*`, `ci-status` and `report-nightly-failure` actions.
> - **The release workflow.** Done on 2026-11-23: `release.yml` builds the packages and calls the shared `release-publish.yml`, and `tag-release.yml` makes the tag. Two differences from the standard stay, on purpose: the files are not named `-unsigned` or `-testsigned` (the documents, the checks and the install notes name them without), and the preparation branch is `chore/release-<version>` (dashes for the dots), as the standard has it: the shared branch-name check allows `release/x.y` only for the fix of an old release.
> - **`main`.** It still sat at `v0.2.0-beta.1` (`0a0c755`, from `git ls-remote`) on 2026-11-22. The shared publish workflow fast-forwards it after a final release, so the decision "the release bot fast-forwards `main`" is in force from the first final release; a pre-release leaves it where it is. The releases `v0.2.0-beta.3` and `v0.2.0-beta.4` are drafts on GitHub, and no release is listed for `v0.2.0-beta.2`, so beta.1 is the last published desktop release.
> - **The signing key.** `docs/signing-key.asc` holds the organisation key (`FD13 CB8C ... 4DBE`), and the `cubeulator-signing-key.asc` that the draft releases of 0.2.0-beta.3 and 0.2.0-beta.4 carry is that key, so it is the key that signs now (checked on 2026-11-22). The two published releases before them, 0.1.0-beta.1 and 0.2.0-beta.1, carry the earlier project key (`0CF2 A242 ... EB46`). `SECURITY.md` and the verify page of the user guide name both. `release.yml` passes the secret names `REPO_GPG_PRIVATE_KEY` and `REPO_GPG_PASSPHRASE`, which the organisation's secrets of those names provide, to the shared workflow, which refuses a release whose signatures do not verify against `docs/signing-key.asc`; the release no longer carries a key file. [Cutting a release](releasing.md#the-signing-key) says so (2026-11-23).
> - **Documents.** `docs/releasing.md` was rewritten on 2026-11-23 for the flow above (tag on `develop`, approve the `release` environment, `main` fast-forwarded).
> - **The coverage ratchet** (floor file, `quality-history` branch, badges): not started. The gate is still `--fail-under-line 80 --fail-under-branch 80` in `ci.yml`.
> - **A TSan preset.** The fuzz preset (`linux-clang-fuzz`) exists, and the debug presets carry ASan and UBSan (run at `-O1` by the `sanitizers` job, `docs/ci.md`); TSan has no preset, only the flags the nightly job passes.
> - **Layout.** The GPU files `cube_lib` compiles from `src/app/platform/` have not moved into the library (`src/lib/CMakeLists.txt`), and `docs/architecture.md` does not yet record the layout or the portable-library candidates.
> - **Fonts for Hebrew, Yiddish and Arabic.** Not bundled on `develop`: the manifest entries for Noto Sans Hebrew and Arabic are dormant because the font files are not in `src/app/qt/fonts/`. They exist only on `origin/feature/ax-kit`, which is 977 commits behind `develop`.
> - **Owner actions** and the **requests to the shared repositories** below are not checkable from this repository; they stand as the maintainer recorded them.
>
> **Dropped:** nothing has been dropped.

The code conventions of this repository stay: Google-based layout with attached braces at 100 columns (`.clang-format`), the naming in `.clang-tidy`, `Result<T>` over `std::expected` (`docs/architecture.md`), and the `src/lib` and `src/app` layout. The standard's code style applies to new projects only (decision D9).

## Decisions made

All decided on 2026-10-27.

| Topic | Decision | Consequence |
|---|---|---|
| Branch model | Adopt the standard's D1. `develop` is the default branch and the trunk; pull requests squash through the merge queue; `main` holds published releases and is fast-forwarded by the release bot after a successful release. No pull requests into `main`, no back-merge. | Rulesets, branch names, `release.yml`, `release-check.yml`, `docs.yml` and the contributor documents change. `main` became an ancestor of `develop` with the back-merge #246. `spike/*` stays outside the ruleset: ask for a project list of extra prefixes in the ruleset and `check_branch_name.py`; the weekly sweep stays. The `release/<version>` preparation branch becomes `chore/release-<version>` |
| vcpkg | Use the labs' fork `iainchesworthlabs/vcpkg` through `cmake/icl/IclVcpkgRoot.cmake` and the `setup-vcpkg` action, and remove the `deps/vcpkg` submodule. The baseline in `vcpkg.json` is the only pin. | One baseline pin; the `vcpkg-baseline-in-fork` check; the `gitsubmodule` Dependabot entry goes. The Tracy overlay port stays and its two features go upstream |
| CI | Hybrid. Adopt the shared `policy`, `zizmor`, `docs`, `docs-deploy`, `dependabot-auto-merge`, `dependency-review`, `ci-status` and `report-nightly-failure` workflows and the `setup-*` actions. Keep the project's own jobs where the shared workflows do not cover them: clang-format, platform isolation, clang-tidy, the three desktop legs with packaging, Android, web and web tests, iOS, system tests, coverage and performance. | `CI Status` becomes the required check on `develop` and `merge_group` stays on `ci.yml`. Tiers: static checks, clang-tidy, Linux GCC build and tests and coverage on the pull request; Windows and macOS in the queue; Android, iOS, web, web tests and system tests after the merge. CodeQL stays on every pull request |
| Licence | The source stays `GPL-3.0-or-later`. A binary that links Qt Quick 3D is GPL-3.0-only, and the project says so (About text, package metadata, `docs/licensing.md`, `LICENSE-NOTE.md`). Qt Quick 3D stays. | Wording changes in their own pull request. The copyright line stays `Copyright (C) 2026 Iain Chesworth` |
| iOS and Android | iOS goes to TestFlight (internal testers) through the shared `import-ios-signing`, `export-ios-ipa` and `upload-testflight` actions, with validation only on branch runs. The ad-hoc IPA leaves the public release; the IPA and install manifest were removed from v0.2.0-beta.1 on 2026-10-27. Android gets its application id (`com.iainchesworthlabs.cubeulator`) and a locked orientation (superseded by the Orientation row: the app keeps both orientations, and the manifest says `screenOrientation="unspecified"`). No store submission. | The ad-hoc steps, `docs/ios-install.md` and `docs/install-ios.html` are replaced by the TestFlight route. The Linux package is checked against the glibc 2.39 floor and built on a 24.04 image |
| Signing key | Releases are signed with the organisation key (`FD13CB8C…4DBE`). The maintainer deletes the repository secrets. | `docs/signing-key.asc` and `SECURITY.md` carry the organisation key; `SECURITY.md` records that releases up to v0.2.0-beta.1 used the project key `0CF2A242…EB46` |
| Release tooling | `release.yml` is rewritten on the shared `release-publish.yml`. The shared workflow gains a notes input, an `sbom-files` input with attestation read-back, and a `pre-release-script`; `tools/release/*` and `changelog.d/` stay. The draft step stays for one release and is then dropped | Pull requests to `icl-common-infra` first, each with a tag the maintainer creates |
| Next release | A corrected pre-release `v0.2.0-beta.2` follows the glibc fix and the iOS asset change | Tag commands from the maintainer |
| Layout | `src/lib` and `src/app` stay | To be recorded in `docs/architecture.md` (it is not there yet); the two GPU files that `cube_lib` compiles from `src/app/platform/` move into the library (not yet moved) |
| Coverage | The baseline (89.8% lines, 85.8% branches on the library layer) is recorded as the ratchet floor and raised each release towards 90/80. The app is measured separately | A floor file and a `quality-history` branch with badges |
| Sanitisers and presets | Linux ASan/UBSan (`-O1`, Tracy off), TSan and fuzz presets are added. Windows ARM64, Linux ARM64, macOS x64 and the iOS simulator wait for a stated need | Tracy stays dormant-on in desktop release builds (extension request) |
| Languages | Nine are kept; Arabic is added to the standard's list. The stored code of Simplified Chinese is renamed from `zh-CN` to `zh_CN` (the catalogue file already was), and a setting saved the old way is rewritten at the next start (done, #256) | Fonts for Hebrew, Yiddish and Arabic are bundled as subsets |
| Orientation | The app keeps its design: both orientations, following the device (`docs/design/platforms.md`), on Android and on iOS. The standard asks for a locked orientation (7.7); an exception is requested | Android gets its application id only (done, #255) |
| Naming | Both enumerator styles and the `kPascal` constants are written into `.clang-tidy`; nothing is renamed | The nested `src/app/.clang-tidy` covers the Qt-facing code |
| Portable libraries | The solver and N x N model are to be recorded as candidates in `docs/architecture.md` (not there yet); no split | |

## To confirm later

- Signed Android bundle and store build: when Android has a camera backend.
- `SIGN_WINDOWS=test` (the organisation test-signing secrets already reach this repository) and `SIGN_MACOS`: before the first release on the new workflow.
- The `SHA512SUMS` and `SHA512SUMS.asc` of v0.2.0-beta.1 still list the removed IPA files; the README, `docs/ios-install.md` and `docs/install-ios.html` still link to it until the iOS pull request. The release notes of v0.2.0-beta.1 no longer name it. Since then: the release's asset list (`gh release view`) carries no IPA or install manifest, `docs/install-ios.html` is gone, and neither the README nor `docs/ios-install.md` mentions the IPA. Whether `SHA512SUMS` still lists the removed files was not checked, because that means downloading it.

## Order of pull requests

Each pull request is one step and its checks are green before the next starts. Anything that touches many files is scheduled with the maintainer, so it does not collide with feature work, and lands alone.

1. This plan.
2. Back-merge #246, so that `main` is an ancestor of `develop` (done).
3. Pending Dependabot updates (#247 to #249) (done).
4. (Done, #252.) Toolchain pins: `.github/toolchain-versions.json`, the vendored `cmake/icl` with `cmake/icl/SOURCE`, `tools/ci/sync_cmake.py` and `repin.py`, and `FindQt6` reading `ICL_QT_ROOT`.
5. (Done, #254.) vcpkg: switch to `IclVcpkgRoot.cmake` and `setup-vcpkg`, remove the submodule, drop the `gitsubmodule` Dependabot entry, add the `vcpkg-baseline-in-fork` check.
6. (Done, #253.) Conventions written down: the `readability-identifier-naming` section of `.clang-tidy`, `.editorconfig`, `.git-blame-ignore-revs`, the Objective-C++ document in `.clang-format`.
7. Hardening flags (done, #259); the shared preprocessor check with its allowlist (done, #258). The fuzz harnesses and the `linux-clang-fuzz` preset are in `src/lib/fuzz/` (the parsers are listed in [Fuzz tests](testing.md#fuzz-tests); `fuzz.yml` runs them, and ctest replays their seeds on every pull request). Debug presets already carry ASan and UBSan, and TSan runs nightly.
8. CI (in progress; the `CI Status` aggregator is #257, the `Policy` job #263, and the three old policy workflows are removed, #265; the draft-pull-request rule is #274 and ccache on the Linux and macOS legs #281): the `CI Status` aggregator, `merge_group` handling, the planner and draft-pull-request rule, ccache and ctest phases, then the shared workflows one at a time. `CI Status` and `Scan dependency diff` are now the two required checks. Left: only `policy.yml` is a shared workflow so far; `zizmor`, `docs`, `docs-deploy`, `dependabot-auto-merge`, `dependency-review` and the nightly report are still this project's own files.
9. (Done on 2026-10-29: default branch `develop`, squash only, `branch-names`, `develop-protection`, `merge-queue-develop`, `main-release-only`, `tags`; the old `main-protection` ruleset deleted.) Settings: the branch model (default branch, squash and linear history, `branch-names`, `tags`, `main-release-only`, the `release` environment, labels, variables). Applied by the maintainer from the dry run of `admin/apply.sh`.
10. (Done: the licence wording #277 on 2026-10-30, then the SPDX headers and `REUSE.toml` #280.) Licence wording, then SPDX headers and `REUSE.toml` (its own pull request, scheduled).
11. (In part. Done: the glibc floor check on the Linux package, #260, and `docs/signing-key.asc` with the organisation key, #266. Left: the release workflow on the shared `release-publish.yml`, the `-unsigned` and `-testsigned` file names, and the records of the signing key in `SECURITY.md` and `docs/releasing.md`.) Release: the glibc floor check on the Linux package (#260), the signing key, `docs/signing-key.asc`, the release workflow on the shared `release-publish.yml`, `-unsigned` and `-testsigned` file names.
12. (In part. Done: `AGENTS.md`, `CONTRIBUTING.md` and the root `SECURITY.md`, #273. Left: `docs/releasing.md`, which still describes the earlier release flow and has no owner runbook.) Documentation: `AGENTS.md`, `CONTRIBUTING.md` (the branching text changes only after step 9), root `SECURITY.md`, `docs/releasing.md` with the owner runbook.
13. (In part. Done: the iPhone app goes to TestFlight through the shared signing actions, #270, from 0.2.0-beta.2; Android has its application id, #255. Left: a signed Android bundle, which waits for an Android camera backend.) Mobile signing and distribution, after the iOS decision.

## Owner actions

These change settings, secrets or accounts and are done by the maintainer.

Done: secret scanning, push protection and Dependabot security updates; the `release` environment with the maintainer as required reviewer; the labels and the variables `SIGN_WINDOWS`, `SIGN_MACOS` and `SIGN_IOS_UPLOAD`; the repository secret `REPO_GPG_PRIVATE_KEY` deleted. The Dependabot secret `MERGE_QUEUE_TOKEN` is the organisation's, which reaches this repository. The branch-model settings (step 9) were applied on 2026-10-29. The repository secret `IOS_PROVISIONING_PROFILE_BASE64` is set, and `SIGN_IOS_UPLOAD` is `true`.

To do:

- After the iOS pull request merges, delete the repository secrets `IOS_DIST_CERTIFICATE_BASE64`, `IOS_DIST_CERTIFICATE_PASSWORD`, `APPLE_TEAM_ID` and `APPLE_PROVISIONING_PROFILE_NAME`. The pull request merged on 2026-10-29 (#270) and no workflow or document names these four any more, so they are unused; whether they are deleted is a setting that cannot be read from here.
- iOS account items (App Store Connect app record, an App Store distribution profile as `IOS_PROVISIONING_PROFILE_BASE64`, access of the five organisation Apple secrets to this repository, the TestFlight internal group, the variable `SIGN_IOS_UPLOAD`). The step list is in [Cutting a release](releasing.md#the-iphone-signing-material) and [iPhone build: signing and releasing](ios-signing.md). That the builds reach TestFlight is shown by the 0.2.0-beta.3 and 0.2.0-beta.4 entries in the changelog; the account items themselves are not readable from here.
- Create the tags (including `v0.2.0-beta.2`) and approve the `release` environment for each release. The tags `v0.2.0-beta.2`, `.3` and `.4` exist. The GitHub releases for `.3` and `.4` are still drafts, and none is listed for `.2` (`gh release list`, 2026-11-22).
- TODO(owner): Apple team ID, App Store name, any contact addresses the documents need.

## Kept as they are

These are project features that the standard has no room for or that go beyond it. They stay.

- The WebAssembly application, which was on GitHub Pages under `/app/` beside the documentation and is now served under `/app/` of the company site (iainchesworthlabs.com/cubeulator/).
- `docs/design/`, `docs/investigations/` and the spike process.
- The platform-isolation check (`tools/checks/check_platform_isolation.ps1`); the shared preprocessor check runs beside it.
- Performance and profiling tools, ProfileRecorder, and Tracy as a dormant build.
- Third-party notices generation, the Qt module SBOM and the Licences screen.
- The nine-language set, `docs/i18n.md` and `docs/i18n-lanes.md`.
- Per-package SPDX and CycloneDX SBOMs, attestation read-back, the release dry run, and the `changelog.d/` types.
- The `Commit is green` release gate, cache hygiene, the OSV and Scorecard workflows, the nightly report that closes its own issue, and the `legs` dispatch.

## Changes to ask of the shared repositories

Each is made at its origin in a pull request of its own before cubeulator pins it.

- A project list of extra branch prefixes for `branch-names` and `check_branch_name.py` (done, infra #38).
- A `pre-release-script` input, a `notes-script` input and `spdx-sbom` and `cyclonedx-sbom` inputs (with attestation and read-back) on `release-publish.yml` (done, infra #35).
- A Ninja pin in `setup-cmake` (done, infra #36); the LLVM key fingerprint check in `install-llvm.sh` (done, infra #37).
- Platform-isolation rules for OS headers, OS tests in CMake and unwired platform files.
- The nightly report reader that closes its issue on a green night; `cache-hygiene.yml`; the Scorecard workflow; zizmor pinned through a requirements file.
- Standard text: patched upstream ports stay overlays or go upstream (4.2); `src/lib` and `src/app` count as `libs/<name>` and `apps/<name>` in a single-library, single-application repository (D14); Quick 3D may stay while the store decision is open (7.1); Tracy may be present but dormant in desktop release builds (8); OpenCV intrinsics may serve for SIMD where OpenCV is already a dependency (9); a ninth language (D3); mobile orientation follows the device where the design says so, as an exception to the locked orientation of 7.7.
