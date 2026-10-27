# Aligning cubeulator with the ICL standard

This is the plan for bringing cubeulator in line with the C++23 project standard (`docs/standard.md` in `iainchesworthlabs/icl-common-infra`, version 0.13). cubeulator is an existing project and one of the repositories the standard was drawn from, so the work is an alignment of a working project. It is kept up to date until the last item is done, then trimmed to a short record.

The code conventions of this repository stay: Google-based layout with attached braces at 100 columns (`.clang-format`), the naming in `.clang-tidy`, `Result<T>` over `std::expected` (`docs/architecture.md`), and the `src/lib` and `src/app` layout. The standard's code style applies to new projects only (decision D9).

## Decisions made

All decided on 2026-10-27.

| Topic | Decision | Consequence |
|---|---|---|
| Branch model | Adopt the standard's D1. `develop` is the default branch and the trunk; pull requests squash through the merge queue; `main` holds published releases and is fast-forwarded by the release bot after a successful release. No pull requests into `main`, no back-merge. | Rulesets, branch names, `release.yml`, `release-check.yml`, `docs.yml` and the contributor documents change. `main` became an ancestor of `develop` with the back-merge #246. `spike/*` stays outside the ruleset: ask for a project list of extra prefixes in the ruleset and `check_branch_name.py`; the weekly sweep stays. The `release/<version>` preparation branch becomes `chore/release-<version>` |
| vcpkg | Use the labs' fork `iainchesworthlabs/vcpkg` through `cmake/icl/IclVcpkgRoot.cmake` and the `setup-vcpkg` action, and remove the `deps/vcpkg` submodule. The baseline in `vcpkg.json` is the only pin. | One baseline pin; the `vcpkg-baseline-in-fork` check; the `gitsubmodule` Dependabot entry goes. The Tracy overlay port stays and its two features go upstream |
| CI | Hybrid. Adopt the shared `policy`, `zizmor`, `docs`, `docs-deploy`, `dependabot-auto-merge`, `dependency-review`, `ci-status` and `report-nightly-failure` workflows and the `setup-*` actions. Keep the project's own jobs where the shared workflows do not cover them: clang-format, platform isolation, clang-tidy, the three desktop legs with packaging, Android, web and web tests, iOS, system tests, coverage and performance. | `CI Status` becomes the required check on `develop` and `merge_group` stays on `ci.yml`. Tiers: static checks, clang-tidy, Linux GCC build and tests and coverage on the pull request; Windows and macOS in the queue; Android, iOS, web, web tests and system tests after the merge. CodeQL stays on every pull request |
| Licence | The source stays `GPL-3.0-or-later`. A binary that links Qt Quick 3D is GPL-3.0-only, and the project says so (About text, package metadata, `docs/licensing.md`, `LICENSE-NOTE.md`). Qt Quick 3D stays. | Wording changes in their own pull request. The copyright line stays `Copyright (C) 2026 Iain Chesworth` |
| iOS and Android | iOS goes to TestFlight (internal testers) through the shared `import-ios-signing`, `export-ios-ipa` and `upload-testflight` actions, with validation only on branch runs. The ad-hoc IPA leaves the public release; the IPA and install manifest were removed from v0.2.0-beta.1 on 2026-10-27. Android gets its application id (`com.iainchesworthlabs.cubeulator`) and a locked orientation. No store submission. | The ad-hoc steps, `docs/ios-install.md` and `docs/install-ios.html` are replaced by the TestFlight route. The Linux package is checked against the glibc 2.39 floor and built on a 24.04 image |
| Signing key | Releases are signed with the organisation key (`FD13CB8C…4DBE`). The maintainer deletes the repository secrets. | `docs/signing-key.asc` and `SECURITY.md` carry the organisation key; `SECURITY.md` records that releases up to v0.2.0-beta.1 used the project key `0CF2A242…EB46` |
| Release tooling | `release.yml` is rewritten on the shared `release-publish.yml`. The shared workflow gains a notes input, an `sbom-files` input with attestation read-back, and a `pre-release-script`; `tools/release/*` and `changelog.d/` stay. The draft step stays for one release and is then dropped | Pull requests to `icl-common-infra` first, each with a tag the maintainer creates |
| Next release | A corrected pre-release `v0.2.0-beta.2` follows the glibc fix and the iOS asset change | Tag commands from the maintainer |
| Layout | `src/lib` and `src/app` stay | Recorded in `docs/architecture.md`; the two GPU files that `cube_lib` compiles from `src/app/platform/` move into the library |
| Coverage | The baseline (89.8% lines, 85.8% branches on the library layer) is recorded as the ratchet floor and raised each release towards 90/80. The app is measured separately | A floor file and a `quality-history` branch with badges |
| Sanitisers and presets | Linux ASan/UBSan (`-O1`, Tracy off), TSan and fuzz presets are added. Windows ARM64, Linux ARM64, macOS x64 and the iOS simulator wait for a stated need | Tracy stays dormant-on in desktop release builds (extension request) |
| Languages | Nine are kept; Arabic is added to the standard's list. The Chinese catalogue and setting are renamed from `zh` to `zh_CN`, with a migration of the saved setting | Fonts for Hebrew, Yiddish and Arabic are bundled as subsets |
| Naming | Both enumerator styles and the `kPascal` constants are written into `.clang-tidy`; nothing is renamed | The nested `src/app/.clang-tidy` covers the Qt-facing code |
| Portable libraries | The solver and N x N model are recorded as candidates in `docs/architecture.md`; no split | |

## To confirm later

- Signed Android bundle and store build: when Android has a camera backend.
- `SIGN_WINDOWS=test` (the organisation test-signing secrets already reach this repository) and `SIGN_MACOS`: before the first release on the new workflow.
- Release notes of v0.2.0-beta.1 still name the removed IPA.

## Order of pull requests

Each pull request is one step and its checks are green before the next starts. Anything that touches many files is scheduled with the maintainer, so it does not collide with feature work, and lands alone.

1. This plan.
2. Back-merge #246, so that `main` is an ancestor of `develop` (done).
3. Pending Dependabot updates (#247 to #249).
4. Toolchain pins: `.github/toolchain-versions.json`, the vendored `cmake/icl` with `cmake/icl/SOURCE`, `tools/ci/sync_cmake.py` and `repin.py`, and `FindQt6` reading `ICL_QT_ROOT`.
5. vcpkg: switch to `IclVcpkgRoot.cmake` and `setup-vcpkg`, remove the submodule, drop the `gitsubmodule` Dependabot entry, add the `vcpkg-baseline-in-fork` check.
6. Conventions written down: the `readability-identifier-naming` section of `.clang-tidy`, `.editorconfig`, `.git-blame-ignore-revs`, the Objective-C++ document in `.clang-format`.
7. Hardening flags and the missing presets; the shared preprocessor check with its allowlist.
8. CI: the `CI Status` aggregator, `merge_group` handling, the planner and draft-pull-request rule, ccache and ctest phases, then the shared workflows one at a time. The two required checks of today stay until `CI Status` has passed on a code change and on a documentation change.
9. Settings: the branch model (default branch, squash and linear history, `branch-names`, `tags`, `main-release-only`, the `release` environment, labels, variables). Applied by the maintainer from the dry run of `admin/apply.sh`.
10. Licence wording, then SPDX headers and `REUSE.toml` (its own pull request, scheduled).
11. Release: the glibc floor check on the Linux package, the signing key, `docs/signing-key.asc`, the release workflow on the shared `release-publish.yml`, `-unsigned` and `-testsigned` file names.
12. Documentation: `AGENTS.md`, `CONTRIBUTING.md` (the branching text changes only after step 9), root `SECURITY.md`, `docs/releasing.md` with the owner runbook.
13. Mobile signing and distribution, after the iOS decision.

## Owner actions

These change settings, secrets or accounts and are done by the maintainer.

- Turn on secret scanning, push protection and Dependabot security updates.
- Create the Dependabot secret `MERGE_QUEUE_TOKEN`.
- Apply the branch-model settings from the `admin/apply.sh` dry run (step 9), including making `develop` the default branch.
- Create the `release` environment with the maintainer as required reviewer.
- Delete the repository secrets `REPO_GPG_PRIVATE_KEY`, and after the iOS pull request merges `IOS_DIST_CERTIFICATE_BASE64`, `IOS_DIST_CERTIFICATE_PASSWORD`, `APPLE_TEAM_ID` and `APPLE_PROVISIONING_PROFILE_NAME`.
- iOS account items (App Store Connect app record, an App Store distribution profile as `IOS_PROVISIONING_PROFILE_BASE64`, access of the five organisation Apple secrets to this repository, the TestFlight internal group, the variable `SIGN_IOS_UPLOAD`). The step list is in `docs/releasing.md` when it is written.
- Create the tags (including `v0.2.0-beta.2`) and approve the `release` environment for each release.
- TODO(owner): Apple team ID, App Store name, any contact addresses the documents need.

## Kept as they are

These are project features that the standard has no room for or that go beyond it. They stay.

- The WebAssembly application on GitHub Pages under `/app/`, beside the documentation.
- `docs/design/`, `docs/investigations/` and the spike process.
- The platform-isolation check (`tools/checks/check_platform_isolation.ps1`); the shared preprocessor check runs beside it.
- Performance and profiling tools, ProfileRecorder, and Tracy as a dormant build.
- Third-party notices generation, the Qt module SBOM and the Licences screen.
- The nine-language set, `docs/i18n.md` and `docs/i18n-lanes.md`.
- Per-package SPDX and CycloneDX SBOMs, attestation read-back, the release dry run, and the `changelog.d/` types.
- The `Commit is green` release gate, cache hygiene, the OSV and Scorecard workflows, the nightly report that closes its own issue, and the `legs` dispatch.

## Changes to ask of the shared repositories

Each is made at its origin in a pull request of its own before cubeulator pins it.

- A project list of extra branch prefixes for `branch-names` and `check_branch_name.py`.
- A `pre-release-script` input, a notes input and an `sbom-files` input (with attestation and read-back) on `release-publish.yml`.
- A Ninja pin in `setup-cmake`; the LLVM key fingerprint check in `install-llvm.sh`.
- Platform-isolation rules for OS headers, OS tests in CMake and unwired platform files.
- The nightly report reader that closes its issue on a green night; `cache-hygiene.yml`; the Scorecard workflow; zizmor pinned through a requirements file.
- Standard text: patched upstream ports stay overlays or go upstream (4.2); `src/lib` and `src/app` count as `libs/<name>` and `apps/<name>` in a single-library, single-application repository (D14); Quick 3D may stay while the store decision is open (7.1); Tracy may be present but dormant in desktop release builds (8); OpenCV intrinsics may serve for SIMD where OpenCV is already a dependency (9); a ninth language (D3).
