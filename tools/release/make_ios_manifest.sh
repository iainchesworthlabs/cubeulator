#!/usr/bin/env bash
# Write the over-the-air install manifest (manifest.plist) of an ad-hoc iOS build.
#
#   tools/release/make_ios_manifest.sh --out <manifest.plist> --ipa <file name>
#       --repo <owner/name> --tag <tag> --version <X.Y.Z>
#       [--bundle-id <id>] [--title <name>]
#
# Safari installs an ad-hoc app from an itms-services:// link that names a manifest
# over HTTPS (a bare .ipa link cannot be installed). The manifest names the
# bundle, its version and where the .ipa is: here a GitHub release's own asset URL,
#
#   https://github.com/<repo>/releases/download/<tag>/<ipa>
#
# which is known before the asset is uploaded. docs/ios-install.md says how the
# link is made and used.
#
# The version is the bundle's CFBundleShortVersionString (the plain X.Y.Z, with
# no pre-release label: iOS accepts none).
set -euo pipefail

out="" ipa="" repo="" tag="" version="" title="Cubeulator"
bundle_id="com.iainchesworthlabs.cubeulator"
while [ $# -gt 0 ]; do
  case "$1" in
    --out) out="${2:?}"; shift 2 ;;
    --ipa) ipa="${2:?}"; shift 2 ;;
    --repo) repo="${2:?}"; shift 2 ;;
    --tag) tag="${2:?}"; shift 2 ;;
    --version) version="${2:?}"; shift 2 ;;
    --bundle-id) bundle_id="${2:?}"; shift 2 ;;
    --title) title="${2:?}"; shift 2 ;;
    -h|--help) sed -n '2,19p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
    *) echo "make_ios_manifest.sh: unknown argument '$1'" >&2; exit 2 ;;
  esac
done
for name in out ipa repo tag version; do
  [ -n "${!name}" ] || { echo "make_ios_manifest.sh: --${name} is required" >&2; exit 2; }
done

fail() { echo "make_ios_manifest.sh: $*" >&2; exit 2; }
# What goes into a URL and an XML text node is held to a plain alphabet, so
# nothing needs escaping and nothing can be injected.
[[ "$ipa" =~ ^[A-Za-z0-9._-]+\.ipa$ ]] || fail "'$ipa' is not a plain .ipa file name"
[[ "$repo" =~ ^[A-Za-z0-9._-]+/[A-Za-z0-9._-]+$ ]] || fail "'$repo' is not owner/name"
[[ "$tag" =~ ^[A-Za-z0-9._-]+$ ]] || fail "'$tag' is not a tag name"
[[ "$version" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || fail "'$version' is not X.Y.Z"
[[ "$bundle_id" =~ ^[A-Za-z0-9.-]+$ ]] || fail "'$bundle_id' is not a bundle identifier"
[[ "$title" =~ ^[A-Za-z0-9\ ._-]+$ ]] || fail "'$title' has characters a title may not"

cat > "$out" <<PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>items</key>
  <array>
    <dict>
      <key>assets</key>
      <array>
        <dict>
          <key>kind</key>
          <string>software-package</string>
          <key>url</key>
          <string>https://github.com/${repo}/releases/download/${tag}/${ipa}</string>
        </dict>
      </array>
      <key>metadata</key>
      <dict>
        <key>bundle-identifier</key>
        <string>${bundle_id}</string>
        <key>bundle-version</key>
        <string>${version}</string>
        <key>kind</key>
        <string>software</string>
        <key>title</key>
        <string>${title}</string>
      </dict>
    </dict>
  </array>
</dict>
</plist>
PLIST
echo "wrote $out"
