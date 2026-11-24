# Verifying a download

You do not need to do this to use Cubeulator. It is for when you want to be sure that a file is the one this project built,
and not something with the same name.

Every released package can be traced to the commit and workflow run that built it. Two independent checks are provided.
Neither is something your operating system looks at, so Windows SmartScreen and macOS Gatekeeper still ask the first time:
the installers are not signed with a Windows or Apple certificate yet ([Installing](install.md#first-run)).

## Build provenance, with no key

Each package carries a
[GitHub build-provenance attestation](https://docs.github.com/actions/security-guides/using-artifact-attestations): a
statement signed through Sigstore with the release workflow's short-lived identity, recording the repository, commit and
workflow that built the file. With the [GitHub CLI](https://cli.github.com/):

```sh
gh attestation verify Cubeulator-<version>-linux-x64.deb --repo iainchesworthlabs/cubeulator
```

A pass prints the repository, the commit and the workflow. Reject a file that does not verify.

## The author's GPG signature

Each release also holds `SHA512SUMS`, its detached signature `SHA512SUMS.asc`, and a signature beside every package and
each SBOM. The public key is not in the release: it is `docs/signing-key.asc` in the repository, so that a release cannot
vouch for itself. Fetch it from there:

```sh
curl -fsSLO https://raw.githubusercontent.com/iainchesworthlabs/cubeulator/develop/docs/signing-key.asc
gpg --import signing-key.asc
gpg --verify SHA512SUMS.asc SHA512SUMS && sha512sum -c --ignore-missing SHA512SUMS
# or one file:
gpg --verify Cubeulator-<version>-linux-x64.deb.asc Cubeulator-<version>-linux-x64.deb
```

Compare the key's fingerprint with this one before you trust it. A key fetched from the same place as the download proves
nothing about who made the release if that is the only place the fingerprint appears.

| | |
|---|---|
| Key | `Iain Chesworth Labs Releases`, ed25519, signing only |
| Fingerprint | `FD13 CB8C AC83 1664 8CD9  74D0 8149 254C 4189 4DBE` |
| Created | 2026-10-22 |
| Expires | 2028-10-21 |

This is the key of every release from 1.0.0-rc.1 on, and the one in `docs/signing-key.asc`. The two releases before it,
0.1.0-beta.1 and 0.2.0-beta.1, were signed with an earlier key, `Cubeulator Release Signing Key` (RSA 4096, created
2026-10-14, expires 2031-10-13), whose fingerprint is `0CF2 A242 440A 7AF0 18A2  BFF3 03E2 D330 B299 EB46`. Those two releases carried
their key as a file named `cubeulator-signing-key.asc` beside the packages; compare that key with the fingerprint for
that release. (0.2.0-beta.2 was tagged and never published.)

```sh
gpg --show-keys --fingerprint signing-key.asc
```

`gpg --verify` also prints a `Primary key fingerprint` and warns that the key is not certified by anyone you trust. That
warning is expected: the fingerprint above is what to compare.

The two checks answer different questions. Provenance ties the bytes to *this repository and build*; the signature ties them
to *the author's key*.

## The bills of materials

Each release carries `Cubeulator-<version>.spdx.json` (SPDX 2.3) and `Cubeulator-<version>.cdx.json` (CycloneDX 1.6): for
each package, every component that ships inside it, with its version and licence. They are signed and attested the same way.
The commands, and how to compare an attested bill of materials with the file you downloaded, are in the
[security policy](https://github.com/iainchesworthlabs/cubeulator/blob/develop/docs/SECURITY.md#the-sboms) on GitHub.

## Next

Back to [Installing](install.md).
