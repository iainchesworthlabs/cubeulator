#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Iain Chesworth
# SPDX-License-Identifier: GPL-3.0-or-later
# Check that an iOS application (a built .app, or an exported .ipa) is complete.
#
#   packaging/ios/verify_ipa.sh <Cubeulator.app | package.ipa> [--version <x.y.z>] [--signed [--store]]
#
# What it looks for:
#
#   * Info.plist is valid, names the identifier, the executable, the version, the
#     icon set and the camera usage text (a program that asks for the camera
#     without it is ended by iOS), and says it is an iPhone program;
#   * with --store: it was built with the iOS 26 SDK or later, which App Store Connect
#     requires of an upload;
#   * Info.plist has a build number and declares that the app uses no encryption beyond
#     the system's (ITSAppUsesNonExemptEncryption), as TestFlight asks;
#   * the compiled asset catalogue (the icon) is in the bundle;
#   * the executable is for arm64 and links nothing outside the system: Qt is
#     static on iOS, so a link to a Qt framework or to a path on the build
#     machine means the wrong kit was used;
#   * with --signed: the code signature verifies, the profile is embedded, it is an
#     ad-hoc one (it names devices and does not allow a debugger to attach) and it
#     belongs to this bundle identifier. With --store it must be an App Store one
#     instead: no devices, and no debugger.
#
# macOS only (plutil, otool, codesign, security).
set -euo pipefail

target=""
version=""
signed=false
store=false
while [ $# -gt 0 ]; do
  case "$1" in
    --version) version="${2:?}"; shift 2 ;;
    --signed) signed=true; shift ;;
    --store) store=true; shift ;;
    -h|--help) sed -n '2,24p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
    -*) echo "verify_ipa.sh: unknown argument '$1'" >&2; exit 2 ;;
    *) target="$1"; shift ;;
  esac
done
[ -e "$target" ] || { echo "usage: verify_ipa.sh <app|ipa> [--version v] [--signed [--store]]" >&2; exit 2; }
fail() { echo "FAILED: $*" >&2; exit 1; }

bundle_id="com.iainchesworthlabs.cubeulator"
executable="Cubeulator"

work=""
cleanup() { if [ -n "$work" ]; then rm -rf "$work"; fi; }
trap cleanup EXIT

app="$target"
case "$target" in
  *.ipa)
    work="$(mktemp -d)"
    unzip -q "$target" -d "$work"
    app="$(find "$work/Payload" -maxdepth 1 -name '*.app' -type d | head -n 1)"
    [ -n "$app" ] || fail "no Payload/*.app in $target"
    ;;
esac
echo "==> $app"

plist="$app/Info.plist"
[ -f "$plist" ] || fail "no Info.plist (an iOS bundle is flat: it is beside the executable)"
plutil -lint "$plist" >/dev/null || fail "Info.plist is not a valid property list"
key() { /usr/libexec/PlistBuddy -c "Print :$1" "$plist" 2>/dev/null || true; }

[ "$(key CFBundleIdentifier)" = "$bundle_id" ] || fail "CFBundleIdentifier is '$(key CFBundleIdentifier)'"
[ "$(key CFBundleExecutable)" = "$executable" ] || fail "CFBundleExecutable is '$(key CFBundleExecutable)'"
[ -n "$(key NSCameraUsageDescription)" ] || fail "Info.plist has no NSCameraUsageDescription"
[ "$(key CFBundleIconName)" = "AppIcon" ] || fail "CFBundleIconName is '$(key CFBundleIconName)'"
[ -n "$(key UILaunchScreen)" ] || fail "Info.plist has no UILaunchScreen (iOS would run the program in a legacy window)"
[ "$(key DTPlatformName)" = "iphoneos" ] || fail "DTPlatformName is '$(key DTPlatformName)'; this is not a device build"
[ -n "$(key MinimumOSVersion)" ] || fail "Info.plist has no MinimumOSVersion"
if $store; then
  sdk="$(key DTPlatformVersion)"
  case "${sdk%%.*}" in
    ''|*[!0-9]*) fail "Info.plist has no DTPlatformVersion, so the SDK the app was built with is unknown" ;;
  esac
  [ "${sdk%%.*}" -ge 26 ] || fail "built with the iOS $sdk SDK; App Store Connect refuses anything older than iOS 26 (build with Xcode 26)"
fi
[ -n "$(key CFBundleVersion)" ] || fail "Info.plist has no CFBundleVersion (the build number)"
[ "$(key ITSAppUsesNonExemptEncryption)" = "false" ] || fail "ITSAppUsesNonExemptEncryption is '$(key ITSAppUsesNonExemptEncryption)', expected false (TestFlight asks about export compliance otherwise)"
if [ -n "$version" ]; then
  [ "$(key CFBundleShortVersionString)" = "$version" ] || fail "CFBundleShortVersionString is '$(key CFBundleShortVersionString)', expected $version"
fi
echo "    Info.plist: identifier, executable, version, icon, camera text, launch screen, iOS $(key MinimumOSVersion) or later"

for required in "$app/$executable" "$app/Assets.car"; do
  [ -e "$required" ] || fail "missing ${required#"$app"/}"
done
ls -d "$app"/*.lproj > /dev/null 2>&1 || fail "no <language>.lproj folder: the camera text is not translated"
echo "    executable, asset catalogue, translations"

archs="$(lipo -archs "$app/$executable" 2>/dev/null || true)"
[ "$archs" = "arm64" ] || fail "the executable is for '$archs', expected arm64"
bad="$(otool -L "$app/$executable" | tail -n +2 | awk '{print $1}' | grep -Ev '^(/usr/lib/|/System/Library/)' || true)"
[ -z "$bad" ] || fail "the executable links outside the system: $bad"
echo "    executable: arm64, links only the system"

if $signed; then
  codesign --verify --strict --verbose=2 "$app" || fail "the code signature does not verify"
  profile="$app/embedded.mobileprovision"
  [ -f "$profile" ] || fail "no embedded.mobileprovision: the build was not signed with a profile"
  decoded="$(mktemp)"
  security cms -D -i "$profile" > "$decoded" 2>/dev/null || fail "cannot decode embedded.mobileprovision"
  pkey() { /usr/libexec/PlistBuddy -c "Print :$1" "$decoded" 2>/dev/null || true; }
  case "$(pkey Entitlements:application-identifier)" in
    *."$bundle_id") ;;
    *) fail "the profile is for '$(pkey Entitlements:application-identifier)', not $bundle_id" ;;
  esac
  devices="$(/usr/libexec/PlistBuddy -c "Print :ProvisionedDevices" "$decoded" 2>/dev/null | grep -c '^    ' || true)"
  if $store; then
    [ "$devices" -eq 0 ] || fail "the profile names $devices device(s): it is not an App Store profile"
  else
    [ "$devices" -gt 0 ] || fail "the profile names no devices: it is not an ad-hoc profile"
  fi
  [ "$(pkey Entitlements:get-task-allow)" = "false" ] || fail "the profile allows a debugger to attach (get-task-allow): it is a development profile"
  kind="ad-hoc"; $store && kind="App Store"
  echo "    signed: the signature verifies; $kind profile '$(pkey Name)' ($devices device(s)), expires $(pkey ExpirationDate)"
  rm -f "$decoded"
fi
echo "OK: $target"
