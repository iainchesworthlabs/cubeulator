# Cutting a release

How a version of Cubeulator becomes downloadable packages on a GitHub Release (an installer for Windows, a disk image
for macOS, a `.deb` and an `.rpm` for Linux, and the browser build's site as a zip; the iPhone app goes to TestFlight,
see [below](#the-iphone-signing-material)), with the notes, the checksums and the signatures that go with them. The
workflow is `.github/workflows/release.yml`, which hands the packages to the shared `release-publish.yml` of
`icl-common-infra`, as the other projects of the family do; the tools are in `tools/release/`; what is in the packages is
[Packaging the desktop app](packaging.md).

A release is a deliberate human step: the tag is the owner's, and the publish job waits for the owner to approve the
`release` environment ([Branch protection](branch-protection.md)).

## The shape of a release

```text
feature/* bugfix/* ──PR──▶ merge queue ──▶ develop
                                              │
   chore/release-<version> ──PR──▶ develop    │   1. cut the changelog, set the version, write whats-new
                                              ▼
              tag v<version> on that develop commit      2. `Tag a release` (the bot), or `git tag` and push
                                              ▼
              Release: verify, build five platforms,     3. release.yml
              the iPhone app to TestFlight, the SBOMs
                                              ▼
              you approve the `release` environment      4. release-publish.yml: SHA-512 sums, GPG, attestations,
                                              ▼             the GitHub Release
              final releases only: the bot              5. `main` fast-forwards to the tag
              fast-forwards `main`
```

- **Releases are cut from `develop`.** The version is set and the changelog cut in a pull request into `develop` that
  goes through the merge queue like any other, from a `chore/release-<version>` branch (dashes for the dots: `chore/release-1-0-0`). The tag is on a commit of `develop`:
  the workflow refuses any other.
- **The tag is the build.** A tag starts `release.yml`. It checks the tree and the tag (`tools/release/check_release.py`),
  that the commit is green, and builds the packages; the shared workflow then signs and attests them and makes the
  release with the notes rendered from `CHANGELOG.md`.
- **A tag the bot makes starts the workflow; one made with the workflow's own token would not.** The `tags` ruleset lets
  only organisation admins and the release bot create a `v*` tag. `tag-release.yml` (Actions, *Tag a release*, run on
  `develop`) makes it with the bot's token, so that a release can be started from a phone; pushing a tag from a
  checkout does the same.
- **`main` follows a final release.** After the release exists, the bot fast-forwards `main` to the tag (never a force),
  so `main` is always an ancestor of `develop`, holds only released commits, and there is no back-merge. A pre-release
  (`-alpha`, `-beta`, `-rc`) leaves `main` where it is.

## What a release holds

| File | What it is |
|---|---|
| `Cubeulator-<version>-windows-x64.exe` | Windows 10 and later, 64-bit: the NSIS installer |
| `Cubeulator-<version>-macos-arm64.dmg` | macOS 14.4 and later, Apple Silicon |
| `Cubeulator-<version>-linux-x64.deb` | Debian and Ubuntu |
| `Cubeulator-<version>-linux-x64.rpm` | Fedora |
| `Cubeulator-<version>-web.zip` | The browser build: the static site that the company site serves under `/app/` ([In your browser](guide/web.md)), with `LICENSE.txt`, `THIRD-PARTY-NOTICES.txt` and `HOSTING.txt`, to be hosted over https. Not an installer; held by `packaging/web/check-package.sh` |
| `SHA512SUMS` | one line per package and for each SBOM |
| `SHA512SUMS.asc`, `<file>.asc` | detached GPG signatures, from the organisation's release key (`docs/signing-key.asc`), on the manifest, every package and each SBOM |
| `Cubeulator-<version>.spdx.json` | an SPDX 2.3 SBOM: for each package, every component that ships in it, with version and licence, from the build's own third-party inventory; checksummed and signed like the packages |
| `Cubeulator-<version>.cdx.json` | the same bill of materials as CycloneDX 1.6, for the scanners that read that format; made from the SPDX document, so the two cannot disagree; checksummed, signed and attested like it |

Every package also carries a **build-provenance attestation** (Sigstore, from
the workflow's own identity) and an attestation of each SBOM (SPDX and CycloneDX), held by GitHub, not as
files.

The SBOM is made by `tools/release/make_sbom.py` from the
`third-party-notices.json` each build writes (the same inventory behind
`THIRD-PARTY-NOTICES.txt`; see [Licensing](licensing.md)), not by scanning the
installers, which would list an installer and nothing in it. It refuses an
inventory with anything unresolved and a package whose platform has no
inventory. The workflow reads each SBOM back from its attestation and compares
it with the file it is releasing, so an SBOM that was attested is the SBOM that
was attached. [Verifying a download](#verifying-a-download) says how to check each.

**The macOS and Linux packages are not OS code-signed** (no Apple Developer ID), and **the Windows installer is signed only
with a test certificate**, or not at all: the repository variable `SIGN_WINDOWS` is `false`, `test` or `production`
(`icl-common-infra`, `docs/test-signing.md`). In `test` mode the program and the installer are signed with a leaf
certificate of the owner's personal certificate authority, with a timestamp. Only a machine that has installed that
authority's root trusts the signature, so SmartScreen still asks everyone else; `production` is for a real code-signing
service, which has not been chosen and fails the build until it is. The package keeps its name in every mode. The notes
say what a person will see (SmartScreen on Windows, Gatekeeper on macOS); where signing plugs in is the
[Signing section of packaging.md](packaging.md#signing). The GPG signature and the attestation prove where a file came
from; they do not stop SmartScreen from asking.

## Versions

`M.M.P`, or `M.M.P-label.N` for a pre-release, where the label is `alpha`,
`beta` or `rc`: `0.1.0-beta.1`, `0.1.0-rc.1`, `0.1.0`. The tag is `v` and that.

While the project is `0.x`, a release with new features moves the minor
(`0.1.0` → `0.2.0`), a release of fixes moves the patch, and a pre-release
counter moves within a line (`0.2.0-beta.1` → `0.2.0-beta.2`). `1.0.0` is for
the first release the project is willing to call stable.

The version lives in **two lines of the top-level `CMakeLists.txt`**:

```cmake
project(Cubeulator VERSION 0.2.0 ...)
set(CUBEULATOR_VERSION_PRERELEASE "beta.4")     # "" for a final release
```

`tools/release/prepare_release.py` sets both, and the manifest's own `"version"`
in `vcpkg.json` to match (`check_release.py` fails a tree where they differ).
Everything else follows:

| | |
|---|---|
| Package file names | `Cubeulator-0.1.0-beta.1-windows-x64.exe`: a beta is never called by the final's name |
| `cube::version_string`, About | `0.1.0-beta.1` |
| Windows installer, version resource | `0.1.0-beta.1` (the resource's numeric fields are `0,1,0,0`) |
| `.deb`, AppStream | `0.1.0~beta.1` (`~` sorts before `0.1.0`, so `apt upgrade` takes a beta to the final) |
| `.rpm` | version `0.1.0`, release `0.beta.1` (`1` for a final) |
| `vcpkg.json` | `"version": "0.1.0"` (vcpkg takes no label) |
| macOS `Info.plist` | `0.1.0` (macOS wants three numbers) |

The Release workflow refuses a tag that does not name the pair exactly.

## The changelog

`CHANGELOG.md` is [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), for
people following what has shipped. It is written once, as work lands, and the
GitHub Release is rendered from it; nothing is retyped at release time.

```markdown
## [Unreleased]

## [0.1.0-beta.1] - 2026-10-20

One paragraph saying what this release is. It is the summary line of the notes.

### Added

- **Bold lead-in naming the change.** Then what it does and why, in the words of
  the person using it. See [docs/page.md](docs/page.md).

### Changed
### Fixed
### Removed
### Security

> **Pre-release.** What is not yet proven. Pre-releases only.
```

- **A change adds a fragment, not a line here.** A file
  `changelog.d/<slug>.<type>.md` holds one item
  ([`changelog.d/README.md`](https://github.com/iainchesworthlabs/cubeulator/blob/develop/changelog.d/README.md)).
  Pull requests that are open at once would otherwise all edit `[Unreleased]`
  and conflict. `prepare_release.py` merges the fragments into it, under their
  types, and deletes them; a release with any left over fails
  `check_release.py`, because its notes would miss them. What is written in
  `[Unreleased]` by hand (the summary, the pre-release caveat) stays there.
- Newest section first; one `## [Unreleased]` at the top.
- Subsections are `Added`, `Changed`, `Deprecated`, `Removed`, `Fixed`,
  `Security`, nothing else.
- Every item opens with a **bold lead-in**: it is what a person scans. Items say
  what changed for the user, not which file moved.
- Links to the docs are relative (`docs/x.md`); the notes rewrite them to the
  tagged tree.
- A pre-release closes with the `> **Pre-release.**` blockquote. What goes in it
  is a judgement: the single biggest thing that is not yet proven.

`tools/release/changelog.py` holds these rules; `check_release.py` applies them
to the section being released and fails with the line that is wrong.

### What the release page says

`tools/release/render_release_notes.py` lays the section out the way every
release of the project family reads:

1. `## What's Changed since v<previous>` (the first release: `## What's Changed`)
2. the summary paragraph
3. the section's `###` subsections, as written
4. `## Installing`: what each platform does on first run
   (`tools/release/install-notes.md`)
5. `## Artifacts`: the *Platform | Files* table, the verification row, and a link
   to the README's install section
6. `**Full Changelog**: …/compare/v<previous>...v<this>` (not the first release)
7. the `> **Pre-release.**` caveat

The Artifacts sentence ("with build-provenance attestations, SPDX and CycloneDX SBOMs and GPG
signatures") lists only what the run did. Preview it any time:

```sh
python tools/release/render_release_notes.py --version 0.1.0-beta.1 \
    --repo iainchesworthlabs/cubeulator --from-unreleased --signed --attested --sbom
```

## Preparing the release

1. **Be ready.** Every workflow the README has a badge for (CI, CodeQL,
   OSV-Scanner and Zizmor) is green on the `develop` commit you mean to
   release, and `python tools/release/check_release.py --dry-run` is clean (it
   reads `[Unreleased]` as if it were cut and applies the same rules).

   ```sh
   python tools/release/check_health.py        # develop's tip; needs an authenticated gh
   ```

   names the workflow that is red, still running or has not run. The README's
   badges show those workflows' latest runs on the default branch (`develop`), so
   a release made from a red commit is a red front page. Two things make "green"
   take longer than the
   pull request's checks suggest. `ci.yml`'s push run on `develop` is hours, and
   a push that arrives while an earlier push's run is still *waiting* replaces
   it (the replaced run reads `cancelled`), so once the PR that prepares the
   release (step 2) has merged, let nothing else merge into `develop` until this
   says green; ask whoever runs the merge queue to hold it. And a `cancelled`
   run is not a pass: re-run it (`gh run rerun <id>`).

   The [user guide](guide/index.md) still describes the app: its pictures are
   from the [screenshot tour](testing.md#the-screenshot-tour) and are
   refreshed with `tools/docs/update_guide_screenshots.py`, and it names what
   does not work yet, so read its "What this release is" box against the
   changelog.

   Every pull request since the last tag that a person would notice has a
   fragment in `changelog.d/` (or the `no-changelog` label, which says it has
   none to write). The `Policy` check asks at review time; this asks for the
   whole interval, and should print only changes nobody reads about (a
   dependency bump, a CI change):

   ```sh
   gh pr list --base develop --state merged --search "merged:>=<date of the last tag>" -L 300 \
       --json number,title,labels,files --jq '.[]
       | select(([.labels[].name] | index("no-changelog")) | not)
       | select(([.files[].path] | map(startswith("changelog.d/")) | any) | not)
       | "\(.number) \(.title)"'
   ```
2. **Prepare the release on a branch.**

   ```sh
   git switch develop && git pull
   git switch -c chore/release-1-0-0
   python tools/release/prepare_release.py 1.0.0
   ```

   That moves `[Unreleased]` under `## [1.0.0] - <today>`, merges the `changelog.d/` fragments in, and sets the version
   pair (`project(VERSION)` and `CUBEULATOR_VERSION_PRERELEASE`) and the manifest's. It says what the section still needs
   (the summary, the bold lead-ins, the caveat for a pre-release); write those. `--check` changes nothing and only says
   what is wrong. Change `docs/whats-new.md` in the same pull request: the new version in bold at the top, and a short
   section that says in plain words what changed and what is not yet proven; the `Site` check fails until the page names
   the newest release of `CHANGELOG.md`. A final release drops the pre-release wording of the README, the FAQ and the
   guide's "What this release is" box. Open a PR into `develop` labelled `no-changelog` (the changelog is assembled by this very change; the branch name must follow `chore/`, and `release/x.y` is for a fix to an old release); it goes
   through the merge queue like any other.

   Pull requests that were open when you prepared the release can merge after it, each with a fragment, and the release
   then fails `check_release.py` (its notes would miss them). Fold them into the section that is already cut:

   ```sh
   python tools/release/prepare_release.py 1.0.0 --fold
   ```

   It adds every fragment still in `changelog.d/` to that version's section (which must be the newest) and deletes them;
   it does not touch the version. Do it in a PR that has merged before you tag, not after.
3. **Rehearse.** Run the [rehearsal](#the-rehearsal) from `develop` after the preparing PR has merged, and read what it
   produced. For a final release, also cut a release candidate first (`1.0.0-rc.1`, a pre-release, through steps 2 to 6
   with the version `1.0.0-rc.1`): it is the only way to see the signing, the attestations, the TestFlight upload and the
   `release` approval work on the real thing, and it can be deleted afterwards. A candidate does not move `main`.
4. **Tag it.** Actions → **Tag a release** → *Run workflow*, branch `develop`, `version` = `1.0.0`. It checks that the
   version, the changelog and the tag agree, refuses a tag that exists, and makes `v1.0.0` on the tip of `develop` with the
   release bot's token. Look at the tip first: it is the commit that is released, and the green gate in step 1 is asked of it again before
   anything is built (of the `develop` tip, if it is a merge commit). If something is red for a reason that has nothing
   to do with the change (a flaky scheduled leg, a runner that died), re-run what is red (`gh run rerun <id> --failed`)
   and tag after it is green. Or push the tag yourself (the `tags` ruleset lets only organisation admins and the release
   bot create a `v*` tag):

   ```sh
   git switch develop && git pull
   git tag -a v1.0.0 -m "Cubeulator 1.0.0" && git push origin v1.0.0
   ```
5. **Watch the `Release` workflow.** `Verify the release` is quick; the three desktop builds, the web build and the
   iPhone app are the long ones (a cold vcpkg cache is longest on Windows), then the SBOMs. The iPhone app goes to
   TestFlight as soon as it is built, before anything is published.
6. **Approve the release.** The `Publish` job waits on the `release` environment: approve it in the run (Actions, the
   run, *Review deployments*, or the GitHub mobile app) when the builds are green. It then writes `SHA512SUMS`, signs
   everything with the release key and checks the signatures against `docs/signing-key.asc`, attests the build
   provenance and the SBOMs and reads the attestations back, renders the notes and creates the release (a pre-release
   if the version has a label). It refuses to publish if any of that fails. Look at the release: the notes, the five
   packages, the SBOMs, the signatures.
7. **`main` moves by itself** after a final release: the job `Fast-forward release branch` makes `main` the tag's commit
   with the release bot. There is no back-merge. Check that `main` is the tag.
8. **Publish the site's pages** if `docs/whats-new.md` changed: `gh workflow run publish-site.yml --ref develop`
   ([Publishing the site](ci.md#publishing-the-site)). A release does not publish them.
9. **Look at the front page.** The README's badges show the latest runs of their workflows on the default branch. The
   OpenSSF Scorecard badge reads "invalid repo path" until Scorecard has published a result, which it does only from a
   run on the default branch; `scorecard.yml` still triggers on a push to `main` (and on Mondays), so it now runs when
   `main` moves. Check:

   ```sh
   python tools/release/check_health.py --scorecard
   ```

   If it still reports no result a quarter of an hour after `main` moved, open the `OSSF Scorecard` run on `main`: a
   rejected result is a warning annotation on the *Run Scorecard analysis* step, with the API's reason (usually a step or
   setting added to `scorecard.yml` that its publishing rules do not allow; they are listed in that file).

## The rehearsal

Actions → **Release** → *Run workflow*, branch `develop` (or the release branch). A run by hand is the whole release up to
the publish job, which needs a tag:

| | rehearsal | release |
|---|---|---|
| version, changelog and tag checks | yes (`[Unreleased]` stands in for a section not cut yet) | yes |
| build, package, `check-package.sh` (`packaging/web/check-package.sh` for the zip), five platforms | yes | yes |
| the Windows files signed as `SIGN_WINDOWS` says | yes | yes |
| the iPhone app | built, and Apple validates it (untick `ios` to leave it out) | built and uploaded to TestFlight |
| SPDX and CycloneDX SBOMs from the build's inventories | yes | yes |
| the commit is green on CI, CodeQL, OSV-Scanner and Zizmor | reported, not enforced (a rehearsal branch is rarely green) | enforced |
| `SHA512SUMS`, GPG signatures, attestations and the read-back, the notes, the release, `main` | **no** | yes, after you approve |

What it leaves is one artifact per package in the run (`release-windows-x64`, `release-macos-arm64`, `release-linux-x64`,
`release-web`, `release-sbom`): the exact files a release would attach, unsigned by the release key. Download the installer,
install it on a machine, see what it does. Preview the notes without a run:

```sh
python tools/release/render_release_notes.py --version 1.0.0 \
    --repo iainchesworthlabs/cubeulator --from-unreleased --signed --attested --sbom
```

It can be run from any branch that has the workflow file, without a tag, and so before the pull request that prepares the
release has merged:

```sh
gh workflow run release.yml --ref <branch> -f ios=true
```

To rehearse a particular version end to end, prepare it on a scratch branch that is never merged (no pull request: a
`support/` branch into `develop` is not a valid flow), write a draft summary and caveat so the changelog check passes, and
dispatch from it:

```sh
git switch -c support/release-<version>-rehearsal origin/develop
python tools/release/prepare_release.py <version>      # then write the summary and the caveat
git commit -am "Prepare <version> (scratch branch, never merged)"
git push origin support/release-<version>-rehearsal
gh workflow run release.yml --ref support/release-<version>-rehearsal
gh run download <run id> -D rehearsal                   # the packages
```

With a warm vcpkg cache the run takes about an hour (the desktop builds are the long part); a cold one is hours. Delete
the scratch branch afterwards.

What a rehearsal cannot show is the publish job, the signature and the attestations: a release candidate does (step 3). A PR
that changes `release.yml`, `tag-release.yml`, `check-package.sh` or `tools/release/` runs the tools' self-test and lets
GitHub parse the files; `ci.yml` already builds and checks the packages on every push.

## The iPhone signing material

The iPhone app goes to TestFlight, not to the release page. It is signed with the
organisation's Apple Distribution certificate and this repository's App Store
provisioning profile, and uploaded with an App Store Connect API key. What is set,
where, and how it is made are in [iPhone build: signing and releasing](ios-signing.md).
In short: the repository variable `SIGN_IOS_UPLOAD` is `true`; the organisation secrets
`IOS_DIST_CERT_P12_BASE64`, `IOS_DIST_CERT_PASSWORD`, `APPSTORE_API_KEY_ID`,
`APPSTORE_API_ISSUER_ID` and `APPSTORE_API_PRIVATE_KEY` reach this repository; and the
repository secret `IOS_PROVISIONING_PROFILE_BASE64` holds the profile.

A release (and a rehearsal) with the iPhone build on and one of them missing fails in the
iOS job before anything is built. A real release uploads the build; a rehearsal has Apple
validate it. The IPA is not a release file, so the release carries no device
identifiers.

## The signing key

Releases are signed with the organisation's release key, `Iain Chesworth Labs Releases`, ed25519 (fingerprint
`FD13 CB8C AC83 1664 8CD9  74D0 8149 254C 4189 4DBE`, created 2026-10-22, expires 2028-10-21). Its secret half is held as the
organisation secrets `REPO_GPG_PRIVATE_KEY` and `REPO_GPG_PASSPHRASE`, which every repository of the family reads; there are
no repository secrets of that name here. Its public half is committed as `docs/signing-key.asc`, and the publish job refuses a
release in which any signature does not verify against that file, so a wrong key in the secret is found by the release and
not by a person downloading. The release does not carry a copy of the key: a person takes it from the repository, and
compares its fingerprint with the one printed in [SECURITY.md](SECURITY.md#verifying-build-authenticity) and on the
guide's [verify page](guide/verify.md). Releases up to 0.2.0-beta.1 were signed with an earlier project key, which those
pages also name.

Keys expire: put the expiry in a calendar. A rehearsal does not use the key, so the first proof that it is still good is a
release candidate. To renew or replace the key, change the two organisation secrets, replace `docs/signing-key.asc`, and change the
fingerprint in `SECURITY.md`, `docs/SECURITY.md` and `docs/guide/verify.md` in the same pull request (the `Site` check
fails if the verify page and `docs/SECURITY.md` disagree).

## Verifying a download

Each file has two independent proofs of origin. [SECURITY.md](SECURITY.md#verifying-build-authenticity)
says how to check them. In short:

```sh
# the checksums, then the signature on them
gpg --import docs/signing-key.asc
gpg --verify SHA512SUMS.asc SHA512SUMS && sha512sum -c --ignore-missing SHA512SUMS

# which repository, workflow and commit built a file
gh attestation verify Cubeulator-0.1.0-beta.1-linux-x64.deb --repo iainchesworthlabs/cubeulator
```

## What stops a release

The workflow refuses, with the reason in the log, when:

| | |
|---|---|
| The tag is not `vX.Y.Z` or `vX.Y.Z-label.N` | |
| The tag is not `v` + the version `CMakeLists.txt` declares, label included | the packages are named for it, so `v0.1.0` would hold `Cubeulator-0.1.0-beta.1-*` |
| `CHANGELOG.md` has no dated `## [version]` section, or it is not the newest | the notes are rendered from it and nothing else |
| The section has no summary, an item without a bold lead-in, or (pre-release) no closing caveat | the same rules `check_release.py` states |
| `changelog.d/` still holds fragments | they are not in the section, so the notes would miss them: run `prepare_release.py` |
| The tag is not on a commit of `develop` | `Check where the release is cut from`, and again in the publish workflow |
| The commit (for a merge commit, the `develop` tip it merged) is not green on CI, CodeQL, OSV-Scanner and Zizmor, or has a red check run | [step 1](#preparing-the-release); a rehearsal reports it and goes on |
| A package fails `packaging/check-package.sh` | Qt, the executable, the icon, the identifier, the camera string, the notices: the same checks `ci.yml` makes on every push |
| A platform produces no package, or two | `Make the SBOMs` counts them |
| `SIGN_WINDOWS` is `test` and the certificate secrets are missing, or it is `production` | the Windows build says which |
| The iPhone signing secrets are missing, the profile does not match them, or has expired | the iOS job says which; a tag push cannot leave the iPhone build out, a rehearsal can untick `ios` |
| `Tag a release` finds that the tag exists already | it refuses, and makes nothing |
| An organisation secret of the signing key is missing, or a signature does not verify against `docs/signing-key.asc` | the publish job says which |
| A package has no attestation that verifies, or an SBOM attested for it is not the file released | the publish job reads them back |
| Nobody approves the `release` environment | the publish job waits |

## If a release has to be undone

A published release can be deleted from the Releases page, but a tag people may have fetched should not be moved: tag the
next patch (or the next candidate counter) instead. If the release page was wrong and the files were right, edit the notes
in place. A `main` that has moved cannot be moved back by the workflow (the ruleset allows only a fast-forward); the owner
decides.

## Design choices, and why

- **The shared publish workflow.** Signing, attestation, the SBOM read-back and the release are the same steps for every
  project of the family, so they live in `icl-common-infra`. The project's part is what only it can do: build its five
  packages and the iPhone app, make its SBOMs from the build's own inventory, and write its notes.
- **The approval is the gate.** Nothing is published until the owner approves the `release` environment, after the builds
  are green; a draft release on top of that would be a second click for the same decision.
- **The tag is made by a bot, on request.** The owner can be anywhere: `Tag a release` is a button, and the bot's token is
  what lets a tag start a workflow and what the `tags` ruleset allows.
- **`main` only ever receives a released commit, by fast-forward.** A force is not possible, so a `main` that is not an
  ancestor of the tag stops the release.
- **The notes come from the changelog**, so that they are written once, where the change is made, and reviewed in a PR. A
  release with no matching section fails outright rather than falling back to a list of commits.
- **The gate reads the README.** The workflows it holds to green are the ones the README has a badge for, so a badge added
  there is held to the same rule without anyone remembering to edit the gate.
- **A cancelled run is not green.** `ci.yml` on `develop` lets a waiting push run be replaced by a newer one, so a commit
  can be passed over and never tested. The gate would rather say "re-run it" than treat the silence as a pass.
- **The build reads the vcpkg cache and never writes it**, so nothing a release run does can end up in what the other
  workflows trust.
- **Everything it uses is pinned** (see [CI & dependencies](ci.md)).
