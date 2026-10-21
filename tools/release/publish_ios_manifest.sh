#!/usr/bin/env bash
# Put the newest published release's iPhone install manifest where the install
# page can name it.
#
#   tools/release/publish_ios_manifest.sh --repo <owner/name> --out <dir>
#
# An ad-hoc iOS app is installed from a Safari link `itms-services://?action=
# download-manifest&url=<https url of a manifest.plist>`. iOS will not follow a
# redirect to the manifest, and a release asset's download URL is one, so the
# manifest has to be served from a fixed address: the documentation site. The
# docs workflow runs this while it builds the site and the site then carries
# <out>/manifest.plist (https://<pages>/ios/manifest.plist) and <out>/latest.txt
# (the tag it belongs to). No .ipa is copied: the manifest names the .ipa's
# release-asset URL, and an app is downloaded from there.
#
# It takes the newest release that is not a draft (a pre-release counts: every
# release so far is one, and GitHub's own "latest" skips them). A repository with
# no release, or a release with no iPhone manifest, is not an error: the site is
# built without the file and the install page says there is no build yet.
# What is wrong is a manifest that does not belong to its release: that stops.
set -euo pipefail

repo="" out=""
while [ $# -gt 0 ]; do
  case "$1" in
    --repo) repo="${2:?}"; shift 2 ;;
    --out) out="${2:?}"; shift 2 ;;
    -h|--help) sed -n '2,22p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
    *) echo "publish_ios_manifest.sh: unknown argument '$1'" >&2; exit 2 ;;
  esac
done
[ -n "$repo" ] && [ -n "$out" ] || { echo "usage: publish_ios_manifest.sh --repo <owner/name> --out <dir>" >&2; exit 2; }

tag="$(gh release list --repo "$repo" --exclude-drafts --limit 1 --json tagName --jq '.[0].tagName // empty')"
if [ -z "$tag" ]; then
  echo "No release yet: the site has no iPhone manifest."
  exit 0
fi

work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
if ! gh release download "$tag" --repo "$repo" --pattern 'Cubeulator-*-ios-manifest.plist' --dir "$work" 2>/dev/null \
   || [ -z "$(ls -A "$work")" ]; then
  echo "$tag has no iPhone manifest: the site has none."
  exit 0
fi

count="$(find "$work" -maxdepth 1 -name '*.plist' | wc -l | tr -d ' ')"
[ "$count" -eq 1 ] || { echo "::error::$tag has $count iPhone manifests, expected one" >&2; exit 1; }
manifest="$(find "$work" -maxdepth 1 -name '*.plist')"

# A property list that parses, and that names this release's .ipa: a manifest
# from another release would install the wrong build.
# python3 where it runs; some Windows shells have only a stub of that name.
py=python3
"$py" -c '' 2>/dev/null || py=python
"$py" - "$manifest" "$repo" "$tag" <<'PY'
import plistlib, sys
path, repo, tag = sys.argv[1:4]
with open(path, "rb") as handle:
    data = plistlib.load(handle)
urls = [
    asset["url"]
    for item in data["items"]
    for asset in item["assets"]
    if asset.get("kind") == "software-package"
]
want = f"https://github.com/{repo}/releases/download/{tag}/"
if len(urls) != 1 or not urls[0].startswith(want) or not urls[0].endswith(".ipa"):
    sys.exit(f"the manifest names {urls}, not an .ipa under {want}")
PY

mkdir -p "$out"
cp "$manifest" "$out/manifest.plist"
printf '%s\n' "$tag" > "$out/latest.txt"
echo "Published the iPhone manifest of $tag to $out/manifest.plist."
