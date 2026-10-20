# 3x3 and N x N: where the screens differ

An audit of every user-facing feature of the app, done on 2026-10-20, comparing what a
3x3 gets with what a 2x2, 4x4 and 5x5 get (the sizes offered on screen; the checks run at 6x6 too) (a 3x3 solved through `cube::nxn` is the
same path as the 3x3 and is not listed apart). It started from a report that the live view
of the cube being solved had gone from the solve screen of a bigger cube. It had:
`SolveScreen.qml` and `AppState.qml` both said "the camera reads a 3x3" and left the
preview off for any other size, which was true until the camera learned to scan every size
and was not changed when it did.

"Fixed" means fixed in the pull request that added this page. "By design" is a gap that
cannot be closed for a bigger cube, or should not be, with the reason. "Open" is real work
left, and why it was not done here.

## Gaps

| # | Feature | 3x3 | Bigger cube | Cause | Status |
|---|---|---|---|---|---|
| 1 | Camera preview on the solve screen (you, solving the cube, with a dot on each sticker found) | shown | **missing** | `AppState.solveCamera` was only true for a 3x3 and `SolveScreen` asked for a 3x3 again; `DirectScan` watched with the 3x3 reader | **Fixed.** The preview is shown at every size, the watch reads at the scan's size (size x size dots, smaller above 4), and it closes in on the middle of the side as it does for a 3x3. The mirror stays display-only |
| 2 | Words under the move beside the preview | "Hold any side square..." / "Matched..." | would have promised a 3x3's behaviour | the words come from `HeldCube`, which only follows a 3x3 | **Fixed.** A bigger cube says its cube is on camera, not followed yet, and to press Next turn (`solve.camera.shownOnly`, nine languages, drafts) |
| 3 | Check colours on the solve screen (look over what the camera read) | offered | missing | `canCheckColours` required a 3x3, from before the review screen took any size | **Fixed.** Offered for any cube that came from a scan |
| 4 | Scan instead / Scan again on the colours screen | offered | button hidden; and Scan again, or back out of a checked scan, began a 3x3 scan at the default size | `ColoursScreen` hid it above 3x3; `AppState.closeColours` and the discard dialog called `beginScan()` without a size | **Fixed.** Offered at every size, and the scan begins at the size of the cube on the screen (`AppState.beginScanAtSize`) |
| 5 | Choosing a method (Beginner, Intermediate, Advanced) | three buttons | on a 2x2 the same three buttons did nothing | a 2x2 is one search for its corners; the method is for the 3x3 stage, which a 2x2 does not have | **Fixed.** Left off a 2x2 |
| 6 | The camera follows the cube: which way it is held, the move seen and stepped on, the wrong move named, the cube on screen turning to match | yes | **no** | `HeldCube`, `cube::vision::read_hold` and `cube::solver::Hold` match nine stickers against a `solver::Cube`; there is no N x N equivalent | **Open.** A matcher over `NCube` and the lattice of a size x size side is a feature (and needs real recordings to tune its thresholds), not a parity fix. The guard that keeps `HeldCube` from judging a bigger side's read as a 3x3's is in place and tested. Until it lands the person presses Next turn |
| 7 | Shader styles (Realism, Instructional Glow, Cel shading) from Settings | all three | the default look only | `CubeNView` is one mesh per part with no per-sticker shader or post effect; the setting reaches `Cube3DView` alone | **Open.** Belongs with the render work on `CubeNView`; no per-size logic is involved |
| 8 | Scanning with edge views and corner views | yes | sizes 2 to 5 on screen | the corner reader goes to 6x6 (`kMaxCornerSize`); 6x6 to 9x9 are solved by the library but the entry and scan screens offer 2 to 5 | **By design** (the plan adds a size to the screens once its measurements are in) |
| 9 | Resume a solve from History without a camera | no camera (no scan to match) | no camera | a solve carried on from History has no scan | **By design**, the same at every size |
| 10 | The user guide said the camera reads only a 3x3 | | | text not updated when scanning other sizes landed | **Fixed** (`docs/guide/other-sizes.md`) |

## Features checked and level

Scan (flat sides, the size choice, size x size dots and chips), rescan during a solve,
next and previous turn, the layer a move names, the stage list, skip to the 3x3 stage
(a 4x4 and up; a 2x2 has nothing to skip), the solution review, the history list and
resume, hand entry and the review screen, the complete screen, drag to look round the cube,
the narrow and wide layouts (held by `layout.overlap` in all nine languages and the
pseudo-locale), right to left, and the web build (the same controllers and QML; the web
camera is asked for the width a bigger cube needs).

## Not verified

- **A real camera.** Everything here is held by drawn frames (`draw_side`) and the layout
  checks. The preview's closing in on a bigger cube, the dot size and the words have not
  been seen with a webcam, and the owner should look at a 4x4 held up in the solve.
- **The cost of the watch at 5x5 and 6x6 on a phone.** The watch now reads a side at the
  cube's size every frame, as the scan does; the scan's own cost (`docs/performance.md`)
  is the nearest measure.
- **The web build in a browser** (CI builds and drives it).
