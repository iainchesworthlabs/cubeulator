# Security policy

## Reporting a vulnerability

Please report security vulnerabilities **privately**, not as a public issue or pull request, through
[GitHub's private vulnerability reporting](https://github.com/iainchesworthlabs/cubeulator/security/advisories/new):
it opens a private advisory that only you and the maintainer can see.

What helps most:

- what is affected: the version (**Settings > About Cubeulator**, or the name of the package), the platform, and
  whether it is the desktop app, the browser build or a release file;
- how to reproduce it, or what you found;
- what someone could do with it.

Please do not attach a camera picture that shows a person or a room.

If you cannot use the form (you have no GitHub account, say), open an issue that says only that you have a security
matter, with no details, and the maintainer will reply there to arrange a private channel.

## What counts

Anything that breaks what the app says about itself is in scope: a camera picture saved, sent or exposed without
someone switching a developer option on (see [Privacy](docs/guide/privacy.md)), a release file that does not verify
against its signature or attestation, a way to run code through a crafted camera frame, settings file or history file,
or a flaw in the build and release workflows.

## What to expect

Cubeulator is maintained by one person, so there is no guaranteed response time. A report is read and acknowledged in
the advisory; one that is confirmed is fixed on `develop` and published with the next tagged release. Say in the report
if you would like to be credited, and please allow time for a fix to ship before you publish details: the advisory is
where that is agreed.

## Supported versions

Cubeulator is pre-1.0 and every release so far is a pre-release (`0.x.0-beta.N`). Only the newest release is
supported: a fix lands on `develop` and ships in the next tagged release, and an older beta is not patched.

## Verifying a release

Every release file carries a GitHub build-provenance attestation and a GPG signature. With the
[GitHub CLI](https://cli.github.com/):

```sh
gh attestation verify Cubeulator-<version>-linux-x64.deb --repo iainchesworthlabs/cubeulator
```

The GPG key that signs `SHA512SUMS`, each package and each SBOM is `Cubeulator Release Signing Key`, RSA 4096, signing
only, with the fingerprint

```text
0CF2 A242 440A 7AF0 18A2  BFF3 03E2 D330 B299 EB46
```

The public key is committed as [`docs/signing-key.asc`](docs/signing-key.asc), and each release holds a copy. The
commands for both checks, for the SBOMs and for working offline are in
[Verifying build authenticity](docs/SECURITY.md#verifying-build-authenticity).
