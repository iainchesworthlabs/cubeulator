#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Iain Chesworth
# SPDX-License-Identifier: GPL-3.0-or-later
# The project's release notes, for the shared publish workflow (release-publish.yml, input notes-script).
#
#   render-notes.sh <dist> <output-file> <version> <tag> <signed>
#
# The workflow runs it in a checkout of the repository, after the files are signed, attested and verified, so the
# table of what was done that the notes carry is true. The notes are the changelog's section for the version, laid
# out the way every release of this project reads (tools/release/render_release_notes.py).
set -euo pipefail

if [ "$#" -ne 5 ]; then
  echo "usage: $0 <dist> <output-file> <version> <tag> <signed>" >&2
  exit 2
fi
dist="$1" output="$2" version="$3" tag="$4" signed="$5"
args=(--version "$version" --tag "$tag" --repo "${GITHUB_REPOSITORY:?GITHUB_REPOSITORY is not set}"
      --artifacts "$dist" --attested --sbom --output "$output")
if [ "$signed" = "true" ]; then
  args+=(--signed)
fi
python3 "$(dirname "$0")/render_release_notes.py" "${args[@]}"
