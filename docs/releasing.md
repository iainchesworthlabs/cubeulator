# Cutting a release

How a version of Cubeulator becomes downloadable packages on a GitHub Release
(an installer for Windows, a disk image for macOS, a `.deb` and an `.rpm` for
Linux, and the browser build's site as a zip; the iPhone app goes to TestFlight, see
[below](#the-iphone-signing-material)), with the
notes, the checksums and the signatures that go with them. The workflow is
`.github/workflows/release.yml`; the tools are in `tools/release/`; what is in
the packages is [Packaging the desktop app](packaging.md).

A release is a deliberate human step (see [Branch protection](branch-protection.md)).
The workflow does everything up to a **draft** release; making it public is the
click that is yours.

## The shape of a release

```text
feature/* bugfix/* ──PR──▶ merge queue ──▶ develop
                                              │
        release/<version> ──PR──▶ develop     │   1. cut the changelog, set the version
                                              ▼
                     develop ──release PR (merge commit)──▶ main      2. the release decision
                                                             │
                                          tag v<version> ────┘        3. the Release workflow
                                                             ▼
                                        draft GitHub Release ──click──▶ published   4.
                                                             │
                                         main ──back-merge PR──▶ develop            5.
```

- **Releases go from `develop` into `main`.** The release PR is merged with a
  *merge commit* (never squash or rebase, or the two branches diverge and the
  next release PR re-lists every commit).
- **The changelog is cut on `develop`**, in the PR that sets the version, so
  that what `main` receives is already the release.
- **The tag is the build.** A tag on `main` (or a dispatch, below) runs the
  Release workflow, which builds, checks, signs and attests the packages, renders
  the notes from `CHANGELOG.md` and leaves a draft release.
- **`main` is back-merged into `develop`** afterwards, so that `develop` contains
  the release's merge commit and the next release PR lists only what is new.

## What a release holds

| File | What it is |
|---|---|
| `Cubeulator-<version>-windows-x64.exe` | Windows 10 and later, 64-bit: the NSIS installer |
| `Cubeulator-<version>-macos-arm64.dmg` | macOS 14.4 and later, Apple Silicon |
| `Cubeulator-<version>-linux-x64.deb` | Debian and Ubuntu |
| `Cubeulator-<version>-linux-x64.rpm` | Fedora |
| `Cubeulator-<version>-web.zip` | The browser build: the static site that [Try it in your browser](web.md) serves under `/app/`, with `LICENSE.txt`, `THIRD-PARTY-NOTICES.txt` and `HOSTING.txt`, to be hosted over https. Not an installer; held by `packaging/web/check-package.sh` |
| `SHA512SUMS` | one line per package and for each SBOM |
| `SHA512SUMS.asc`, `<file>.asc` | detached GPG signatures, from the project key, on the manifest, every package and each SBOM |
| `cubeulator-signing-key.asc` | the public key, so that a download can be verified offline |
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

**The packages are not OS code-signed** (no Authenticode certificate, no Apple
Developer ID). The notes say so and say what a person will see: SmartScreen on
Windows, Gatekeeper on macOS. Where that plugs in is the
[Signing section of packaging.md](packaging.md#signing). The GPG signature and
the attestation prove where a file came from; they do not stop SmartScreen from
asking.

## Versions

`M.M.P`, or `M.M.P-label.N` for a pre-release, where the label is `alpha`,
`beta` or `rc`: `0.1.0-beta.1`, `0.1.0-rc.1`, `0.1.0`. The tag is `v` and that.

While the project is `0.x`, a release with new features moves the minor
(`0.1.0` → `0.2.0`), a release of fixes moves the patch, and a pre-release
counter moves within a line (`0.2.0-beta.1` → `0.2.0-beta.2`). `1.0.0` is for
the first release the project is willing to call stable.

The version lives in **two lines of the top-level `CMakeLists.txt`**:

```cmake
project(Cubeulator VERSION 0.1.0 ...)
set(CUBEULATOR_VERSION_PRERELEASE "beta.1")     # "" for a final release
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

   names the workflow that is red, still running or has not run. The badges on
   `main` are those workflows' latest runs there, so a release that arrives with
   one red is a red front page. Two things make "green" take longer than the
   pull request's checks suggest. `ci.yml`'s push run on `develop` is hours, and
   a push that arrives while an earlier push's run is still *waiting* replaces
   it (the replaced run reads `cancelled`), so once the PR that prepares the
   release (step 3) has merged, let nothing else merge into `develop` until this
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
2. **Rehearse.** Run the pipeline as a [dry run](#the-dry-run) from `develop`
   (better: from a scratch branch that has run `prepare_release.py` for the
   version, so the cut, the fragments and the version check are rehearsed too)
   and read what it produced. Do this before every release: develop moves,
   and the dependency baseline, Qt and the runner images with it. The last
   rehearsal (2026-10-20, 0.2.0-beta.1 prepared on a scratch branch) passed on
   all four builds, the signing, the SBOM and every attestation, and the files
   it produced were verified with the commands in
   [SECURITY.md](SECURITY.md#verifying-build-authenticity).
3. **Prepare the release on a branch.**

   ```sh
   git switch develop && git pull
   git switch -c release/0.1.0-beta.1
   python tools/release/prepare_release.py 0.1.0-beta.1
   ```

   That moves `[Unreleased]` under `## [0.1.0-beta.1] - <today>`, merges the
   `changelog.d/` fragments in, and sets the version pair and the manifest's.
   It says what the section still needs (the summary, the bold lead-ins, the
   caveat); write those. `--check` changes nothing and only says what is wrong.
   Open a PR into `develop`; it goes through the merge queue like any other.

   Pull requests that were open when you prepared the release can merge after
   it, each with a fragment, and the release then fails `check_release.py` (its
   notes would miss them). Fold them into the section that is already cut:

   ```sh
   python tools/release/prepare_release.py 0.1.0-beta.1 --fold
   ```

   It adds every fragment still in `changelog.d/` to that version's section
   (which must be the newest) and deletes them; it does not touch the version.
   Do it in a PR before the release PR is opened, not after.
4. **Open the release PR**: `develop` → `main`, merge commit. The `Release
   readiness` check puts the rendered notes in its summary: read them there.
   `Commit is green` asks step 1's question of the PR's head commit and puts the
   table in its summary. It waits (up to three hours) for a workflow that has
   not finished and fails at once on a red workflow or check run. It is a
   **required check on `main`**, so a release (or a hotfix) cannot merge from a
   red commit without someone choosing to bypass it.

   If it goes red for a reason that has nothing to do with the change (a flaky
   scheduled leg, a runner that died), re-run what is red, which the table in
   its summary names (`gh run rerun <id> --failed`), and then re-run this check
   the same way; a push to the PR would restart every check instead. Merge
   with it red (`gh pr merge --admin`: the admin role bypasses through a pull
   request only, see [Branch protection](branch-protection.md)) only when the
   red is real and known not to matter for the change, such as a hotfix that
   has to go out while an unrelated leg is broken, and say why in the PR.

   `main`'s ruleset is in [Branch protection](branch-protection.md). It asks
   for "up to date", so anything that reaches `main` after the release PR's
   last green run (a hotfix, a back-merge) starts every check again, and the
   full matrix is hours on hosted runners: keep the release PR the last thing
   that touches `main`, and do the back-merge (step 9) after the tag.
5. **Tag the merge commit on `main`:**

   ```sh
   git switch main && git pull
   git tag -a v0.1.0-beta.1 -m "Cubeulator 0.1.0-beta.1"
   git push origin v0.1.0-beta.1
   ```

   or run the workflow by hand from `main` with `dry_run` unticked: it makes the
   tag itself, at the end, once everything has passed, so a failed run leaves
   no tag behind. The workflow asks step 1's question again before it builds:
   of the `develop` tip the merge commit merged (its second parent: that is what
   was tested, and the merge commit's own runs have only just started).
6. **Watch the `Release` workflow.** `Verify the release` is quick; the three
   desktop `Build` jobs and `Build the web app` are the long ones (a cold vcpkg
   cache is hours on Windows and macOS and for WebAssembly; the cache CI keeps
   makes it much less: the web leg's is `wasm32-emscripten-threads`). If a
   platform fails, fix it and tag again, or re-run the failed jobs.
7. **Review the draft** under Releases: the notes, the five packages, the
   checksums and signatures. Edit the notes if you want to.
8. **Publish it.**
9. **Back-merge**: a PR from a `support/back-merge-<version>` branch cut from
   `main`, into `develop`, so `develop` has the release.
10. **Look at the front page.** The README's badges on `main` show the runs the
    merge started, which are hours from done; the OpenSSF Scorecard badge reads
    "invalid repo path" (red) until Scorecard has published a result, and it
    publishes only from a run on the default branch, so the merge into `main`
    is what produces the first one: `scorecard.yml` runs on that push and the
    badge turns to a score within minutes. Check:

    ```sh
    python tools/release/check_health.py --scorecard
    ```

    If it still reports no result a quarter of an hour after the merge, open
    the `OSSF Scorecard` run on `main`: a rejected result is a warning
    annotation on the *Run Scorecard analysis* step, with the API's reason
    (usually a step or setting added to `scorecard.yml` that its publishing
    rules do not allow; they are listed in that file).

## The dry run

Actions → **Release** → *Run workflow*, branch `develop` (or the release
branch), `dry_run` ticked (it is ticked by default). It is the whole release
minus the two things that cannot be undone:

| | dry run | release |
|---|---|---|
| version, changelog and tag checks | yes (`[Unreleased]` stands in for a section not cut yet) | yes |
| build, package, `check-package.sh` (`packaging/web/check-package.sh` for the zip), five platforms (the iPhone app is checked by `packaging/ios/verify_ipa.sh` inside its build) | yes | yes |
| `SHA512SUMS`, GPG signatures | yes, with a key made for the run | yes, the project key |
| the provisioned key | proven to import and sign, in a keyring that is thrown away | used |
| SPDX and CycloneDX SBOMs, provenance and SBOM attestations, `gh attestation verify`, the attested SBOMs compared with the files | yes | yes |
| notes rendered from the changelog | yes | yes |
| the commit is green on CI, CodeQL, OSV-Scanner and Zizmor | reported, not enforced (a rehearsal branch is rarely green) | enforced |
| the tag, the release | **no** | yes |

What it leaves is the `release-<version>` artifact of the run: the exact files
and `notes.md` that a release would attach, and the notes are also in the run's
summary. Download it, install the package on a machine, read the notes. The dry
run's signatures verify against the `cubeulator-signing-key.asc` in the same
artifact, and against nothing else.

It can be run from any branch that has the workflow file, without a tag, and so
before the first release PR has merged (checked on 2026-10-14, from a scratch
branch):

```sh
gh workflow run release.yml --ref <branch> -f dry_run=true
```

To rehearse a particular version end to end, prepare it on a scratch branch
that is never merged (no pull request: a `support/` branch into `develop` is
not a valid flow), write a draft summary and caveat so the changelog check
passes, and dispatch from it:

```sh
git switch -c support/release-<version>-dry-run origin/develop
python tools/release/prepare_release.py <version>      # then write the summary and the caveat
git commit -am "Prepare <version> (scratch branch, never merged)"
git push origin support/release-<version>-dry-run
gh workflow run release.yml --ref support/release-<version>-dry-run -f dry_run=true
gh run download <run id> -n release-<version> -D dryrun      # the files and notes.md
```

With a warm vcpkg cache the run takes about an hour (the desktop builds are
the long part, then a few minutes queued for the shared runners to sign); a
cold one is hours. Check what it made with the commands in
[SECURITY.md](SECURITY.md#verifying-build-authenticity) (use the
`cubeulator-signing-key.asc` in the artifact, not the project's), then delete
the scratch branch.

Dry-run attestations are real attestations of files that were never released;
they sit in the repository's attestations list under that run and are harmless.

A PR that changes `release.yml`, `check-package.sh` or `tools/release/` runs the
tools' self-test and lets GitHub parse the file; `ci.yml` already builds and
checks the packages on every push.

## The iPhone signing material

The iPhone app goes to TestFlight, not to the release page. It is signed with the
organisation's Apple Distribution certificate and this repository's App Store
provisioning profile, and uploaded with an App Store Connect API key. What is set,
where, and how it is made are in [iPhone build: signing and releasing](ios-signing.md).
In short: the repository variable `SIGN_IOS_UPLOAD` is `true`; the organisation secrets
`IOS_DIST_CERT_P12_BASE64`, `IOS_DIST_CERT_PASSWORD`, `APPSTORE_API_KEY_ID`,
`APPSTORE_API_ISSUER_ID` and `APPSTORE_API_PRIVATE_KEY` reach this repository; and the
repository secret `IOS_PROVISIONING_PROFILE_BASE64` holds the profile.

A release (and a dry run) with the iPhone build on and one of them missing fails in the
iOS job before anything is built. A real release uploads the build; a dry run has Apple
validate it. The IPA is not a release file, so the release carries no device
identifiers.

## The signing key

The release is signed with the project's GPG key, held as two repository
secrets (the same names the other projects of the family use):

| Secret | Value |
|---|---|
| `REPO_GPG_PRIVATE_KEY` | the armored secret key |
| `REPO_GPG_PASSPHRASE` | its passphrase (omit for a key without one) |

A **real release without the key fails**, before anything is published, unless
the dispatch ticks `unsigned`: a missing or mistyped secret cannot become an
unsigned release unnoticed. A dry run reports the key as missing as a warning,
and, when it is present, proves it imports and signs.

To make one:

```sh
gpg --quick-generate-key "Cubeulator <address>" rsa4096 sign 5y
gpg --armor --export-secret-keys "Cubeulator" > cubeulator-signing-private.asc
gh secret set REPO_GPG_PRIVATE_KEY < cubeulator-signing-private.asc
gh secret set REPO_GPG_PASSPHRASE
# then delete cubeulator-signing-private.asc; keep a copy of the key somewhere safe
gpg --fingerprint "Cubeulator"     # publish this in SECURITY.md
```

or reuse the key of another project of the family, so that one fingerprint
vouches for all of them. That is a choice between convenience for people
verifying and the blast radius of one leaked secret; either works with the
workflow.

Keys expire: put the expiry in a calendar. The dry run is the check that the key
is still good before a release needs it.

## Verifying a download

Each file has two independent proofs of origin. [SECURITY.md](SECURITY.md#verifying-build-authenticity)
says how to check them. In short:

```sh
# the checksums, then the signature on them
gpg --import cubeulator-signing-key.asc
gpg --verify SHA512SUMS.asc SHA512SUMS && sha512sum -c --ignore-missing SHA512SUMS

# which repository, workflow and commit built a file
gh attestation verify Cubeulator-0.1.0-beta.1-linux-x64.deb --repo iainchesworthlabs/cubeulator
```

## What stops a release

The workflow refuses, with the reason in the log, when:

| | |
|---|---|
| The tag is not `vX.Y.Z` or `vX.Y.Z-(alpha\|beta\|rc).N` | |
| The tag is not `v` + the version `CMakeLists.txt` declares, label included | the packages are named for it, so `v0.1.0` would hold `Cubeulator-0.1.0-beta.1-*` |
| `CHANGELOG.md` has no dated `## [version]` section, or it is not the newest | the notes are rendered from it and nothing else |
| The section has no summary, an item without a bold lead-in, or (pre-release) no closing caveat | the same rules `check_release.py` states |
| `changelog.d/` still holds fragments | they are not in the section, so the notes would miss them: run `prepare_release.py` |
| A final release is not on `main` | it is tagged on `main`, after the release PR; a pre-release may be cut from a `release/*` branch to be tried first |
| The commit (for a merge commit on `main`, the `develop` tip it merged) is not green on CI, CodeQL, OSV-Scanner and Zizmor, or has a red check run | [step 1](#preparing-the-release); a dry run reports it and goes on |
| A dispatched release names a tag that exists | |
| A package fails `packaging/check-package.sh` | Qt, the executable, the icon, the identifier, the camera string, the notices: the same checks `ci.yml` makes on every push |
| A platform produces no package, or two | |
| The iPhone signing secrets are missing, the profile does not match them, or has expired | the iOS job says which; a tag push cannot leave the iPhone build out, a dispatch can untick `ios` |
| The signing key is not provisioned (a real release) | unless `unsigned` is ticked |
| A package has no attestation that verifies | |

## If a release has to be undone

A draft can be deleted from the Releases page with no trace. A published
release can be deleted too, but a tag people may have fetched should not be
moved: tag the next patch (or the next pre-release counter) instead. If the
release page was wrong and the files were right, edit the notes in place.

## Design choices, and why

- **A draft, not a published release.** The notes are rendered and the packages
  are unsigned by the OS; a person should look before the world can download
  them.
- **The notes come from the changelog**, so that they are written once, where
  the change is made, and reviewed in a PR. A release with no matching section
  fails outright rather than falling back to a list of commits.
- **The same machinery signs a rehearsal and a release.** A dry run that skipped
  the signing and attestation steps would make the release the first time they
  run.
- **The signing job and the publishing job are different jobs.** `sign` holds
  the key and the OIDC identity and can write nothing to the repository;
  `publish` can write releases and holds neither.
- **The gate reads the README.** The workflows it holds to green are the ones
  the README has a badge for, so a badge added there is held to the same rule
  without anyone remembering to edit the gate, and a badge cannot be red on the
  first release's front page without the release having said so first.
- **A cancelled run is not green.** `ci.yml` on `develop` lets a waiting push
  run be replaced by a newer one, so a commit can be passed over and never
  tested. The gate would rather say "re-run it" than treat the silence as a
  pass.
- **The build reads the vcpkg cache and never writes it**, so nothing a release
  run does can end up in what the other workflows trust.
- **Everything it uses is pinned** (see [CI & dependencies](ci.md)).
