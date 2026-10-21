# Settings and shortcuts

Open **Settings** from the start screen, or press <kbd>Ctrl</kbd>+<kbd>,</kbd>
(<kbd>⌘</kbd>+<kbd>,</kbd> on a Mac) from anywhere. **Back** returns to the
screen you came from. A solution is where you left it; a scan that was in
progress starts again from the first side, because the camera is closed while
Settings is open.

<figure markdown="span">
  ![Settings: Camera, Solving, Appearance, Language, History, Cube style and Cube motion, with a 3D cube beside them.](img/settings-wide-light.webp#only-light){ loading=lazy }
  ![Settings: Camera, Solving, Appearance, Language, History, Cube style and Cube motion, with a 3D cube beside them.](img/settings-wide-dark.webp#only-dark){ loading=lazy }
  <figcaption>The top of Settings on a desktop window. The list goes on, to
  Rendering, About and the Developer rows, and scrolls.</figcaption>
</figure>

## What is there

| Section | What it does | Kept for next time? |
| --- | --- | --- |
| **Camera** | Lists the cameras it found and uses the one you pick. **Look again** searches again, which is what to press after plugging one in. | No |
| **Solving** | The method: *Beginner*, *Intermediate* or *Advanced*. The same buttons are on the solve screen. See [Following the solution](solve.md#three-ways-to-solve-it). | Yes |
| **Appearance** | *Same as this device*, *Light* or *Dark*. It applies at once. | Yes |
| **Language** | *Same as this device*, or one of nine languages. It applies at once. See below. | Yes |
| **History** | *Keep a history of solves*, on by default, and **Clear history**. Both are described in [Past solves](history.md). | Yes |
| **Cube style** | How the 3D cube is shaded: *Physically-Based Realism* (the default), *Instructional Glow* or *Borderlands Cel-Shading*. It applies to the cube on the start, solve and review screens, at every size of cube. | No |
| **Cube motion** | Whether the cube on the start and settings screens turns by itself when left alone, and how fast. | No |
| **Rendering** | Which graphics system draws the window (Automatic, Direct3D 11 or 12, Vulkan, OpenGL; what is offered depends on the system). Automatic is right unless the cube does not draw, and falls back on its own if the first choice cannot start on your computer ([how](../graphics-api.md)). A change takes effect the next time Cubeulator starts. | Yes |
| **About Cubeulator** | The name, the version, who made it and under what licence, the address of the source code, and the way to the [licences](#about-and-licences). (On a Mac it is in the Help menu.) | |
| **Show frame rate** | A small counter over the 3D cube, for people measuring performance. | No |
| **Write diagnostics to a file**, **Also keep camera frames**, **Snapshot now** | For people debugging a scan: write what the scan reads and decides to a file on your device. Off every time the app starts. See [Privacy](privacy.md#developer-recording-and-diagnostics). | No |

A wide window also has a **Test turn** button under the cube, which turns a
random layer so you can see the cube style at work.

## About and licences

**About Cubeulator** (at the end of the list in Settings, and in the Help menu
on a Mac) names the app and its version, says who made it, that it is free
software under the GNU General Public License, and gives the address of its
source code, which opens in your browser when you press it. **Back**, or
<kbd>Esc</kbd>, returns to where you opened it. It is not offered in the middle
of a scan or a solve, where leaving would lose your place.

**Licences** lists Cubeulator itself and every third-party component that is in
this build, by name, each with the licence it is used under: the libraries, the
parts of Qt, and the typefaces (Archivo and IBM Plex Mono). Press a row to read
its copyright notice and licence in full. These are legal texts and are shown in
the words their authors chose, in every language, left to right; you can scroll
them with the arrow and page keys and select and copy them. <kbd>Esc</kbd> goes
back one step at a time: from the text to the list, from the list to About. The
same notices are in the installed package, in `THIRD-PARTY-NOTICES.txt`.

## Language

Cubeulator has nine languages. Each is named in itself, so you can find yours
even when the screen is in another one:

English, Deutsch, Français, Español, 日本語, 简体中文, עברית, العربية, ייִדיש

Choosing one changes the words, the direction of the layout (Hebrew, Arabic and
Yiddish are written right to left, and the screen mirrors), numbers and plurals
at once. **Same as this device** follows the system's language.

What to expect in this beta:

- **English** is complete, and the source of every other.
- **Japanese** and **Simplified Chinese** are complete first drafts made with
  machine assistance that no fluent reader has been through yet. Expect
  wording a native speaker would change.
- **German, French, Spanish, Hebrew, Arabic and Yiddish** show English text
  until their translations are written. They are selectable so that the layout
  and direction can be tried.
- **The scan screen's guidance and most of the words round the solution** (the
  stage names, *Previous*, *Next turn*, *Finish*, *Out of step? Rescan*) are
  English in every language for now.

Japanese and Chinese are drawn in your system's own fonts (the browser version
carries its own). On Linux, install a CJK font such as Noto Sans CJK.

## Keyboard

| | Windows, Linux | macOS | Where |
| --- | --- | --- | --- |
| Settings | <kbd>Ctrl</kbd>+<kbd>,</kbd> | <kbd>⌘</kbd>+<kbd>,</kbd> | Anywhere but Settings |
| New scan | <kbd>Ctrl</kbd>+<kbd>N</kbd> | <kbd>⌘</kbd>+<kbd>N</kbd> | The start and solved screens (never in the middle of a scan or solve, where it would lose your place) |
| Full screen | <kbd>F11</kbd> | <kbd>Ctrl</kbd>+<kbd>⌘</kbd>+<kbd>F</kbd> | Anywhere |
| Quit | <kbd>Ctrl</kbd>+<kbd>Q</kbd> | <kbd>⌘</kbd>+<kbd>Q</kbd> | Anywhere |
| Close window, Minimise | | <kbd>⌘</kbd>+<kbd>W</kbd>, <kbd>⌘</kbd>+<kbd>M</kbd> | Anywhere |
| Next turn | <kbd>→</kbd>, <kbd>Space</kbd>, <kbd>Enter</kbd> | the same | The solve screen |
| Previous move | <kbd>←</kbd> | the same | The solve screen |
| Play or pause, first turn, solved cube | <kbd>Space</kbd>, <kbd>Home</kbd>, <kbd>End</kbd> | the same | The solution screen ([Following the solution](solve.md#reviewing-the-solution)) |
| Back | <kbd>Esc</kbd> | the same | The history, solution, About and Licences screens, and a question asking before it deletes |
| Move between the rows of About and Licences, open one | <kbd>Tab</kbd>, <kbd>↑</kbd> and <kbd>↓</kbd>; <kbd>Enter</kbd> or <kbd>Space</kbd> | the same | About and Licences |
| Diagnostics on or off, snapshot | <kbd>Ctrl</kbd>+<kbd>Shift</kbd>+<kbd>D</kbd>, <kbd>Ctrl</kbd>+<kbd>Shift</kbd>+<kbd>S</kbd> | <kbd>⌘</kbd> in place of <kbd>Ctrl</kbd> | Anywhere ([Privacy](privacy.md#developer-recording-and-diagnostics)) |

*Esc* goes back on the history, solution, About and Licences screens; elsewhere use the
**Back**, **Cancel** and **Finish** buttons.

On a Mac the same actions are in the menu bar (File, View, Window, Help). The
Help item is greyed out: there is no help screen in this release.

## The window

The window opens at 960 by 680 pixels and remembers its size, its position and
whether it was maximised. A position that is off every screen (a monitor you
have since unplugged) is ignored. It can be made as small as 360 by 560.

Below 720 pixels wide the screens use a single-column, phone-shaped layout, with
the same controls arranged one above another; that is also what you get in a
browser on a phone.

## Where the settings are kept

In the system's usual per-user settings store: the registry
(`HKEY_CURRENT_USER\Software\Cubeulator\Cubeulator`) on Windows, a preferences
file on macOS, `~/.config/Cubeulator/Cubeulator.conf` on Linux, and the
browser's local storage for the site in a browser. They stay on your device
([Privacy](privacy.md)). Uninstalling does not remove them.

## Next

[Privacy](privacy.md), or [Troubleshooting](troubleshooting.md).
