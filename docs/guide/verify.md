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

Each release also holds `SHA512SUMS`, its detached signature `SHA512SUMS.asc`, a signature beside every package, and the
public key `cubeulator-signing-key.asc`:

```sh
gpg --import cubeulator-signing-key.asc
gpg --verify SHA512SUMS.asc SHA512SUMS && sha512sum -c --ignore-missing SHA512SUMS
# or one file:
gpg --verify Cubeulator-<version>-linux-x64.deb.asc Cubeulator-<version>-linux-x64.deb
```

Compare the key's fingerprint with this one before you trust it. A key that arrives with the release proves nothing about
who made the release if it is the only place the fingerprint appears.

| | |
|---|---|
| Key | `Cubeulator Release Signing Key`, RSA 4096, signing only |
| Fingerprint | `0CF2 A242 440A 7AF0 18A2  BFF3 03E2 D330 B299 EB46` |
| Created | 2026-10-14 |
| Expires | 2031-10-13 |

```sh
gpg --show-keys --fingerprint cubeulator-signing-key.asc
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
