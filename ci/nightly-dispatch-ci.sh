#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Iain Chesworth
# SPDX-License-Identifier: GPL-3.0-or-later
# Runs ci.yml by hand on a ref, with the legs named, and waits for it to end.
# Used by nightly.yml, which wants the whole of CI plus the two wall-time legs
# that a pull request leaves out, against the tip of develop.
#
#   GH_TOKEN=... REPO=owner/name REF=develop LEGS=linux,macos,... \
#       ci/nightly-dispatch-ci.sh
#
# Prints "run_id=<id>" and "run_url=<url>" on stdout (and appends them to
# $GITHUB_OUTPUT when that is set). Exits 0 whenever the dispatched run came to
# an end, whatever its conclusion: judging the jobs is nightly-report.sh's work,
# because a job that CI marks continue-on-error ends "success" on the run even
# when it failed. Exits non-zero only when the run could not be started, found
# or waited for.
#
# A workflow_dispatch made with the workflow's own GITHUB_TOKEN does start a
# run (it is one of the two events GitHub exempts from the rule that a token's
# events start nothing). The run is found by what it is, not by an id: gh
# prints none, so the newest dispatch on the ref by github-actions[bot] after
# the moment of dispatch is taken.
set -euo pipefail

: "${GH_TOKEN:?}" "${REPO:?}" "${REF:?}" "${LEGS:?}"
TIMEOUT_MINUTES="${TIMEOUT_MINUTES:-330}"

started="$(date -u +%Y-%m-%dT%H:%M:%SZ)"
gh workflow run ci.yml --repo "$REPO" --ref "$REF" -f "legs=$LEGS"

run_id=""
for _ in $(seq 1 30); do
    sleep 10
    run_id="$(gh api -X GET "repos/${REPO}/actions/workflows/ci.yml/runs" \
        -f event=workflow_dispatch -f branch="$REF" -f "created=>=${started}" -f per_page=20 \
        --jq '[.workflow_runs[] | select(.triggering_actor.login == "github-actions[bot]")]
              | sort_by(.created_at) | last | .id // empty')"
    [ -n "$run_id" ] && break
done
if [ -z "$run_id" ]; then
    echo "::error::the dispatched CI run did not appear within five minutes"
    exit 1
fi

run_url="https://github.com/${REPO}/actions/runs/${run_id}"
echo "dispatched ci.yml on ${REF} with legs=${LEGS}: ${run_url}"
{
    echo "run_id=${run_id}"
    echo "run_url=${run_url}"
} | tee -a "${GITHUB_OUTPUT:-/dev/null}"

# Poll rather than `gh run watch`: a minute between calls is plenty for a run
# of an hour and a half, and a dropped connection costs one poll, not the wait.
deadline=$(($(date +%s) + TIMEOUT_MINUTES * 60))
while :; do
    status="$(gh api "repos/${REPO}/actions/runs/${run_id}" --jq .status)"
    [ "$status" = completed ] && break
    if [ "$(date +%s)" -ge "$deadline" ]; then
        echo "::error::the dispatched CI run ${run_url} was still ${status} after ${TIMEOUT_MINUTES} minutes"
        exit 1
    fi
    sleep 60
done
echo "the dispatched CI run ended: $(gh api "repos/${REPO}/actions/runs/${run_id}" --jq .conclusion)"
