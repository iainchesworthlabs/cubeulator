# Contributing

Thanks for taking a look at Cubeulator! This guide is short — the goal is
just to save you a round-trip on the handful of things that aren't obvious
from the code.

## Before you start

For anything more than a typo fix, it's worth
[opening an issue](https://github.com/iainchesworthlabs/cubeulator/issues) first
to say what you're thinking — it avoids duplicated effort and lets us agree
on the approach before you sink time into it. A security problem is the
exception: report it privately, as the [security policy](SECURITY.md) says,
not in an issue. Everyone taking part is held to the
[code of conduct](CODE_OF_CONDUCT.md).

## Branching model

One shared trunk, and a branch that holds releases:

- **`develop`** is the trunk and the default branch: target your pull request
  here. It is protected: nothing is pushed to it directly, and pull requests
  land through a [merge queue](branch-protection.md) rather than by hand.
- Pull requests are squashed into `develop`, one commit each, so its history is
  linear.
- **`main`** holds published releases only. A release is a `v*` tag on a
  `develop` commit; when the release workflow succeeds it fast-forwards `main`
  to that commit. Nobody opens a pull request into `main`, and there is no
  promotion or sync-back, because `main` is always an ancestor of `develop`.
- A fix for an old release goes on a `release/x.y` branch cut from its tag,
  only when `develop` has moved on.
- Work branches start with `feature/`, `bugfix/`, `docs/`, `chore/` or
  `agent/` (`agent/` is for an automated contributor), followed by letters,
  digits, `.`, `_` or `-`. `release/`, `hotfix/` and `support/` are also
  accepted, for the maintainer. The `branch-names` ruleset and the `Policy`
  check enforce this.
- **`spike/<name>`** is for finding something out. It is pushed, tried (CI legs
  are dispatched at it by hand) and shown, and it is **never merged**. What it found goes in
  [a page under `docs/investigations/`](investigations/README.md); a spike left
  alone for 30 days is tagged `archive/spike-<name>` and its branch deleted.

Commits carry a `Signed-off-by` line (`git commit -s`), which certifies the
[Developer Certificate of Origin](https://developercertificate.org/). It is the
only trailer the `Policy` check allows. Run `python3 tools/ci/precheck.py`
before you push to see the checks it makes on your commits. Automated
contributors also read
[`AGENTS.md`](https://github.com/iainchesworthlabs/cubeulator/blob/develop/AGENTS.md),
which lists, for each kind of change, what else to update.

## Set up your build

See [Getting started](getting-started.md) for toolchain requirements and
your first build. `-DCUBEULATOR_BUILD_APP=OFF` skips Qt entirely if you're
only touching `cube::lib` and want a faster inner loop.

## Tests are not optional

This project is TDD by convention: new behaviour comes with a test, and
existing behaviour that changes gets its test updated in the same PR. See
[Testing & coverage](testing.md) for where each kind of test lives.

CI enforces an **80% line and branch coverage** floor
(`Coverage (Linux, GCC)`) — a PR that drops coverage below that will fail
that check.

## Code style

- Formatting follows the repo's
  [`.clang-format`](https://github.com/iainchesworthlabs/cubeulator/blob/develop/.clang-format) —
  run `clang-format` before committing, or configure your editor to do it
  on save. The version is **23.1.3**: CI installs that wheel from
  [`ci/requirements-lint.txt`](https://github.com/iainchesworthlabs/cubeulator/blob/develop/ci/requirements-lint.txt)
  and checks with it (`pip install -r ci/requirements-lint.txt` gives you the
  same one). A 21, 22 or 23 formats the tree the same way, so any is fine
  locally.
- [`.clang-tidy`](https://github.com/iainchesworthlabs/cubeulator/blob/develop/.clang-tidy)
  is what the `clang-tidy` CI check runs, over a Clang build of everything
  but the Qt app; a build on your machine does not run it. Compiler warnings
  are treated as errors in every build, so fix them rather than silencing
  them where you reasonably can.
- Names follow what the code already does, and `.clang-tidy` enforces it
  (`readability-identifier-naming`): types are `PascalCase`; functions,
  methods, locals, parameters, namespaces and struct fields are `lower_case`;
  private and protected members end in an underscore (`name_`); constants are
  `kPascal`; macros are `UPPER_CASE`; files are `snake_case.cpp`. The
  Qt-facing code under `src/app` (properties, signals, slots and invokable
  functions that QML sees) is `camelCase`, set by `src/app/.clang-tidy`, and
  the QML files and the classes registered with QML are `PascalCase`.
  Enumerators are not checked, because both `lower_case` and `PascalCase` are
  in use; follow the enum you are adding to.
- [`.editorconfig`](https://github.com/iainchesworthlabs/cubeulator/blob/develop/.editorconfig)
  carries the indent and line-ending rules for the files `clang-format` does
  not read. A commit that only reformats goes in
  [`.git-blame-ignore-revs`](https://github.com/iainchesworthlabs/cubeulator/blob/develop/.git-blame-ignore-revs)
  (`git config blame.ignoreRevsFile .git-blame-ignore-revs`).
- Modern C++23 is encouraged over older idioms — see
  [Architecture](architecture.md) for the patterns already in use
  (`Result<T>` over `std::expected`, for one) and follow their lead.

## Opening a pull request

1. Push your work branch (`feature/*`, `bugfix/*`, ...) and open a PR against
   `develop`.
2. Make sure the required checks are green — see
   [Branch protection and the merge queue](branch-protection.md) for which
   checks are required on `develop` today (the set grows as the build and
   analysis jobs go green).
3. When it's ready, put it in the queue: `gh pr merge <n>` (or **Merge when
   ready** on the PR; the queue chooses the squash strategy). GitHub merges it onto the current
   `develop` tip, re-runs the required checks on that tree, and lands it —
   you don't rebase or re-merge `develop` yourself unless the PR conflicts.
   Your remote branch is deleted once it merges.
4. If a person using the app would notice the change, add a **changelog
   fragment**: one small file, `changelog.d/<slug>.<type>.md`, holding one item
   (`added`, `changed`, `fixed`, ...). It is a file of its own so that pull
   requests open at the same time do not conflict over `CHANGELOG.md`. If the
   change is not one a reader of the changelog would want to hear about (a
   refactor, a test, a build detail), label the PR `no-changelog`. How to write
   one is in
   [`changelog.d/README.md`](https://github.com/iainchesworthlabs/cubeulator/blob/develop/changelog.d/README.md);
   the `Policy` check says when one is missing.
5. Keep PRs focused — a bug fix doesn't need to carry an unrelated
   refactor along with it.

That's it. If anything here is unclear or out of date, flagging it in an
issue is itself a welcome contribution.
