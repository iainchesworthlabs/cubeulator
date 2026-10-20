# Following a bigger cube with the camera

The solve screen of a 3x3 watches the cube in the person's hands: it works out how the cube is held
(and turns the cube on screen to match), sees the move that was asked for being made and steps on
by itself, and names a wrong turn. A 2x2, 4x4, 5x5 or 6x6 shows the camera's picture
([`nxn-ui-parity.md`](nxn-ui-parity.md), gap 6) but follows nothing: the person presses **Next
turn** after every move. This page is the design for closing that gap, and the record of what is
known about it and what is not. The code is `HeldCube`
(`src/app/qt/controllers/held_cube.cpp`) and `cube::vision::read_hold`
(`src/lib/include/cube/vision/held_cube.hpp`); the 3x3 half is described in
[`recovery-plan.md`](recovery-plan.md) and is not changed by any of this.

The rule that governs all of it is the 3x3's: **a wrong read must never step the solution.** Being told
a correct move was wrong, or having a move skipped, is worse than pressing a button. Everything below
that decides anything decides it only on evidence a glint, a finger or a misread colour cannot supply
alone, and when the evidence is not there the screen says so and the person presses **Next turn**.

## What the camera can see

One side of the cube, square to the camera, as N x N cells: the side facing away from the holder.
Nothing is tracked between reads, and nothing is read at an angle. The cube is known (it was scanned,
and every move since is the solution's), so each read is compared with what each of the cube's 24
holds would show, and with what they would show after the move that was asked for.

That fixes what a camera can say about a turn of one layer, whatever the size. Counted over every
turn of every layer of a scrambled cube from every hold (`held_cube_nxn_test.cpp`,
`[.observability]`):

| size | layer turns x holds | seen | **not seen** | fewest cells it changes | median cells | looks like rolling the cube |
|---|---|---|---|---|---|---|
| 2x2 | 2592 | 2112 | 480 (19%) | 1 | 2 | 496 |
| 3x3 | 3888 | 3024 | 864 (22%) | 1 | 3 | 432 |
| 4x4 | 5184 | 3888 | 1296 (25%) | 1 | 4 | 432 |
| 5x5 | 6480 | 4752 | 1728 (27%) | 2 | 5 | 432 |
| 6x6 | 7776 | 5616 | 2160 (28%) | 2 | 5 | 432 |

- **A turn is seen when it changes the side in view.** The layers *across* the line of sight (two of the
  three axes) each change one row or column of the side: N cells, fewer where neighbours share a colour.
  The outermost layer *facing* the camera turns the whole side in place. The other layers on that axis,
  **the inner slices parallel to the side in view and the layer at the back, change nothing the camera
  can see.** That is (N-1)/3N of all turns: a 3x3's "the side facing you", and on a bigger cube every
  inner layer behind the front one as well. Those moves can never be seen from that hold; the person
  presses **Next turn**, and the screen says why (the layer is hidden from the camera, from here).
  Showing another side first does not help: a move is hidden or not by where its layer is to the camera.
- **A turned front side looks like a rolled cube.** The side facing the camera, turned a quarter, shows
  what the whole cube rolled the same way would. A 3x3 settles this by remembering that the cube was
  seen held as shown while this move was being asked for; the same is done here, and without that
  sighting nothing is decided.
- **A wide move is the union of its layers** and is seen if any of them is. A half turn is two quarters;
  the quarter on the way is progress, not a mistake (the 3x3 does the same).
- **Few cells can change.** The fewest is one or two, when the row that moved is the colour of the one
  it replaced. A move that changes fewer than two cells (as the camera would see them, from the hold it
  is seen in) is **not followed**: one cell is as likely a glint as a turn.

## Deciding, and how sure it has to be

Per steady frame (the side held still since the frame before, as the 3x3 requires), for a cube of size
N, the same questions in the same order as the 3x3 (`HeldCube::judge`), each asked of N^2 cells, the
cells the reader found:

1. **Has the move been made?** The read fits the cube after the move clearly better than the cube as it
   stood: `distance(here) >= distance(after) + margin`. `kHoldFitDistance` (3.5, a *mean* over the
   cells, so it does not depend on N) is the bound on a fit; the margin is
   `hold_clear_distance(N)`: the 3x3's 0.5 scaled by 9/N^2 and no higher, because one cell is a smaller
   part of the mean the more cells there are. **In addition** (the 3x3 does not need it, a row of three
   being a third of a side), the cells where the two cubes differ must be enough to decide on: at least
   two of them, at least half of them found, and at least four fifths of those found nearer the colour
   the move gives them than the colour they had. A move that passes the mean test on one cell's evidence
   does not pass this.
2. **Is it held as shown?** The read fits the cube as it stands, in the hold already seen, or in another
   (which then becomes the hold, and the cube on screen turns to it). A side that looks the same turned
   is "unsure" and decides nothing.
3. **Is it a mistake?** Said only of a side seen whole (all N^2 cells found), with at least
   `max(2, N^2 / 12)` cells plainly the wrong colour (two for a 3x3, 2 for a 4x4 and 5x5, 3 for a 6x6),
   for most of a second (`kFramesToDoubt`), and not a half turn on its way. Named (the side that was
   turned) only if one side's turn, by any amount but the one asked for, explains the read and no other
   side's does. On a cube of 4 or more the turn of an inner layer is among the candidates; the side is
   named, the layer is not.

The frame counts are the 3x3's (`kFramesToTrust` 8, `kFramesToAccuse` 20, `kFramesToDoubt` 30). So are
the verdicts: a side seen whole and shown to match the cube is "matched", a read that fits nothing is
doubted slowly, and a read that fits one side's turn is named only once.

### Why these numbers, and what they were not measured on

The 3x3's thresholds were measured on five recorded scans of real cubes (`recovery-plan.md`). Nothing
of the sort exists yet for a bigger cube in the hand, so what is above is the 3x3's, **scaled by
argument, not by measurement**:

- `kHoldFitDistance` is a mean, and the fit of a read to the hold it shows should not depend on how
  many cells there are. Whether it does, on a real 4x4 under a lamp, is unmeasured.
- The margin shrinks with the cell count on the argument that one cell is worth 1/N^2 of the mean. The
  vote on the cells that differ (point 1) is there so that no decision rests on the margin alone.
- Fingers cover more of a big side than a small one; "found" is judged cell by cell, and a decision
  needs the cells that decide it, not the whole side. A mistake is said only of a whole side.

What is **measured**: a real recorded scan of a 2x2, 4x4 and 5x5 has steady reads of each side of a real
cube (`E:\cb\nxn-real-scans`, kept off the repository: it shows a person). Replayed against the cube the
scan made, they give the first real numbers for the fit of a read to the hold it shows, by size
([Measured](#measured)). They say nothing about moves: nobody has recorded a bigger
cube being turned in front of the camera.

## When it cannot follow

The screen never says less than it does today. Every state below ends the same way: **Next turn** steps
the solution, and nothing the camera says can step it except a move it has seen made.

| the camera sees | the screen says | the move |
|---|---|---|
| no side square to it | how to hold the cube (as the 3x3's) | **Next turn** |
| a side that fits one hold | nothing wrong; the cube on screen turns to the hold seen | made and seen: stepped on |
| a move whose layer is hidden from this hold | "matched: the camera cannot see this layer from here, press Next turn after this move" | **Next turn** |
| a move that changes fewer than two cells | the same words | **Next turn** |
| a side that looks the same turned | "show another side to tell which way up" | **Next turn** |
| not the move asked for | "that was not the move": the side, if one turn explains it | **Next turn** or undo it |
| fits nothing | "not the side that was expected" | **Next turn**, or rescan |

Rescan (`solve.rescan`) and Check colours stay where they are for every size.

## Plan

1. **The library** (this page's first change): `nxn::camera_view` (the side a camera sees under a hold,
   at any size), `read_hold`, `fit_holds` and `stickers_off` for an `NCube`, `hold_clear_distance`. Tested
   against the 3x3 (byte for byte at size 3) and on drawn sides of 2x2 to 6x6 from every hold.
2. **The controller and the screen**: `HeldCube` follows a cube of any size; the cube on screen turns to
   the hold; the words above. Tested with the app's own scan, solve and a camera fed drawn frames,
   including the cases that must not step: a wrong side, a glint, a finger, a hidden layer, a move
   half-made, noise on every cell.
3. **A measurement**: `cube_scan holds` for a bigger cube's recording, so that the margin and the vote
   can be set on a real cube's reads. Until it has been run on a cube *turned* in the hand, the
   thresholds are an argument.

## Measured

`cube_scan holds <session>` now takes a recording of any size. Replayed over the 65 sessions of real 2x2, 4x4 and 5x5 cubes recorded on 17 and 18 October (which show a person and are not in the repository), today's scan makes a cube from seven of them; for the steady reads of each, against the cube it made:

| session (size) | steady reads | read shows a hold | median distance to the hold shown | 90% | next different hold is at least |
|---|---|---|---|---|---|
| 4x4, 212724 | 167 | 127 (76%) | 1.87 | 4.21 | 13.0 further |
| 4x4, 212851 | 102 | 100 (98%) | 1.38 | 1.61 | 18.5 |
| 5x5, 213335 | 100 | 58 (58%) | 3.10 | 5.08 | 21.2 |
| 2x2, 223424 | 130 | 106 (82%) | 2.78 | 5.44 | 0.0 (look-alikes) |
| 2x2, 141602 | 130 | 87 (67%) | 2.26 | 5.00 | 3.0 |
| 2x2, 141718 | 115 | 38 (33%) | 4.00 | 7.57 | 6.2 |
| 2x2, 143526 | 119 | 45 (38%) | 4.40 | 10.54 | 0.0 (12 reads between look-alikes) |

For comparison the 3x3's five scans sat at a median of 1.3 to 1.7 with nine in ten within 3.5.

- **A 4x4 in the hand is read as well as a 3x3.** The distance does not grow with the cell count, which is
  what taking it as a mean assumed.
- **The next different hold is far away** (13 to 26 further than the one shown) on every 4x4 and 5x5 read, so the margin
  (0.18 to 0.28 at these sizes) is nowhere near binding. What decides how often a bigger cube is followed is
  `kHoldFitDistance`, not the margin.
- **A 5x5 and a 2x2 sit further from the hold they show**, a median of 2.3 to 4.4, and a third to two thirds of
  their steady reads are over 3.5 and so show no hold. Following those cubes will, on these recordings, be
  available some of the time and fall back to **Next turn** the rest. The threshold is deliberately not
  loosened for it: the cause (the 2x2's colours are named from its corners; the 5x5 has more sticker to sample
  wrongly) is not known, and a threshold chosen to make seven sessions look good is how a wrong read steps
  the solution.
- **A 2x2 side can look the same turned**: two sessions have reads that two holds show equally (distance 0.0 between
  them). Those decide nothing, as a 3x3's look-alikes do.
- **Nothing here is about moves.** No recording of a bigger cube being turned in front of the camera exists.
  What a turn looks like to the reader (a layer in motion, a hand across the side, the colours of the row that moved) is
  covered only by drawn frames until one does.
