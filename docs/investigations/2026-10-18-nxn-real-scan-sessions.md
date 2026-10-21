# 2026-10-18: what the real 2x2, 4x4 and 5x5 scan sessions say about the reader

**Asked:** 2026-10-18, by the maintainer: the new N x N recordings do not all
scan, "sometimes it's due to reflection, sometimes it's due to not quite colour
accuracy". Which of the reader's parts is losing them, and what should be tried
next?

**Scope:** `src/lib/src/vision/` (reader, colour, corner), the assembly it feeds
(`src/lib/src/nxn/scan_assembly.cpp`), and the capture loop
(`src/app/qt/controllers/direct_scan.cpp`), read against every recorded session
on the machine.

## The sessions

65 sessions of a real cube, recorded on the evenings of 2026-10-17 and 2026-10-18
with `E:\cb\nxn-scan-try\scan-{2x2,4x4,5x5}.bat` (`CUBE_SCAN_SIZE`,
`CUBE_SCAN_RECORD_DIR`), replayed with `cube_scan diagnose` and `cube_scan
replay`.

Two things about how they were made. The app in that run folder is the 12:45
build of the 18th, older than `E:\cb\scan-nxn-app` (13:59) and `E:\cb\nxn-rescan`
(14:22) beside it, so everything recorded today came from that one executable - it
does not matter for the reader, because `src/lib/.../vision` and `tools/scan` are
byte-identical across `develop`, `bugfix/nxn-scan-size-ci` and
`feature/nxn-solve-rescan`. And every `scan-*.bat` tells the person to "show each
of the six sides square to the camera and hold it still", which is the protocol
all 65 follow - and is half of finding 5.

What they came to:

| size | sessions | frames | median frames | groups of a side (mean / max) | six sides captured | a cube | ambiguous | six sides, no cube | naming refused | not six sides |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 2x2 | 43 | 12,915 | 214 | **12.9 / 24** | 30 | 11 | 8 | 9 | 2 | 13 |
| 4x4 | 15 | 4,459 | 259 | 8.0 / 15 | 11 | 4 | 3 | 1 | 3 | 4 |
| 5x5 | 7 | 3,136 | 345 | 7.3 / 10 | 7 | 4 | 2 | 0 | 1 | 0 |

"Naming refused" is `resolve_colors` declining the six sides - "no valid cube was
detected" - which is what it does when two of the six centres read as one colour:
either a genuine colour failure or, far more often here, the same side captured
twice. It is 6 of the 65 sessions.

Share of frames that read a side at all (median): 2x2 64%, 4x4 58%, 5x5 43%.

**Read these numbers with four caveats.** None of the 65 has a `truth.txt` or
`layout.txt`, so "a cube" means the six sides assembled into *some* valid cube,
not the cube in the hand. The sessions were recorded while the size support was
being built, so some are short deliberate probes (four are under 100 frames) and
a run that ends "fewer than six sides" can be the person stopping rather than the
scan failing. A recording ends on the frame the cube is read, so its length is
the time the scan took, not a delay. And, the one that decides how much of this
page to believe: **every recording is downscaled to 640 px wide**, so a 4x4 or
5x5 replayed from one is not what the app saw (see finding 0).

Time to complete, from each recording's own `index.csv` (the sessions that did
complete): 2x2 8.1 to 64.4 s, median **16.3**; 4x4 7.5 to 13.5 s, median
**10.0**; 5x5 6.8 to 40.3 s, median **22.8**. A 2x2 taking longer than a 4x4 is
the shape of finding 1.

The 3x3 has no session from these two evenings. Its record stands where it was:
the recovery plan's scoreboard has the four `20261013` daylight sessions and
`live-demo-p6-retry` exact, and the two hard ones from 2026-10-14 -
`corner-scans`, 480 frames by a lamp, and `red-orange-scans`, 1,449 frames - both
end **ambiguous**, with six sides and no way to tell which way up they were held.
Those two replay the same today: 0 and 9 corner reads out of 1,449 and 480
frames, both ambiguous.

## What it found

### 0. Every recording is downscaled to 640 wide, so the 4x4 and 5x5 rows above are a harder test than the app

The recorder resizes every frame it keeps to `kAnalysisWidth` (640) before
writing it, whatever size the cube is (`recording_v2.cpp:26`,
`recording_v2.hpp:45`). The camera here delivers 1920x1080 - every session's
`index.csv` says so in `full_w`/`full_h` - and the frames on disk are 640x360.

The reader does not work at 640 for those sizes. `prepare_frame` works at 640 for
a 3x3, **960 for a 4x4 and 5x5** and 1280 for 6x6 and up, downscaling the camera
frame to that width (`prepared_frame.cpp:27`). So live, a 4x4 or 5x5 was read
from a 1920-wide frame at 960; replayed from a recording it is read at 640 -
**1.5 times coarser linear, 2.25 times in area**. Every 4x4 and 5x5 number here
is therefore pessimistic, and the zero-read 4x4 session in finding 2 may well read
live.

For a 2x2 the recording is exact: its working width *is* 640. So finding 1 is
measured on the very pixels the app read, and the 4x4/5x5 findings are lower
bounds to be confirmed at the right width.

This also means **no tuning above a 3x3 can be validated on this corpus as it
stands** - the size-aware working width, the perspective fit, a finer block, all
of them would be measured against a 640-pixel stand-in.

### 1. A 2x2 cannot join a side to itself if one of its four stickers is off (bug)

`most_stickers_unlike_in_one_side(size)` (`direct_scan.cpp:214`) is
`(size * size) / 8`. For a 2x2 that is integer division of 4 by 8: **zero**. A
2x2 read must therefore match all four stickers within `kSameColourLab` (10 Lab)
to join a side, and it has no centre to vouch for it (`has_centre` is
`size % 2 == 1`, `fit_to_side` at `direct_scan.cpp:816`). One sticker moved by a
glint, a reflection or a lamp's edge - a quarter of the side - and the read is a
new side.

Nothing recovers it afterwards: the merge step (`merge_alike_groups`,
`first_alike_pair`) asks the same `fit_to_side`, so the same zero tolerance
applies; and the "this is one of them, lit differently" guard
(`is_shaded_view_of_any`, `direct_scan.cpp:1657`) is only asked of reads that are
**not** square-on. A square-on read that nearly matches a side is unguarded.

That is the shape of the measurements. A cube with six sides averages 12.9 groups
(max 24, the cap), and 37 of the 43 sessions have 8 or more. Seven sessions have
two of the six *captured* sides differing in one of four cells - the same side
taken twice - and `SideScan::sides()` keeps the six with the most reads, so the
second copy displaces the side that is missing. Two of those seven end "the six
sides make no cube", four never reach six sides, and one ends "a cube", which is
the false-accept risk in the flesh and cannot be checked without truth. Nowhere
else does a cube that is simply slow show up as twelve groups.

The plainest case, `20261018-141707-118` (239 frames, 24 groups, six captured,
no cube):

```
side seen 6th:  34 reads; middle cell Lab  92.6   6.6   4.9
side seen 23th: 19 reads; middle cell Lab  92.6   6.3   5.0
```

One side, captured twice - the seventh and the twenty-fourth group of a cube that
has six - both kept among the six because each has more reads than a side that
was only shown once. Seven corner reads were also kept and none could help: no
corner view can repair a cube with two of the same side in it. The session hit
`kMaxGroups` (24), so groups were being dropped as well.

The 4x4 and 5x5 are affected less but not zero (8.0 and 7.3 groups, 2 of 16 and
3 of 25 stickers allowed to differ), which fits: the tolerance is
`N^2 / 8`, so it only vanishes at the smallest cube.

### 2. Above a 3x3, most losses happen before the lattice: merged and missing blobs

The reader needs `min_lattice_blobs` = two thirds of the stickers *as blobs*
before it will try a lattice at all (`tuning_for`: 3 of 4 at 2x2, 11 of 16 at
4x4, 17 of 25 at 5x5). Everything downstream - the affine fit, `is_square_on`,
the patch sampling - is never reached on a frame that is two blobs short.

One 4x4 session (`20261017-233130-201`, 259 frames) reads **nothing at all**:
0 lattice reads, 0 corner reads, though a frame from it plainly shows the cube
in a hand. That is in the 640-wide recording (finding 0), so it is the worst case
and not necessarily what the app saw; it is quoted because it exposes the
mechanism. Reconstructing `prepare_frame` + `sticker_blobs` for two of its
frames in numpy (same brightness, 5x5 blur, adaptive block 33, offset 8, 3x3
open, then the area/shape gates for N=4):

| frame | sticker-sized blobs kept (of 16) | merged contours (1.5 to 4 stickers across) |
| --- | ---: | ---: |
| 120 | 11 - exactly the gate | 2 (89 and 96 px, against a 43 px sticker) |
| 200 | **9 - under the gate** | 2 (105 and 112 px, against a 27 px sticker) |

So the failure is not the lattice: stickers are being **bridged into one contour
by a highlight across the black gap**, and the count falls below two thirds
before the reader has looked. The second look at a narrower block does not help
here, because it runs only after the first look produced *at least*
`min_lattice_blobs` blobs (`read_side`, `direct_scan.cpp:1548`).

Two smaller fragilities in the same place:

- `gaps_are_plastic` measures the gaps against the **darkest sticker of the
  side** (`darkest` over the lattice's cells). A side with one sticker in deep
  shadow sets the reference low and every gap then fails, so a correct lattice is
  thrown away - on the second look only, but that is the look meant to rescue
  dark stickers. It should be the side's median sticker, not its darkest.
- The blob shape gates (`kMaxBlobAspect` 1.45, `kMinBlobFill` 0.72) drop a
  sticker whose contour is eroded by a reflection or a shadow at its edge. On a
  3x3 losing two of nine is affordable; on a 4x4 losing five of sixteen is not.

### 3. The lattice is affine; a cube held close is not

`fit_affine` and `fit_lattice_through_fit` fit `origin + column_step * c +
row_step * r`. That is a parallel projection. A 4x4 or 5x5 is held close to a
webcam - the 4x4 session above fills two thirds of the frame's height - and a
wide-angle lens gives a few per cent of pitch change across four or five cells.
`is_square_on` then judges that the same lattice is *not* square-on
(`kMaxLatticeRms` 0.12 of a spacing), or the far cells drift off their stickers
and the patch samples the plastic between them.

This is a guess from the geometry and the frames, not a measurement: nothing in
the tool prints the post-fit `rms`. It is cheap to settle - print `rms`,
`ratio` and `cosine` for the best lattice on a frame - which is the first thing
to add to `cube_scan`.

### 4. A scan takes many seconds, and the steadiness gate is in the wrong unit

`kMinReads` is 8 for every size (`direct_scan.hpp:280`), and the steadiness gate
allows `kSteadyFraction` (0.12) of a **sticker spacing** of movement between
frames. A bigger cube has a smaller spacing for the same cube on screen, so the
same hand-shake costs more frames: the S0 sweep measured 92% of frames counted at
+-0.5 px for 3x3, 5x5 and 7x7 alike, but 82 / 29 / 11% at +-4 px. The scans bear
the cost out: the sessions that completed took a median of 16.3 s for a 2x2, 10.0
for a 4x4 and 22.8 for a 5x5, the slowest 64 s.

Yet a read of a 5x5 carries 25 samples and is a *stronger* observation than a
3x3's nine: asking for eight of them is more than the evidence needs.

### 5. The corner reader is starved exactly when it is needed

`DirectScan::processFrame` (`direct_scan.cpp:658`) runs `read_corner` only on a
frame with **no** square-on read. The guidance tells the person to hold a side
square to the camera; the frames that hold two sides at once are the quick sweeps
between them. Both hard 3x3 sessions end ambiguous with 0 and 9 corner reads out
of 1,449 and 480 frames. The evidence that would settle them is being skipped on
every frame that has a square-on read - which is most of them - including every
frame after the sixth side is in, when the corner reader is the only thing that
can help.

The protocol doubles it. `scan-2x2.bat` and its siblings say "show each of the six
sides square to the camera and hold it still", so a person following them does
exactly that and produces almost no corner views: the reader is starved by the code
*and* by the instruction that fills its recordings.

### 6. The browser never gets the size-aware working width

`prepare_frame` works at 640 px for 3x3, 960 for 4x4 and 5x5, 1280 for 6x6 and
up. The web camera is capped at 640 wide (`MAX_FRAME_WIDTH` in `camera_web.js`,
`kFrameCapacity` 640x1280x4 in `camera_web.cpp`), so on the page a 5x5 is always
read at 640 and the gain S0 measured (a 7x7 readable from 25% of the frame in
daylight at 1280 against 31% at 640) never arrives.

### 7. Nothing tells the person what is wrong while they hold the cube

The scan screen has states for reading, next side, seen before, conflict and
ambiguous, and none for the three things these sessions actually fail on: the
cube is too small in the frame, a highlight is bridging stickers, or the hand is
moving too fast for the gate. The reader knows all three - blob count against
`min_lattice_blobs`, the too-big contours, and the distance the steady point
moved - and throws the numbers away.

### 8. A cell that is genuinely ambiguous is counted as if it were certain

`resolve_colors` sorts the 6N² samples into six clusters of **exactly N²**
(`refine_clusters` under the exactly-A constraint, `color_resolution.cpp:728`).
One glare-blown or badly-lit cell that lands in the wrong cluster pushes a
correctly-read cell of that colour out, the cube then fails to validate, and
nothing records which cell was doubtful. The confidence reported is 0 when a rival
cluster was as close as the winner. Over the sessions that reached naming:

| size | named cells | confidence 0.00 | under 0.60 |
| --- | ---: | ---: | ---: |
| 2x2 | 864 | 43 (5.0%) | 111 (12.8%) |
| 4x4 | 768 | 9 (1.2%) | 19 (2.5%) |
| 5x5 | 900 | 0 (0.0%) | 14 (1.6%) |

An eighth of a 2x2's named cells are under 0.60, and a 2x2's cell is a quarter of
its side - which is the "not quite colour accuracy" the request named, measured.
Above a 3x3 the naming is much more sure (1 to 3%), so colour is not what loses
the 4x4 and 5x5 sessions: the reader is.

Where `resolve_colors` refuses outright - "no valid cube was detected" - the cause
is two of the six centres within `kMinCentreSeedSeparationLab` (12 Lab). That is 6
of the 65 sessions (2x2 2, 4x4 3, 5x5 1). Both 2x2s had more than six sides
captured, so the likeliest cause there is the same side twice (finding 1) rather
than two colours confused; one 4x4 (`20261017-205916-992`) had exactly six, so
that one is the naming itself.

### 9. There is no truth to score any of this against

The single biggest obstacle to improving the scan is that none of these 65
sessions can be scored. Every claim above about accuracy is an inference. The
20261013 sessions were labelled by three agreeing scans; there is no equivalent
for a 2x2, 4x4 or 5x5, and `layout.txt` has to be written by hand.

## Recommendations, in the order to try them

Each is written as an experiment with the number that decides it and the thing it
must not break. **R0 comes before any of the others**: at 640 wide the corpus
cannot measure what R2 to R6 change. Each wants its own branch and worktree
(`..\wt\Cubeulator\<name>`), built out of tree (`E:\cb\<name>`) as every build
beside the corpus is, and thrown away if the number does not move.

To try one end to end rather than on the corpus: build the worktree, copy its
`Cubeulator.exe` into a run folder of its own (or over `E:\cb\nxn-scan-try`,
whose copy is already older than the builds beside it - refresh it first or the
comparison is between two apps), run `scan-2x2.bat`, and `cube_scan diagnose` the
session it wrote. The `scan-*.bat` files set `CUBE_SCAN_SIZE` and
`CUBE_SCAN_RECORD_DIR`; the size lands in the session's `manifest.json` (omitted
for a 3x3, which is why a 3x3 session reads back as one), so a session recorded at
another size without the size set is replayed as a 3x3.

### R0 - Record at the reader's width, then label the corpus (hours, and everything else depends on it)

1. Make `kAnalysisWidth` size-aware: 640 for a 3x3 (unchanged, so existing 3x3
   recordings replay identically), 960 for 4x4 and 5x5, 1280 above. Or turn on
   the recorder's full-resolution copy (`recorder_.start(..., true)`) and have
   `replay`/`diagnose` work from `fullres/`, which `manifest.size` already lets
   them scale for.
2. Add a "this is my cube" step after the colours review that writes `layout.txt`
   for the session, or a `cube_scan label` command that takes the reviewed cube
   and writes it. **The reading half is already there**: `truth_n_of` and
   `layout_truth_of` (`tools/scan/main.cpp:154`, `:248`) score a session against
   `layout.txt` or `cube_n.txt` today, and only something that *writes* one from a
   reviewed real scan is missing. With that, the 65 sessions become a scoreboard,
   `cube_scan board` gates every change below, and "wrong cube" becomes a number
   instead of an anecdote.

**Decides it:** a re-run of this page's tables on recordings at the right width.
Until then the 4x4 and 5x5 numbers are floors, and R1 is the only finding that
can be acted on with confidence, because a 2x2's width is 640 either way.

### R1 - Join an even-size read to its side, softly (bug fix, hours)

1. Floor the tolerance: `std::max<std::size_t>(1, (size * size) / 8)`. One of a
   2x2's four, one of a 4x4's sixteen.
2. Ask the shaded-view guard of **square-on** reads too when the side has no
   centre (`!has_centre`), so a read that misses by one sticker joins the side it
   is nearest rather than starting a seventh.
3. Optionally, keep a read as "near" a group and let two more reads settle it,
   instead of committing it on one frame.

**Decides it:** over the 43 2x2 sessions, `cube_scan diagnose` - mean groups (now
12.9, want ≤ 7), sessions with two captured sides differing in ≤1 cell (now 7,
want 0), and the outcome mix (now 11 cube / 8 ambiguous / 9 no cube / 2 naming
refused). **Must not move:** `cube_scan board` on the five known 3x3 sessions stays
5 of 5 exact, and `tests/unit/direct_scan_test.cpp` / `direct_scan_nxn_test.cpp`
pass.

### R2 - Recover merged and missing stickers before the lattice gate (days)

1. Split a contour that is over `max_blob_area` at the valleys inside it: distance
   transform + watershed, or simply cut it on the lattice's predicted pitch once a
   hypothesis exists.
2. Lower `min_lattice_blobs` for N ≥ 4 to about half (8 of 16, 12 of 25) and hold
   the lattice to `is_square_on`'s rms instead; the 3x3's six of nine is the
   behaviour to preserve, not the share.
3. Run the narrow-block second look on every frame above a 3x3, and add the third
   look at half the block that S0 measured but did not take (it read a 5x5 by lamp
   from pitch 16 at half again the block).
4. Make `gaps_are_plastic` compare against the side's median sticker, not its
   darkest.

**Decides it:** the share of frames that read a side, on the sessions with a cube
in view - 4x4 now 58%, 5x5 43% (median), both floors until R0 - and specifically
the 0-read session returning reads. **Cost:** `cube_scan cost` before and after; a
frame must stay inside the camera's interval (33 ms), and the browser has 3.5 to
8 ms today.

### R3 - Fit a homography for N ≥ 4, not an affine map (days)

Four extreme blobs give a projective frame; fit the rest through it. Keep the
affine path for 3x3 so its replay is byte-identical.

**Decides it:** print each candidate's `rms` (in sticker spacings) first, then
count reads on close-held 4x4/5x5 frames before and after - at the reader's width,
so after R0. If the rms on frames that do read is already near 0.12, perspective
is the cause; if it is near 0.04, R3 is not worth the cost.

### R4 - Make capture cheaper for a bigger side (days)

`kMinReads` per size (8 at 3x3, 4 at 4x4, 3 at 5x5 and above) and a steadiness
allowance that is a share of the **cube's** size in the frame rather than of a
sticker spacing.

**Decides it:** frames from first read to capture, per side, on the recorded
sessions (`cube_scan diagnose` already reports reads per side); and no rise in
sessions that capture the same side twice. **Must not move:** the drawn-tumble
sweeps (`cube_tumble_sim` + `cube_scan board`), which are the only place a
regression in per-side stability would show.

### R5 - Let the corner reader run when it can help (days)

Run `read_corner` on a square-on frame when (a) six sides are in and the cube is
ambiguous, or (b) every Nth frame regardless. The guard that keeps a corner read
from being counted twice is already in `SideScan::add_corner`; the only cost is
time, 2 to 5 ms at 4x4, 6 to 12 at 5x5, and it is paid only in the state where
the scan has nothing else to do. The asking has to change with it: a session
recorded under "hold each side square" holds no corner views for it to read, so
`scan-*.bat` and `scan.guide.ambiguous` should ask for a slow turn through a corner
once six sides are in, and the trial needs re-recording, not just a replay.

**Decides it:** `cube_scan corners` / `corner-colours` and, on the two ambiguous
3x3 sessions, whether the verdict resolves to the truth. **Watch:** the
browser's main thread (policy is in the web frame-cost investigation).

### R6 - Give the page the resolution the reader asks for (days, web only)

Raise `MAX_FRAME_WIDTH` and `kFrameCapacity` to 960 x 1280 x 4 (or negotiate the
track's largest mode and downscale to the working width for the size). This is
the single largest web-side lever for 5x5 and 7x7 and needs no vision change.

**Decides it:** a drawn 5x5 and 7x7 through the page's own camera path, and
`docs/investigations/2026-10-15-web-scan-frame-cost.md`'s budget.

### R7 - Say what is wrong while the cube is in the hand (days, UI)

Three new guidance signals, all already computed: sticker pitch in pixels (too
small - "move the cube closer"), the count of over-size contours (glare - "tilt
the cube away from the light"), and the steady point's movement (unsteady -
"hold it still"). The colour review (`ColoursScreen`) should be what a bigger
cube lands on, with low-confidence cells flagged, since a swap of two centres on
a 4x4/5x5 is invisible to `validate` (100% and 49.7% of centre swaps still
validate, per the S0 measurements).

### R8 - Let a doubtful cell be doubtful (days)

Carry `resolve_colors`' per-cell doubt into the assembly instead of rounding it to
a name: a cell whose second-nearest cluster is within a Lab unit or two may take
either name, and the cube is accepted if *some* assignment of the doubtful cells
validates. The search is bounded - most cells have no rival, and capping it at
(say) six doubtful cells is 64 assignments on top of an assembly that is already
4^6. Flag the doubtful cells on the review screen at the same time.

**Decides it:** the sessions that now end "no cube" with a naming that has
doubtful cells; and the false-accept rate, which this could raise - so it needs
the labelled corpus of R0 before it is trusted.

### Also worth fixing while in there

- `cube_scan diagnose` reports a 2x2's cell 2 as its "middle cell Lab"; an even
  size has no middle sticker, and the label misleads exactly where the centre is
  the thing missing.
- A scan whose six sides make no cube surfaces as
  `CubeError::invalid_cube_state` - "the supplied cube state is not physically
  valid" - which reads as the person's fault. It wants an error of its own
  ("the six sides do not make a cube").
- A 2x2 cannot spare a single sticker (3 of 4 make a lattice) and still read the
  side, which is stricter by share than the 3x3's six of nine. Two of four cannot
  pin an affine lattice, so the answer is not a lower count: it is a hypothesis
  search over blob triples, as `read_corner` already does, used as the fallback
  when the square grid leaves nothing.

## Where the code is

`src/lib/src/vision/direct_scan.cpp` - `most_stickers_unlike_in_one_side` (214),
`fit_to_side` (816), `is_shaded_view_of_any` (885), `gaps_are_plastic` (1435),
`read_blobs` (1465), `read_side` (1534), `count_read` (1621);
`src/lib/src/vision/prepared_frame.cpp` - `working_width_for` (27),
`outlines`/block (47); `src/lib/src/vision/recording_v2.cpp:26` with
`recording_v2.hpp:45` (the 640 analysis width); `src/lib/src/vision/color_resolution.cpp` -
`corner_seeds` (753), `refine_clusters` (728); `src/lib/src/nxn/scan_assembly.cpp`;
`src/app/qt/controllers/direct_scan.cpp:658` (corner gating);
`src/app/platform/web/camera_web.js:25` and `camera_web.cpp:73` (the 640 cap).

Reproduce the tables with:

```
cube_scan diagnose <session>     # groups, reads per side, near-identical sides, verdict
cube_scan replay  <session>      # frames, lattice and corner reads, sides, exact cube
```

over the private corpus's `nxn-real-scans\{2x2,4x4,5x5}`. No frame from it, and
nothing derived from one, is in this repository.
