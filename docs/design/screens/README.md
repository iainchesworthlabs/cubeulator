# Screen specs

> [!NOTE]
> **Status in 0.2.0-beta.4:** These are specifications, not descriptions of the
> app. The app has twelve screens, all of them in `src/app/qt` (`Main.qml`
> routes between them), and none of them built from the kit's components (see
> [system.md](../system.md)). **Built:** the solution (`SolutionScreen.qml`),
> History (`HistoryScreen.qml`), Licences (`LicencesScreen.qml`) and *Another
> size of cube* (`EntryScreen.qml`, specified in
> [review.md](review.md#a-cube-of-another-size), a screen of its own). **Partly
> built:** Home, Permission, Scan, the colours screen (Review and hand entry),
> Solve, Complete, Settings and About. **Not built:** First run, Privacy and
> Help. Each page opens with its own status, and the table at the foot says it
> at a glance. A screen's `objectName`s are the spec's only where its page says
> so: Home's are `launch.*`, the colours screen's `colours.*`, and the scan and
> solve screens have a few of theirs.

One file per screen. Each follows the same order:

1. **What it is for**, its lane, and where it is entered from.
2. **Layout** in each size class (`../system.md`, "Size classes"): compact,
   short and expanded. Medium is the compact layout in a 560-wide centred
   column unless a spec says otherwise.
3. **Parts**: every control and region, the kit component it is, the
   strings it shows, and its `objectName`. Tests find controls by
   `objectName` and by accessible name (plan, lane contracts).
4. **States**: one heading per state, and the heading is the state's id
   from `../journey.md`. `docs/design/tools/check_design.py` fails if the
   two lists differ, or if a spec names a string that `../strings.json` does
   not have.
5. **Needs** from the scan or solve lane, where the screen is theirs.

Conventions:

- `objectName` is `<screen>.<part>` in camelCase, and never changes once a
  test uses it.
- An accessible name is the visible label unless the table gives another.
- "Leading" and "trailing" are the start and end of a line of text: left
  and right in English, swapped in Hebrew, Arabic and Yiddish.
- Sizes are in device-independent pixels at a text scale of 1.0.
- Where a screen belongs to another lane, the spec is a proposal for its
  chrome and states. How the cube is read and what a turn means are not
  specified here.

The prototype (`../prototype/cubeulator-prototype.html`) draws every state
in every size class. Where the prototype and a spec disagree, the spec
wins and the prototype is wrong.

| screen | file | lane | in 0.2.0-beta.4 |
|---|---|---|---|
| Home | [home.md](home.md) | experience | Partly: `LaunchScreen.qml` (`launch.*`). Scan a cube, Another size of cube, History, a language button and Settings; no Enter colours by hand, Help, unfinished-solve card or camera notices |
| First run | [first-run.md](first-run.md) | experience | Not built |
| Permission | [permission.md](permission.md) | experience | Partly: `PermissionScreen.qml`, on phones only (an iPhone reaches a scan from it; Android has no camera backend yet). Desktops, macOS included, and the browser never show it |
| Scan | [scan.md](scan.md) | scan (proposal) | Partly: `DirectScanScreen.qml`. Dots, corner dots, the status panel, six chips, size buttons, **Check the colours**, **Switch camera**, **Try again**; no dialogs, More sheet, camera picker, stuck hint or hand entry |
| Review and hand entry | [review.md](review.md) | scan (proposal) | Partly: `ColoursScreen.qml` (`colours.*`), which is the colours screen of 2×2 to 5×5 by hand, and of a misread scan put right. The 3x3 sketch's own states (`review.working`, `review.solve-failed`), its keyboard map and the unfolded overview are not built |
| Another size of cube | [review.md](review.md#a-cube-of-another-size) | experience | Built: `EntryScreen.qml` (`entry.*`), from Home; it has no state in [journey.md](../journey.md) |
| Solve | [solve.md](solve.md) | solve (proposal) | Partly: `SolveScreen.qml`. Stepping, the three methods, the camera following the turns, **Check the colours**, **Out of step? Rescan**; no hold step, pause menu, stage check, timer mode or 96-high buttons |
| Complete | [complete.md](complete.md) | solve; chrome by experience | Partly: `CompleteScreen.qml`. No Done, no **My cube is not solved** |
| The solution | [solution.md](solution.md) | experience | Built, as `SolutionScreen.qml`, for a cube of any size from 2×2 to 5×5 |
| History | [history.md](history.md) | experience | Built, as `HistoryScreen.qml` |
| Settings | [settings.md](settings.md) | experience | Partly: `SettingsScreen.qml`. Camera, method, theme, language, history, About (desktop and browser) and the developer rows; no marks, text size, motion, sound, notation or Help |
| About | [about.md](about.md) | experience | Partly: `AboutScreen.qml`, on desktop and in the browser (not on phones). No icon, Privacy row, build line or Copy details |
| Licences | [licences.md](licences.md) | experience | Built, as `LicencesScreen.qml`, from the notices the build generates |
| Privacy | [privacy.md](privacy.md) | experience | Not built |
| Help | [help.md](help.md) | experience | Not built |
