# Solve

> [!NOTE]
> **Status in 0.2.0-beta.4:** Partly built, as `SolveScreen.qml`, for a cube of
> any size from 2×2 to 5×5: the cube in 3D with the layer to turn nudging the
> way it goes (a layer inside a big cube is named in words, `solve.move`), the
> hold line, the solve methods (`solve.method`; not shown for a 2×2, which has
> one way to be solved), the stage list on the wide layout, **Skip to the 3x3 stage**
> on a cube of 4 or more (`solve.skip`), **Previous** and **Next turn** (Right
> arrow, Space and Enter also step; Left arrow steps back), **Finish** to
> Complete, **Check the colours** (for a cube that came from a scan; it opens
> the colours screen, `solve.checkColours`), **Out of step? Rescan**, and a
> camera view that steps the solution on by itself, says which turn was made
> wrong, and says when the cube and the picture part (the camera follows a
> bigger cube's turns too, but cannot see a layer behind the side in view, and
> then waits for **Next turn**). Not built: the hold step, the pause menu and
> its sheets, the stage check, timer mode, the 96-high buttons, the three-line
> instruction block, notation, sound, carrying on after leaving (a solve carried
> on from History goes straight to its turn, with no camera), and keeping the
> screen awake. Where it differs from this page: the cube can be dragged, and
> the camera follows the turns, which the spec lists as later. Every word on it
> asks for its string by id. Some are worded differently from this page, and
> have strings of their own so that the design's keep theirs ("The words the
> built screen has", below): the stage names of stages 4 and 5 are "Orient last
> layer" and "Permute last layer", not the design's "Yellow top" and "Top layer
> in order" (`stage.4.title`, `stage.5.title`, which the solution screen does
> use), the button is **Previous**, not **Back one**, and the hold line names the
> colours the cube is held by.

Lane: **solve** (`feature/solve-guidance`). This is a proposal for the
chrome round the cube view and for the states of `SolveScreen.qml`. The
cube view, its animation and what a turn means are theirs.

One turn at a time until the cube is solved. **Both hands are on the
cube.** The device is propped up or lying flat and read from arm's
length. So: two targets a knuckle can hit, type twice the usual size, a
screen that stays awake, and nothing small anywhere.

Entered from: `scan.complete`, `review.working`, **Carry on** on Home or
in History (the app has it in History only).

## Advancing a turn with both hands full

The solve lane owns the mechanism. This is the proposal, with what was
weighed.

| way | for | against | verdict |
|---|---|---|---|
| **A large tap.** The whole cube picture, and a bar 96 high across two-thirds of the width | Works today. A knuckle, the side of a hand or a little finger will do. One turn, one tap: the person sets the pace and cannot be overtaken | A hand leaves the cube for an instant, sixty to a hundred and fifty times | **The default** |
| **A key.** Space, Enter, Page Down or the forward arrow, from anywhere on the screen | Free with the above. A page-turner pedal or a presenter's clicker sends these keys, on phones too, so feet work | Needs a keyboard or a pedal | **In, always on** |
| **A timer.** Turns arrive at a set pace: slow 6 s, steady 4 s, quick 2.5 s. A bar drains to show how long is left. Any tap or key pauses | Hands never leave the cube | It can get ahead of the person, and one missed turn spoils everything after it. Needs the pause to be unmissable | **In, as the option.** Off by default. Stops at every stage change |
| **Voice.** "Next", "back" | Hands and eyes free | No speech recognition that is on-device and the same on five platforms. Several would send audio away, against the privacy statement. Needs the microphone | **Out of the first release** |
| **The camera sees the turn** | The best answer: no input at all, and it would notice a wrong turn as it happens | The cube must stay in view while both hands hold it; a turn of the back changes nothing the camera can see square-on. It needs the free-handling reads of recovery plan R5 | **Later.** The layout leaves room: see "If the camera follows" below |

What the choice costs the layout: the tap needs the cube picture to take
no input of its own. The cube is not draggable on this screen.

## Layout

**Compact.**

1. **Bar**, 56 high: the pause button (56 x 56) at the leading edge; the
   stage as an eyebrow ("Stage 2 of 5 · White corners"), wrapping to two
   lines if it must; the turn count at the trailing edge in the mono face.
2. **Progress**, 6 high, edge to edge, with a tick at each stage boundary.
3. **The cube**, in all the height that is left (at least 140). The whole
   area is the tap target.
4. **The hold reminder**, one centred line in `small`: two swatches and
   `solve.holdLine`.
5. **The instruction block**.
6. **Two buttons**, 96 high, 16 from the edges and 8 clear of the safe
   area: **Back one** (one third) at the leading side, **Next turn** (two
   thirds) at the trailing side.

**Short.** Bar and progress as above. Beneath, two columns: the cube
leading (54%), and trailing the instruction block over the two buttons
(72 high). The hold reminder is dropped from the screen; it is in the
pause menu.

**Expanded.** Bar and progress across the full width. Beneath, two panes.
Leading: the cube, then the hold reminder. Trailing, 380 wide with a rule
on its leading edge: the **stage list** at the top, taking what height is
left and scrolling if it must; then the instruction block, centred; then
the two buttons. The main button shows the `Space` key name.

The instruction sits with the buttons on every size, so the eye goes from
what to do, to the cube, and back to one place.

At large text sizes the screen gives up, in this order: the stage
explanation, the hold reminder, then cube size down to 140. The
instruction and the two buttons are never dropped, shrunk or scrolled
away.

## Parts

| part | component | string | objectName |
|---|---|---|---|
| Pause | icon button, pause icon | `solve.a11y.menu` | `solve.pause` |
| Method | three buttons in a row, the chosen one primary; wide, a line under them says what it is. On a cube bigger than 3x3 a line above them reads "Finish with" | `solve.method.beginner`, `solve.method.intermediate`, `solve.method.advanced`, `solve.method.beginner.note`, `solve.method.intermediate.note`, `solve.method.advanced.note`, `solve.method.finishWith` | `solve.method` |
| Stage | eyebrow | `solve.stageOf`, `stage.1.title`, `stage.2.title`, `stage.3.title`, `stage.4.title`, `stage.5.title` | `solve.stage` |
| Turn count | `numeral`. A screen reader that moves to it hears the fuller `solve.a11y.progress` | `solve.progress`, `solve.a11y.progress` | `solve.count` |
| Progress | solve progress bar. Role: progress bar, value in turns | - | `solve.progress` |
| The cube | The solve lane's cube view. Role: graphic. Also the tap target for the state's main action | `solve.a11y.cube` | `solve.cube` |
| Hold reminder | Two swatches (yellow, green) and a line | `solve.holdLine` | `solve.holdLine` |
| Instruction block | instruction block; a polite live region whose text is the whole sentence | `side.*`, `dir.*`, `amount.*`, `turn.*`, `solve.a11y.notation` | `solve.instruction` |
| First-turn hint | A line on `accentTint` above the instruction | `solve.hint.touch`, `solve.hint.keys` | `solve.hint` |
| Next turn | button, primary, giant | `solve.next`, `solve.finish` | `solve.next` |
| Back one | button, secondary, giant | `solve.previous` | `solve.previous` |
| Skip to the 3x3 | button, secondary, above the two buttons; wide, with a line under it. Only on a cube of 4 or more that has not reached its 3x3 stages | `solve.skipToCore`, `solve.skipToCore.note` | `solve.skip` |
| Layer words | The move line of a layer inside a big cube, in words | `solve.move.layer`, `solve.move.layers` | `solve.move` |
| Stage list | Five stage rows (expanded only) | `stage.*` | `solve.stages` |
| Stage check | A panel on `accentTint` with a check, replacing the instruction | `solve.stageDone.title`, `solve.stageDone.check`, `solve.stageDone.next`, `stage.*` | `solve.stageCheck` |
| Carry on | button, primary, giant | `common.carryOn` | `solve.carryOn` |
| It does not match | button, secondary, giant | `solve.differs` | `solve.differs` |
| Countdown | countdown bar | - | `solve.countdown` |
| Pause (timer) | button, secondary, giant, pause icon | `solve.auto.tapToPause`, `solve.auto.keysToPause` | `solve.autoPause` |
| Menu | sheet | `solve.paused`, `common.carryOn`, `solve.menu.hold`, `solve.menu.lost`, `common.settings`, `solve.menu.leave`, `solve.menu.leaveNote` | `solve.menu` |
| Lost sheet | sheet | `solve.lost.title`, `solve.lost.body`, `solve.lost.step1`, `solve.lost.step2`, `solve.lost.step3`, `solve.lost.goBack`, `solve.lost.rescan`, `solve.lost.carryOn` | `solve.lostSheet` |
| Hold step | The cube, a title, a paragraph, one giant button and one small one | `solve.hold.title`, `solve.hold.body`, `solve.hold.match`, `solve.hold.ready`, `solve.hold.check`, `solve.holdResume.title`, `solve.holdResume.body`, `solve.matches` | `solve.hold` |
| Working | working indicator | `solve.working` | `solve.working` |
| Message | A title, a sentence and buttons | `solve.failed.title`, `solve.failed.body`, `solve.alreadySolved.title`, `solve.alreadySolved.body`, `scan.checkColours`, `common.scanAgain`, `complete.scanAnother`, `common.home` | `solve.message` |

**The method** is how the cube is being solved: Beginner (layer by layer,
five stages, about 100 turns), Intermediate (the way fast solvers do it, four
stages, about 50) or Advanced (the fewest, about 20, two stages that are not
meant to be learnt). It is chosen here and in settings; the choice is kept.
Choosing a different one while a solve is under way solves on from the cube
as it stands: whatever is already done stays done, the turn count starts
again for the new solution, and the way the cube is held is kept. The stages
shown are the chosen method's: `stage.1.*` to `stage.5.*` for Beginner,
`stage.1.*`, `stage.f2l.*`, `stage.4.*` and `stage.5.*` for Intermediate,
`stage.reduce.*` and `stage.finish.*` for Advanced.

**The instruction** is three lines (`../content.md`, "Naming a turn"): the
side with a swatch of its centre colour, the direction with its arrow,
the amount. For a turn of the back, "as seen from the front" follows the
amount. With notation on, the letters follow in a key-name box. The arrow
and the words *left* and *right* never mirror.

**In right-to-left layouts** the two buttons swap sides, and with them
which arrow key means next. The cube does not change.

**Keys** work from anywhere on the screen, with focus on nothing: Space,
Enter, Page Down or the forward arrow for the state's main action; Page
Up or the back arrow for **Back one**; P for the menu; H for how to hold;
Esc to leave.

**The screen stays awake** for as long as it is showing and the menu has
not been open for two minutes.

**Sound and haptics**: a soft tick as a turn is advanced; one note at a
stage change.

**Back** (the platform's, or Esc) leaves for Home and keeps the solve. It
asks nothing (brief, D5). The on-screen way to do the same is **Pause >
Leave this solve**: there is no back arrow on this screen, because a
96-high target and a small arrow should not sit together.

## The words the built screen has

The screen was built before these strings were written and says some things in
its own words. Each has a string of its own, with the English the screen showed,
so that the design's wording keeps its ids and nothing a person reads changed
when the words moved into the catalogue. Which wording wins is the owner's to
say: where it is the design's, the screen takes the id in the last column and
the string here goes.

| part | the screen says | strings | the design's, which the screen does not use |
|---|---|---|---|
| Hold reminder | "Hold yellow on top, green facing you", and "Holding ..." once the camera has seen how the cube is held. The colours are those of the centres on top and facing the person | `solve.holdLine.hold`, `solve.holdLine.holding`, `colour.word.*` | `solve.holdLine` |
| Stage names 4 and 5, and what each stage asks | "Orient last layer", "Permute last layer", and a sentence of its own for every stage of the Beginner method | `solve.stage.4.title`, `solve.stage.5.title`, `solve.stage.1.detail`, `solve.stage.2.detail`, `solve.stage.3.detail`, `solve.stage.4.detail`, `solve.stage.5.detail` | `stage.4.title`, `stage.5.title`, `stage.1.what` to `stage.5.what` |
| Stage headings | "STAGES" over the wide layout's list, "STAGE 1 OF 5" over the move: the capitals are the app's | `solve.stages`, `solve.stageOf` | |
| The move in words | "Left side" and "Right side" (with the word *side*), "quarter turn" and "half turn" (lower case, after a dot) | `solve.place.left`, `solve.place.right`, `solve.move.quarter`, `solve.move.half` | `side.left`, `side.right`, `amount.quarter`, `amount.half` |
| What the camera says | The words under the move label | `solve.camera.matched`, `solve.camera.sideHidden`, `solve.camera.matchedMove`, `solve.camera.unsure`, `solve.camera.wrongMove`, `solve.camera.wrongSide`, `solve.camera.showAgain`, `solve.camera.holdAny` | `solve.camera.layerHidden` is the one string the design had |
| When the cube does not match | A note with a title, a sentence that names the side by the colour of its centre and, when it is not the side the move was on, where that side is, and a button | `solve.mistake.title.move`, `solve.mistake.title.side`, `solve.mistake.wayWrong`, `solve.mistake.sideWrong`, `solve.mistake.unknown`, `solve.mistake.rescan`, `solve.theSide.*`, `solve.mistake.place.*` | `solve.lost.*` |
| A view that has turned | A note while the picture has swung round behind the cube | `solve.view.behind`, `solve.view.note` | |
| Stepping back, and the way out | **Previous**; **Out of step? Rescan** | `solution.previous`, `solve.rescan` | `solve.previous`, `solve.lost.rescan` |
| The rest | The hint on the cube; the spinner's word | `solve.hint.drag`, `solve.solving` | `solve.working` |

## States

### `solve.working`

The solution is not ready when the screen opens. The working indicator
and `solve.working`, centred. Normally never seen: the scan screen holds
its "Cube read" state until the solution is ready.

### `solve.failed`

No solution for a cube that was accepted. Should not happen; if it does
it is said plainly: `solve.failed.title`, `solve.failed.body`.

- **Check the colours** (primary): to `review.valid`.
- **Scan again**: to `scan.starting`.
- **Home**.

### `solve.already-solved`

The cube that was read is solved: `solve.alreadySolved.title`,
`solve.alreadySolved.body`.

- **Scan another cube** (primary): to the scan.
- **Home**.

### `solve.hold`

Before the first turn. This is the state the app lacked on phones: every
turn is named for a side *as held this way*, and nothing said so.

- A top bar with back only. Compact: the cube as scanned in the upper
  part, the text beneath. Expanded: the cube in the leading pane, the text
  in a `surface` pane.
- `solve.hold.title`; `solve.hold.body`; then `solve.hold.match` in
  `inkSecondary`. The picture is the person's own cube, so its pattern can
  be matched even by someone who cannot tell the colours apart.
- **I am holding it like this**: giant, primary, the only large target.
  To `solve.move` (or `solve.move-hint` the first time, or `solve.auto`).
- **Something looks wrong? Check the colours**: ghost. To `review.valid`.
- Back: to Home; the solve is kept (`home.resume`).

### `solve.hold-resume`

The same step, entered by **Carry on**.

- Title `solve.holdResume.title` ("Carry on from turn 24"); body
  `solve.holdResume.body`. The cube shown is as it should be at that turn.
- **It matches** (giant, primary): to `solve.move` at that turn.
- **It does not match** (thumb, secondary): to `solve.lost`.

### `solve.move`

One turn is shown; the app waits.

- On the cube, the layer that turns leans the way it must go, and eases
  back, about every two seconds. With reduced motion it is drawn leaning
  and still.
- The instruction block names the turn.
- **Next turn**, a tap on the cube, or a key: to `solve.turning`.
- **Back one** (disabled at the first turn): plays the last turn
  backwards and shows it again.
- Pause: to `solve.menu`.

### `solve.move-hint`

`solve.move` for the first turn of the first solve ever made on this
device. One extra line above the instruction, on `accentTint`:
`solve.hint.touch` on a touch screen, `solve.hint.keys` with a keyboard.
It goes when the first turn is advanced and never returns.

### `solve.turning`

The turn plays on the cube: 380 ms; nothing with reduced motion. The
buttons stay live: a second press finishes this turn at once and starts
the next, so a quick person is never made to wait.

Then, by itself: `solve.move`; or `solve.stage-done` if that was the last
turn of a stage; or `solve.last-move`; or `complete.default`.

A screen reader hears the new turn's sentence when it lands.

### `solve.stage-done`

Between stages. A deliberate stop, for two reasons: it is where a wrong
turn can be caught before it costs the whole solve, and it is where
someone learning sees what the stage achieved.

- The cube stands as it should now, with nothing leaning.
- In place of the instruction, a panel on `accentTint`: a check and
  `solve.stageDone.title` ("White cross done"); `solve.stageDone.check` in
  bold; `solve.stageDone.next` ("Next: White corners") followed, when
  **Explain each stage** is on, by that stage's what and why.
- **Carry on** (giant, primary; also a tap on the cube or a key): to
  `solve.move` or `solve.auto`.
- **It does not match** (giant, secondary, equal width): to `solve.lost`.
- Timer mode stops here too and waits.

A person who cannot see the cube's colours cannot make the comparison.
One key (Space) carries on; nothing forces the check.

### `solve.last-move`

The last turn is shown. As `solve.move`, with the main button reading
`solve.finish`. Advancing plays the turn and goes to `complete.default`
with no further tap.

### `solve.auto`

Timer mode (Settings > Solving > Move to the next turn > On a timer).

- The instruction block as usual. Beneath it the **countdown**, draining
  over the pace; when it empties the turn plays and the next is shown.
- One full-width giant button: `solve.auto.tapToPause` (touch) or
  `solve.auto.keysToPause` (keyboard), with a pause icon. A tap anywhere
  on the cube, or any of the usual keys, also pauses.
- Stops at `solve.stage-done` and at the end.
- With reduced motion the countdown is replaced by the seconds left as a
  number.

### `solve.auto-paused`

Timer mode, paused. Also where the app returns after being in the
background: it never resumes by itself.

- The countdown is full and still.
- **Carry on** (giant, primary) and **Back one** (giant, secondary).

### `solve.menu`

The pause menu. A sheet; the solve waits behind it.

1. **Carry on** (primary).
2. **How to hold the cube**: to `help.holding`.
3. **My cube does not match**: to `solve.lost`.
4. **Settings**: to `settings.main`; back returns here.
5. **Leave this solve** (ghost), with `solve.menu.leaveNote` beneath it:
   to Home, the solve kept.

Hands are off the cube when this is open, so its rows are the ordinary 56
high.

### `solve.lost`

The cube and the picture disagree. A sheet: `solve.lost.title`,
`solve.lost.body`, then three numbered things to try, in order of cost.

- **Go back one turn**: closes the sheet and plays the last turn
  backwards.
- **Scan the cube again** (primary): to the scan. The new scan replaces
  this solve; the solver starts from wherever the cube now is. This is
  the way out that always works.
- **Carry on anyway** (ghost): closes the sheet.

## Interruptions

- **Background, lock, lid closed:** the place is kept. Timer mode comes
  back paused.
- **The app is closed or killed:** the scanned cube and the turn reached
  are stored as each turn is advanced, so the next launch shows
  `home.resume`.
- **Rotation or a resize:** the layout changes; the turn, the state and
  the cube's view do not.

## If the camera follows (later)

**Built since** (0.1.0-beta.1, and at every size from 0.2.0-beta.1): the camera
view is in the screen (`solve.camera`, with a dot on each sticker found, mirrored
like the scan screen), it presses **Next turn** when it sees the turn made, and
it says when it cannot. For a layer that the side in view does not show it says
`solve.camera.layerHidden` and waits for **Next turn**. The paragraph below is the
original proposal.

When turns can be seen by the camera, the expanded layout's trailing pane
and the compact layout's hold-reminder line are where a small camera
picture goes, with a tick or a "that was the wrong way" beside it. The
two buttons stay: the camera then presses **Next turn** for the person,
visibly, and they can still press it themselves. Nothing else moves.

## Needs from the solve lane

1. **No input on the cube view** on this screen, so that a tap on it is
   the screen's main action. (As built, `SolveScreen.qml` sets
   `interactive: true`: the cube can be dragged to look round it.)
2. **Never mirrored.** The view sets `LayoutMirroring.enabled: false` on
   itself.
3. **A still form of the lean** for reduced motion, and a turn that snaps
   when asked to.
4. **The turn-finished signal** (`movePlayed`, already there) and the
   ability to finish a turn in flight at once (`finishPlaying`, already
   there).
5. **Marks on stickers**, from `Theme`, on by default, switchable. If the
   3D view cannot draw them, say so: the swatch beside the side's name
   then carries the mark alone.
6. **Sticker colours from the new tokens** (`../tokens.json`, `cube`).
   They are retuned; the old `faceU`..`faceR` names will point at them.
7. **Jumping to a turn**: restoring a stored cube at a given turn, for
   **Carry on** and for the solution screen. The solver is deterministic,
   so storing the scanned cube and an index is enough.
8. **The stage a turn belongs to**, and where each stage ends
   (`stageRanges`, already there).
9. **A text description of the cube** for screen readers
   (`SolveController.stickers` already carries what is needed).
10. **Where it navigates.** *(Written when nothing went to Complete and
    nothing left; both are done: Finish goes to Complete and Rescan leaves.)*
    After the last turn the shell needs one signal; it does the rest.
11. The mock "AUTO" box and the "orthographic" captions go. *(Done.)*
