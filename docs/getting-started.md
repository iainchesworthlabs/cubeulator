# Getting started

This page is for building Cubeulator from source. To use it, see the
[user guide](guide/index.md) and [Installing](guide/install.md).

## Requirements

- A C++23 compiler with `<mdspan>`: GCC 16+, Clang 23+, MSVC 14.51+, or AppleClang 17+.
- CMake 4+ and Ninja.
- Qt 6.12.x, the current LTS (prebuilt, via [aqtinstall](https://github.com/miurahr/aqtinstall))
  — only needed if you're building the desktop GUI shell (the default).
- Git (vcpkg is vendored as a submodule and bootstraps itself automatically —
  nothing else to install for it).

## Clone

```sh
git clone --recurse-submodules https://github.com/iainchesworthlabs/cubeulator
cd cubeulator
```

If you already cloned without `--recurse-submodules`:

```sh
git submodule update --init --depth 1 deps/vcpkg
```

## Sharing vcpkg between checkouts

By default every checkout has its own copy of vcpkg: the `deps/vcpkg`
submodule. That is what a fresh clone and CI use, and nothing needs setting up.
If you keep several checkouts or worktrees (or several repositories that use
vcpkg), that is a lot of copies. Point them at one shared **store** instead;
set this once per machine, in your environment:

```sh
# Windows (new terminals and Visual Studio pick it up after a restart)
setx ICL_VCPKG_STORE C:\Repos\common\vcpkg
# Linux/macOS: put this in your shell profile
export ICL_VCPKG_STORE=$HOME/.cache/icl/vcpkg
```

`ICL_VCPKG_STORE` is read by every Iain Chesworth Labs repository that carries
the same `cmake/VcpkgRoot.cmake`, so they all share the one directory.
`CUBEULATOR_VCPKG_STORE` overrides it for Cubeulator alone.

The first configure clones the fork the submodule points at into
`<store>/mirror`, and checks out the commit this checkout pins into
`<store>/<first 12 characters of the commit>`. Every checkout (of this
repository or another) that pins the same commit uses that directory; when a pin
moves (a vcpkg bump), the new commit gets a directory of its own, and nothing
that already exists is touched. That is the point of a directory per commit
rather than one shared checkout: a checkout of another branch, or a build that
is running, never has vcpkg change under it. A repository whose submodule points
at a different fork has its commit fetched into the same mirror. Directories of
old commits can be deleted when you no longer need them
(`git -C <store>/mirror worktree remove <dir>`).

Already have a full clone of your fork somewhere? Set `ICL_VCPKG_MIRROR` (or
`CUBEULATOR_VCPKG_MIRROR`) to it and the store makes its checkouts from that
instead of cloning again.

A build directory remembers the vcpkg it was first configured with, so after
the pin moves, configure it again with `cmake --fresh --preset <name>` (CMake
says so). Build artifacts are separate from all this: share the downloads,
binary cache and installed tree with `VCPKG_DOWNLOADS`,
`VCPKG_BINARY_SOURCES` and `VCPKG_INSTALLED_DIR`, as vcpkg documents.

Two other ways in, for when the store does not fit:

- `CUBEULATOR_VCPKG_ROOT` names one vcpkg checkout you manage yourself. There is
  no organisation-wide equivalent on purpose: one directory cannot be at the
  commit every repository pins. A different commit from the pin is a warning,
  since a baseline newer than the scripts it is built with can fail.
- `-DCMAKE_TOOLCHAIN_FILE=...` (or an IDE that sets it) is used as it is.

Avoid pointing `deps/vcpkg` itself at a shared directory with a junction or
symlink: deleting that checkout (or a worktree of it) can then empty the shared
copy for everyone, and a dead link makes git report only that it "cannot find
the current revision". The configure now says which link is dead.

## First build

Install Qt locally with the pinned helper script, then configure and build:

```sh
# Windows
pwsh ci/install-qt.ps1
cmake --preset windows-msvc-debug -DCMAKE_PREFIX_PATH=C:/Qt/6.12.0/msvc2022_64
cmake --build --preset windows-msvc-debug
ctest --preset windows-msvc-debug

# Linux/macOS
./ci/install-qt.sh
cmake --preset linux-gcc-debug -DCMAKE_PREFIX_PATH=$HOME/Qt/6.12.0/gcc_64
cmake --build --preset linux-gcc-debug
ctest --preset linux-gcc-debug
```

`ctest --preset <preset>` leaves out the system tests, which draw the real app
in a window; `ctest --preset <preset>-system` runs them
([testing.md](testing.md#system-tests)).

Just want the core library, no Qt/GUI?

```sh
cmake --preset linux-gcc-debug -DCUBEULATOR_BUILD_APP=OFF
cmake --build --preset linux-gcc-debug
ctest --preset linux-gcc-debug
```

See [Building & packaging](building.md) for every preset (including Android,
which builds the same Qt/QML app, iOS, which is currently build-only, and
[WebAssembly](building.md#webassembly-browser), which builds it for a browser)
and packaging options.

Just want to try it? [Open it in your browser](web.md).
