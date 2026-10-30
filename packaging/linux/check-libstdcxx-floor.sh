#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Iain Chesworth
# SPDX-License-Identifier: GPL-3.0-or-later
# Fails when a program or library in a Linux package needs a newer libstdc++ than the oldest system the package is for.
#
#   check-libstdcxx-floor.sh <package folder> [highest GLIBCXX] [highest CXXABI]
#
# The shared check-glibc-floor action covers glibc. libstdc++ is the other half of the floor: the package depends on the
# system's libstdc++6, and a build with a newer GCC can use a symbol version that the system's does not have. The defaults
# are Ubuntu 24.04 and Debian 13, which carry GCC 14's libstdc++ (GLIBCXX_3.4.33, CXXABI_1.3.15).
set -euo pipefail

folder="${1:?usage: check-libstdcxx-floor.sh <package folder> [highest GLIBCXX] [highest CXXABI]}"
allowed_glibcxx="${2:-3.4.33}"
allowed_cxxabi="${3:-1.3.15}"

highest() { # <prefix> <file>: the highest version of a symbol family the file needs
    objdump -T "$2" 2>/dev/null | grep -o "$1_[0-9][0-9.]*" | sed "s/^$1_//" | sort -uV | tail -n 1 || true
}

failed=0
checked=0
while IFS= read -r -d '' file; do
    head -c 4 "$file" | grep -q 'ELF' || continue
    checked=$((checked + 1))
    for family in "GLIBCXX:$allowed_glibcxx" "CXXABI:$allowed_cxxabi"; do
        prefix="${family%%:*}"
        allowed="${family#*:}"
        needed="$(highest "$prefix" "$file")"
        [ -n "$needed" ] || continue
        newest="$(printf '%s\n%s\n' "$allowed" "$needed" | sort -V | tail -n 1)"
        if [ "$newest" != "$allowed" ]; then
            echo "error: $file needs ${prefix}_$needed; the package is for systems with ${prefix}_$allowed" >&2
            failed=1
        fi
    done
done < <(find "$folder" -type f -print0)

echo "checked $checked files against GLIBCXX_$allowed_glibcxx and CXXABI_$allowed_cxxabi"
test "$checked" -gt 0
exit "$failed"
