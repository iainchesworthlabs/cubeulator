#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Iain Chesworth
# SPDX-License-Identifier: GPL-3.0-or-later
# Runs every fuzz harness (src/lib/fuzz) for a time, from its seeds, the regressions and
# the corpus kept from earlier runs.
#
#   ci/fuzz-run.sh <bin dir> <corpus dir> <seconds per harness>
#
# A crash is copied to build/fuzz-crashes/<harness>/ and the script exits 1 after the last
# harness has run, so that one crash does not hide another. -runs=0 first replays the seeds
# and the regressions alone: a regression that crashes again fails before any mutation.
set -uo pipefail

bin_dir=${1:?bin dir}
corpus_dir=${2:?corpus dir}
seconds=${3:?seconds per harness}

root=$(cd "$(dirname "$0")/.." && pwd)
fuzz_dir="$root/src/lib/fuzz"
crash_dir="$root/build/fuzz-crashes"
status=0

for harness in "$bin_dir"/fuzz_*; do
    [ -x "$harness" ] || continue
    name=$(basename "$harness")
    name=${name#fuzz_}
    corpus="$corpus_dir/$name"
    crashes="$crash_dir/$name"
    mkdir -p "$corpus" "$crashes"

    echo "::group::fuzz $name"
    if ! "$harness" "$fuzz_dir/seeds/$name" "$fuzz_dir/regressions/$name" -runs=0; then
        echo "::error::$name: a seed or a regression fails"
        status=1
    fi
    # The first directory is the one libFuzzer adds new inputs to.
    if ! "$harness" "$corpus" "$fuzz_dir/seeds/$name" "$fuzz_dir/regressions/$name" \
        -max_total_time="$seconds" -timeout=20 -rss_limit_mb=2048 \
        -artifact_prefix="$crashes/"; then
        echo "::error::$name: found a crash, a timeout or a leak (see build/fuzz-crashes/$name)"
        status=1
    fi
    echo "::endgroup::"
done

exit "$status"
