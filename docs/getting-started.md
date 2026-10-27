# Getting started

This page is for building Cubeulator from source. To use it, see the
[user guide](guide/index.md) and [Installing](guide/install.md).

## Requirements

- A C++23 compiler with `<mdspan>`: GCC 16+, Clang 23+, MSVC 14.51+, or AppleClang 17+.
- CMake 4+ and Ninja.
- Qt 6.12.x, the current LTS (prebuilt, via [aqtinstall](https://github.com/miurahr/aqtinstall))
  — only needed if you're building the desktop GUI shell (the default).
- Git, and a place for vcpkg (the labs' fork, pinned by `vcpkg.json`; see
  [vcpkg](#vcpkg) below).

## Clone

```sh
git clone https://github.com/iainchesworthlabs/cubeulator
cd cubeulator
```

## vcpkg

vcpkg supplies fmt, OpenCV and Catch2. `vcpkg.json` pins it with a
`builtin-baseline`, a commit of the labs' fork `iainchesworthlabs/vcpkg`, and
`cmake/icl/IclVcpkgRoot.cmake` picks the checkout for a configure, first match
wins:

1. A `CMAKE_TOOLCHAIN_FILE` you pass (or an IDE sets) is used as it is.
2. `CUBEULATOR_VCPKG_ROOT` names one vcpkg checkout you manage yourself. A
   different commit from the baseline is a warning, since a baseline newer than
   the scripts it is built with can fail.
3. A **store**, from `CUBEULATOR_VCPKG_STORE` or, for every Iain Chesworth Labs
   repository on the machine, `ICL_VCPKG_STORE`.
4. `VCPKG_ROOT`, which is what CI sets (icl-common-infra's `setup-vcpkg` checks
   out the baseline and exports it).

With none of them a configure that needs vcpkg stops and lists these options.
Set the store once per machine, in your environment:

```sh
# Windows (new terminals and Visual Studio pick it up after a restart)
setx ICL_VCPKG_STORE C:\Repos\common\vcpkg
# Linux/macOS: put this in your shell profile
export ICL_VCPKG_STORE=$HOME/.cache/icl/vcpkg
```

The first configure clones the fork into `<store>/mirror` and checks out the
baseline commit into `<store>/<first 12 characters of the commit>`. Every
checkout, worktree or repository that pins the same commit uses that directory.
When the baseline moves (a vcpkg bump) the new commit gets a directory of its
own and nothing that already exists is touched, so a build that is running never
has vcpkg change under it. Directories of old commits can be deleted when you no
longer need them (`git -C <store>/mirror worktree remove <dir>`).

Already have a full clone of the fork? Set `ICL_VCPKG_MIRROR` (or
`CUBEULATOR_VCPKG_MIRROR`) to it and the store makes its checkouts from that
instead of cloning again. `CUBEULATOR_VCPKG_URL` names another fork to clone.

A build directory remembers the vcpkg it was first configured with, so after the
baseline moves, configure it again with `cmake --fresh --preset <name>` (CMake
says so). Build artifacts are separate from all this: share the downloads,
binary cache and installed tree with `VCPKG_DOWNLOADS`, `VCPKG_BINARY_SOURCES`
and `VCPKG_INSTALLED_DIR`, as vcpkg documents.

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
