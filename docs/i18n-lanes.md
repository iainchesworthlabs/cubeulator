# Literals not yet converted

The English literals the QML still shows without a string id (73 at
0.2.0-beta.4), grouped by the lane that owns each file, with the id in
`strings.json` to use where the design has one. It is the output of
`python tools/i18n/lint_strings.py --report`, whose allowlist is
`tools/i18n/lint_allowlist.json`: regenerate it rather than editing it, as the
line numbers drift. The lane names are the branches the work was split across
when the lint was written; most have since merged.

### experience lane (feature/app-experience)

| file | line | literal | id in strings.json |
|---|---|---|---|
| `CompleteScreen.qml` | 162 | METHOD | none: needs a string |
| `SettingsScreen.qml` | 233 | Shader style for the 3D cube shown around the app. | none: needs a string |
| `SettingsScreen.qml` | 249 | CUBE MOTION | none: needs a string |
| `SettingsScreen.qml` | 253 | Auto-rotate when idle | none: needs a string |
| `SettingsScreen.qml` | 269 | RENDERING | none: needs a string |
| `SettingsScreen.qml` | 443 | Test turn | none: needs a string |
| `StatsOverlay.qml` | 65 | FPS  | none: needs a string |

### kit session (feature/ax-kit)

| file | line | literal | id in strings.json |
|---|---|---|---|
| `SettingsState.qml` | 72 | System | none: needs a string |
| `SettingsState.qml` | 73 | Light | `settings.theme.light` |
| `SettingsState.qml` | 74 | Dark | `settings.theme.dark` |
| `SettingsState.qml` | 90 | Auto | `scan.size.auto` |
| `SettingsState.qml` | 27 | Physically-Based Realism | none: needs a string |
| `SettingsState.qml` | 27 | Instructional Glow | none: needs a string |
| `SettingsState.qml` | 27 | Borderlands Cel-Shading | none: needs a string |

### scan lane (feature/direct-scan)

| file | line | literal | id in strings.json |
|---|---|---|---|
| `DirectScanScreen.qml` | 73 | Camera unavailable | none: needs a string |
| `DirectScanScreen.qml` | 75 | Cube read | `scan.title.complete` |
| `DirectScanScreen.qml` | 77 | Starting the camera… | `scan.title.starting` |
| `DirectScanScreen.qml` | 78 |  of 6 sides read | none: needs a string |
| `DirectScanScreen.qml` | 84 | All six sides make a valid cube. Working out the solution… | `scan.guide.complete` |
| `DirectScanScreen.qml` | 88 | Setting the camera's exposure and colour balance… | `scan.guide.settling` |
| `DirectScanScreen.qml` | 94 | These six sides fit together in more than one way. Turn the cube slowly so that two sides show at once. | `scan.guide.ambiguous` |
| `DirectScanScreen.qml` | 97 | These six sides do not make a valid cube yet. Show each side again, square to the camera. | `scan.guide.conflict` |
| `DirectScanScreen.qml` | 99 | Reading this side — hold it there | none: needs a string |
| `DirectScanScreen.qml` | 101 | Hold it still | `scan.guide.inView` |
| `DirectScanScreen.qml` | 103 | Hold one side of the cube square to the camera | `scan.guide.first` |
| `DirectScanScreen.qml` | 104 | Turn to a side that is not filled in below, square to the camera | `scan.guide.next` |
| `DirectScanScreen.qml` | 230 | Recording this scan to  | none: needs a string |
| `DirectScanScreen.qml` | 359 | Cancel | `common.cancel` |
| `DirectScanScreen.qml` | 375 | Start over | `scan.startOver` |

### solve lane (feature/solve-guidance)

| file | line | literal | id in strings.json |
|---|---|---|---|
| `SolveScreen.qml` | 23 | White cross | `stage.1.title` |
| `SolveScreen.qml` | 23 | Bring the four white edges home, matching each side centre. | none: needs a string |
| `SolveScreen.qml` | 24 | White corners | `stage.2.title` |
| `SolveScreen.qml` | 24 | Insert the four white corners to complete the first layer. | none: needs a string |
| `SolveScreen.qml` | 25 | Middle layer | `stage.3.title` |
| `SolveScreen.qml` | 25 | Send the middle-layer edges home. | none: needs a string |
| `SolveScreen.qml` | 26 | Orient last layer | none: needs a string |
| `SolveScreen.qml` | 26 | Orient the last layer's edges and corners so the top side reads solid. | none: needs a string |
| `SolveScreen.qml` | 27 | Permute last layer | none: needs a string |
| `SolveScreen.qml` | 27 | Permute the last layer's pieces into their solved positions. | none: needs a string |
| `SolveScreen.qml` | 184 | wrong | none: needs a string |
| `SolveScreen.qml` | 184 | That was not the move | none: needs a string |
| `SolveScreen.qml` | 185 | unknown | none: needs a string |
| `SolveScreen.qml` | 185 | That side is not expected | none: needs a string |
| `SolveScreen.qml` | 510 | STAGE  | none: needs a string |
| `SolveScreen.qml` | 510 |  OF  | none: needs a string |
| `SolveScreen.qml` | 514 | Solved | `complete.title` |
| `SolveScreen.qml` | 579 | Drag the cube to look round it | none: needs a string |
| `SolveScreen.qml` | 824 | Rescan the cube and carry on from there | none: needs a string |
| `SolveScreen.qml` | 850 | Only the view has turned. Keep your cube as it is. | none: needs a string |
| `SolveScreen.qml` | 1062 | STAGES | none: needs a string |
| `SolveScreen.qml` | 1140 | Previous | `solution.previous` |
| `SolveScreen.qml` | 1179 | Out of step? Rescan | none: needs a string |
| `SolveScreen.qml` | 1223 | Solving… | `settings.section.solving` |
| `SolveScreen.qml` | 136 |  on top,  | none: needs a string |
| `SolveScreen.qml` | 137 |  facing you | none: needs a string |
| `SolveScreen.qml` | 155 | Camera unavailable | none: needs a string |
| `SolveScreen.qml` | 157 | Starting the camera… | `scan.title.starting` |
| `SolveScreen.qml` | 163 | Matched to your cube | none: needs a string |
| `SolveScreen.qml` | 169 | Matched. The camera cannot see the side facing you: press Next after this move. | none: needs a string |
| `SolveScreen.qml` | 171 | Matched. Make the move and it steps on by itself. | none: needs a string |
| `SolveScreen.qml` | 172 | That side looks the same turned. Show another to tell which way up. | none: needs a string |
| `SolveScreen.qml` | 173 | Not the move that was asked for | none: needs a string |
| `SolveScreen.qml` | 174 | Not the side that was expected | none: needs a string |
| `SolveScreen.qml` | 176 | Show a side again if you turn the cube over | none: needs a string |
| `SolveScreen.qml` | 177 | Hold any side square to the camera and the cube here turns to match | none: needs a string |
| `SolveScreen.qml` | 191 |  was turned, but not the way shown. Turn it back and try again. | none: needs a string |
| `SolveScreen.qml` | 193 | ) was turned instead. Turn it back, then make the move shown. | none: needs a string |
| `SolveScreen.qml` | 196 | The side facing the camera is not what the solution has there. If the last move went wrong and cannot be put right, rescan. | none: needs a string |
| `SolveScreen.qml` | 218 | Left side | none: needs a string |
| `SolveScreen.qml` | 218 | Right side | none: needs a string |
| `SolveScreen.qml` | 220 | half turn | `amount.half` |
| `SolveScreen.qml` | 220 | quarter turn | `amount.quarter` |
| `SolveScreen.qml` | 246 | Seen from behind | none: needs a string |
