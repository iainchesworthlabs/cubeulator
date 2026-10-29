# Branch protection and the merge queue

GitHub branch and repository settings can't be expressed as a workflow file. They are applied by an
administrator, from the shared `admin/apply.sh` in `iainchesworthlabs/icl-common-infra`, and this page
records what is configured and why, so the settings can be rebuilt or audited from the page alone.
The decision to adopt the shared branch model is in [Aligning cubeulator with the ICL standard](migration.md).

## The flow

```text
feature/* bugfix/* chore/* docs/* ...  ──PR──▶  merge queue  ──squash──▶  develop
                                                                             │
                                          release workflow, after a release  ▼
                                                                    fast-forward main
```

- `develop` is the default branch and the trunk. Every change reaches it through a pull request and
  the **merge queue**. Nobody pushes to `develop`, and nobody merges by hand.
- Pull requests are **squashed**: one commit per pull request, a linear history. The commit title and
  message come from the pull request title and body.
- `main` holds published releases only. It is never the target of a pull request. After a release is
  published the release bot fast-forwards `main` to the released commit of `develop`, so `main` is
  always an ancestor of `develop`. There is no back-merge.
- Tags `v*` are created by the release workflow and the organisation owner only.
- **`spike/*`** is for investigations ([Investigations](investigations/README.md)): the branch is pushed,
  CI legs are dispatched at it by hand, and it is never merged. It is listed in
  `.github/branch-prefixes.txt`, so the `branch-names` ruleset allows it. `spike-sweep.yml` tags and
  deletes a spike that has been untouched for 30 days.
- Head branches are deleted when their pull request merges.

## Rulesets

| Ruleset | Applies to | What it does |
| --- | --- | --- |
| `branch-names` | every branch except `main`, `develop`, `gh-pages`, `quality-history` and the allowed prefixes | A branch name must start with `feature/`, `bugfix/`, `release/`, `hotfix/` or `support/`, or with a prefix in the exclusion list below. Dependabot (`dependabot/`), the merge queue (`gh-readonly-queue/`), `docs/`, `chore/`, `agent/` and `spike/` are allowed |
| `develop-protection` | `develop` | A pull request is required (0 approvals, conversations resolved, stale approvals dismissed), squash only, linear history, required checks `CI Status` and `Scan dependency diff`, no deletion, no force push. There is no bypass actor |
| `merge-queue-develop` | `develop` | The merge queue: `SQUASH`, `ALLGREEN`, up to 4 entries built and 5 merged, check timeout 180 minutes |
| `main-release-only` | `main` | No update, deletion or force push, except by the release bot (the only bypass actor) |
| `tags` | `v*` | No creation, update or deletion, except by the organisation owner and the release bot |

The approval count is 0 because this is a solo-maintainer repository and GitHub doesn't count an
author's own approval, so a required approval could only be met by bypassing it.

Which prefixes the `branch-names` ruleset exempts comes from the shared `admin/apply.sh` plus the
project's own `.github/branch-prefixes.txt`. The same file is read by the `Policy` check, so the
check and the ruleset agree.

## Required checks

Two checks are required on `develop`:

- **`CI Status`** is the one job in `ci.yml` that summarises the rest. It needs every other job in the
  workflow, lists the result it expects from each one, and fails if a job that should have run was
  skipped. A failed or cancelled job fails it as well. A new job goes into its `needs` list and, when
  the planner decides whether it runs, its `expected` map ([CI](ci.md)). That is why a
  documentation-only change, which skips the builds, still reports a result.
- **`Scan dependency diff`** (`dependency-review.yml`) fails a pull request that adds a vulnerable or
  disallowed-licence dependency.

`Policy` (branch name, commit and pull request text, the preprocessor check, action pinning, the
changelog fragment, the vcpkg baseline in the fork) is a job in `ci.yml`, so it is part of
`CI Status`.

## The merge queue

A pull request enters the queue once its own checks pass, or earlier with auto-merge
(`gh pr merge --merge <n>`; the queue picks the squash strategy). GitHub builds a temporary
`gh-readonly-queue/develop/pr-<n>-<sha>` branch, the pull request merged onto the current queue tip,
re-runs the required checks on that tree and merges it only if they pass. A failed entry leaves the
queue and the entries behind it are rebuilt without it.

Two rules keep the queue honest:

- **Every workflow that produces a required check must also trigger on `merge_group`.** GitHub only
  runs workflows that opt in on the queue's temporary ref, so a required check from a workflow
  without that trigger never reports, and the entry waits until the timeout. `ci.yml` and
  `dependency-review.yml` carry it.
- **A workflow that produces no required check must not carry `merge_group`**, or every queue entry
  runs it for nothing. A workflow that mixes required and other jobs (`ci.yml`) carries the trigger
  and skips the rest itself. Which jobs run on a pull request, in the queue and after the merge is in
  [CI](ci.md).

## Repository settings

- Default branch `develop`.
- Squash merging only; merge commits and rebase merging off.
- Squash commit title from the pull request title, message from its body.
- Allow auto-merge: on. Automatically delete head branches: on. Allow update branch: on.
- Wiki, projects and discussions off.
- The description, homepage and topics are kept in `.github/repo-settings.json`.

## Dependabot pull requests

Dependabot opens its pull requests against `develop` from `dependabot/*` branches (its prefix is not
configurable, so the `branch-names` ruleset and the `Policy` check allow it).
`dependabot-auto-merge.yml` enables auto-merge on the low-risk ones: minor and patch updates of
GitHub Actions and the pip tooling. They go through the queue like any other pull request. Nothing
is bypassed: the `develop-protection` and `merge-queue-develop` rules apply unchanged. Major updates
and the vcpkg baseline stay manual. The policy and the one place to widen it are in the workflow.

## Other scanners (visible only)

`osv-scanner.yml`, `zizmor.yml` and `scorecard.yml` upload SARIF to **Security → Code scanning** but
don't fail pull request checks. Triage their alerts there rather than through a required status
check. `scorecard.yml` runs on pushes to `main` and weekly, not on pull requests.

## Rebuilding the settings

From a checkout of this repository on `develop` and one of `icl-common-infra`:

```sh
RELEASE_BOT_APP_ID=5426061 OWNER_USER=iainchesworth \
  admin/apply.sh --repo iainchesworthlabs/cubeulator --config-dir <this checkout>/.github \
  --only settings,rulesets            # a dry run; add --apply to make the change
```

`apply.sh` creates a ruleset that is missing and updates one it finds by name. It never deletes a
ruleset, so one that is no longer wanted is removed by hand. Rulesets are not edited from a workflow
or by an agent without the maintainer's say.
