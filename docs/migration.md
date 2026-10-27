# Aligning cubeulator with the ICL standard

This is the plan for bringing cubeulator in line with the C++23 project standard (`docs/standard.md` in `iainchesworthlabs/icl-common-infra`, version 0.13). cubeulator is an existing project and one of the repositories the standard was drawn from, so the work is an alignment of a working project. It is kept up to date until the last item is done, then trimmed to a short record.

The code conventions of this repository stay: Google-based layout with attached braces at 100 columns (`.clang-format`), the naming in `.clang-tidy`, `Result<T>` over `std::expected` (`docs/architecture.md`), and the `src/lib` and `src/app` layout. The standard's code style applies to new projects only (decision D9).

## Decisions made

| Date | Topic | Decision | Consequence |
|---|---|---|---|
| 2026-10-27 | Branch model | Adopt the standard's D1. `develop` is the default branch and the trunk; pull requests squash through the merge queue; `main` holds published releases and is fast-forwarded by the release bot after a successful release. No pull requests into `main`, no back-merge. | Rulesets, branch names, `release.yml`, `release-check.yml`, `docs.yml` and the contributor documents change. Order below |
| 2026-10-27 | vcpkg | Use the labs' fork `iainchesworthlabs/vcpkg` through `cmake/icl/IclVcpkgRoot.cmake` and the `setup-vcpkg` action, and remove the `deps/vcpkg` submodule. The baseline in `vcpkg.json` is the only pin. | One baseline pin; the `vcpkg-baseline-in-fork` check; the `gitsubmodule` Dependabot entry goes. The Tracy overlay port stays for now (open item) |
| 2026-10-27 | CI | Hybrid. Adopt the shared `policy`, `zizmor`, `docs`, `docs-deploy`, `dependabot-auto-merge`, `dependency-review`, `ci-status` and `report-nightly-failure` workflows and the `setup-*` actions. Keep the project's own jobs where the shared workflows do not cover them: clang-format, platform isolation, clang-tidy, the three desktop legs with packaging, Android, web and web tests, iOS, system tests, coverage and performance. | `CI Status` becomes the required check on `develop`. Tiers and the queue run are open items below |
| 2026-10-27 | Licence | The source stays `GPL-3.0-or-later`. A binary that links Qt Quick 3D is GPL-3.0-only, and the project says so (About text, package metadata, `docs/licensing.md`, `LICENSE-NOTE.md`). Qt Quick 3D stays. | The Apple stores remain blocked by this position; the decision of 2026-10-13 to leave them alone stands. TestFlight is therefore an open item. Wording changes in their own pull request |

## Open items

Each has the default that is used until it is decided. The options are in the maintainer's decision list.

| Item | Default until decided |
|---|---|
| Spike branches: `spike/*` is outside the `branch-names` ruleset | Keep the weekly sweep; ask for a project list of extra prefixes in the ruleset and `check_branch_name.py` |
| The `release/<version>` preparation branch (the standard reserves `release/x.y` for support branches) | Rename to `chore/release-<version>` when the branch model lands |
| Tracy overlay port (`vcpkg-overlay-ports/tracy`) | Keep the overlay; send `manual-lifetime` and `only-localhost` upstream |
| Which legs gate a pull request, which the queue and which the push; `merge_group` handling | Static checks, clang-tidy, Linux GCC build and tests and coverage on the pull request; Windows and macOS in the queue; Android, iOS, web, web tests and system tests after the merge |
| CodeQL on every pull request or nightly | Keep every pull request |
| iOS: TestFlight through the shared actions, or ad hoc | No change until decided. The ad-hoc IPA stays off public releases from the next release |
| Android: shipped target, application id, orientation, signed AAB | Set the application id and lock the orientation; no store build |
| Signing key: organisation key `FD13CB8C…4DBE` or the project key | The project key, as now |
| Release tooling: how much `release-publish.yml` replaces | Keep `tools/release/*` and `changelog.d/`; extend the shared workflow before adopting it |
| Coverage | Gate at 80/80 until the baseline (89.8% lines, 85.8% branches on the library layer) is recorded as the ratchet floor |
| Sanitiser, TSan and fuzz presets; Windows ARM64, Linux ARM64, macOS x64, iOS simulator | Add the Linux sanitiser, TSan and fuzz presets; the other platforms wait for a stated need |
| Language set: Arabic is a ninth language; `zh` against `zh_CN` | Keep nine; no rename |
| Copyright wording in SPDX headers | `Copyright (C) 2026 Iain Chesworth`, as the repository states it today |
| Enumerator and constant naming (26 lower_case enums against 20 PascalCase; about 520 `kPascal` constants against 41 lower_case) | Write both patterns into `.clang-tidy` without renaming |
| Portable solver library | Recorded as a candidate in `docs/architecture.md`; no split |

## Order of pull requests

Each pull request is one step and its checks are green before the next starts. Anything that touches many files is scheduled with the maintainer, so it does not collide with feature work, and lands alone.

1. This plan.
2. Back-merge #246, with a merge commit, so that `main` is an ancestor of `develop`.
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

- Merge pull request #246 with a merge commit (the only method the rulesets allow today).
- Turn on secret scanning, push protection and Dependabot security updates.
- Create the Dependabot secret `MERGE_QUEUE_TOKEN`.
- Apply the branch-model settings from the `admin/apply.sh` dry run (step 9), including making `develop` the default branch.
- Create the `release` environment with the maintainer as required reviewer.
- If the organisation key is chosen: delete the repository secret `REPO_GPG_PRIVATE_KEY`.
- iOS account items (App Store Connect record, App Store profile, access of the organisation secrets to this repository, TestFlight group) once TestFlight is decided.
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
