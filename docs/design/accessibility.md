# Accessibility

> [!NOTE]
> **Status in 0.2.0-beta.4 plus the accessibility work after it:** Part built.
> Every control of every screen has a role and a name, can be reached with Tab
> and pressed with Enter or Space, and shows a focus ring; each screen is a named
> pane with a heading; what changes by itself (the scan's guidance, the next
> turn, an error) is announced; the whole journey is walked by keyboard in a
> test; and the app follows the device's request for less motion, with a Motion
> row in Settings. The rest of the page is still the specification. All of it was
> tried against Qt's own accessibility tree and events in the system tests, and
> **none of it with a screen reader**: whether Narrator, NVDA or VoiceOver read
> it well is not known.

## What the app has today

Checked against `src/app/qt`, `tests/system/accessibility_test.cpp` and
`CHANGELOG.md`. Everything not listed as built is design.

### Built

- **Names and roles (`Accessible`), everywhere.** `AppButton` is a button, or a
  radio button when it is one of a set of choices (a chip, a method, a size,
  a side: `checkable` and `checked`), named by its own label so that a
  translation carries over, with the disabled state exposed (it is the item's
  own `enabled`; it used to shadow it with a property of its own). Switches in
  Settings are check boxes; the rows of About, Licences and History are buttons
  (links where they leave the app); a colour of the palette is a radio button;
  a turn of the solution is a list item that can be pressed. Icons and arrows
  are ignored: the control that holds them carries the name. Each screen is a
  pane named by its title, and its title, its eyebrow or its headline is a
  heading. A set of choices is a group named by its section. The 3D cube on the
  solve screen is a graphic named "The cube as it should look now" with the turn
  in words; the scan's picture is "Camera picture" and each of its six chips says
  which side it is and whether it is read. The home, settings and finishing
  cubes are decoration and not listed. The test
  (`every control on every screen has a name and a role`) walks Qt's
  accessibility tree - the one every platform's reader is built on - on each
  screen in both layouts and fails on a control that is not in it with a role
  that can be operated and a name that is not an id.
- **Keyboard.** Every button, choice, switch, list row, history row and delete
  button, the palette and the colours screen's side grid is a Tab stop
  (`activeFocusOnTab`) and is pressed with Enter or Space; Shift+Tab goes back.
  A click or tap does not take focus, so the solve screen's keys still act on
  the screen after a button was clicked. On the colours screen the grid is one
  Tab stop: the arrow keys move a cursor over the stickers, Space and Enter
  paint, Backspace and Delete unset, 1 to 6 choose the colour, Page Up and Page
  Down change side. A dialog holds Tab inside itself, and the scramble box lets
  Tab leave. Focus that arrives below the fold of a list is scrolled into view.
  Esc goes back on Settings (to where it was opened from), History, the
  solution, About, Licences, *Another size of cube*, the colours screen and a
  dialog (its safe choice), is **Not now** on the camera permission screen and
  **Cancel** on the scan screen (it does not ask first). The solve screen steps
  on Space, Enter and the right arrow, and back on the left arrow; the solution
  on the arrow keys (swapped in right-to-left languages), Space, Home and End;
  the colours screen has the platform's Undo and Redo. The desktop shell has
  Ctrl+, (Settings), Ctrl+N (a new scan, from Home or Complete), F11 or
  Ctrl+Cmd+F (full screen) and Ctrl+Q, and on a Mac Cmd+W and Cmd+M;
  Ctrl+Shift+D and Ctrl+Shift+S are for developers. F1 (Help) and Shift+Ctrl+N
  (enter by hand) are bound and disabled, because there is no Help and no Home
  entry for them to open. `tests/system/accessibility_test.cpp` walks Tab round
  every screen and the whole journey (scan, solve, finish, review, scan again)
  with the keyboard alone, in both layouts.
- **Focus ring.** Two bands inside the edge of the control, so that no list that
  clips can cut them: 2 px of `ink`, then 2 px of the page's colour (the design
  has the ring outside the control; most of the controls sit in lists and
  columns that clip, so it is drawn on the control's own edge). Over the camera
  picture, on the scan screen, it is `overlay.ink` and the overlay panel's
  colour, so that one band shows against any picture. The pairs are the ones
  `tokens.json` already measures (`ink` on `paper`), and `check_tokens.py` still
  passes. `FocusRing.qml` is the one place it is drawn.
- **Announcements.** `Announcer.qml` says something to a screen reader through
  `Accessible.announce` (Qt 6.8's `QAccessibleAnnouncementEvent`: UI Automation's
  notification event on Windows; the platform's announcement notification on
  macOS and iOS) without moving focus, politely unless it says otherwise, after
  the words have stopped changing for 300 ms and never the same words twice
  within two seconds. It is used for: the scan's headline and guidance, a
  camera that will not start (assertive), the turn to make on the solve screen
  with its place in the solve and, when it changes, the stage; what the camera
  makes of the cube there; the solution's turn as it moves; what is wrong with a
  typed scramble (assertive); the colours screen's verdict when the cube is
  valid, miscounted or impossible; the camera permission being refused; and
  "Solve deleted" and "History cleared". The tests listen with
  `QAccessible::installUpdateHandler` and read what was said, for the scan, the
  camera failure, the solve's turn, the solution's turn, the scramble's error
  and the history; what the camera makes of the cube, the permission refusal
  and the colours screen's verdict are said the same way and have no test of
  their own.
- **Light, dark and the device's own scheme**, chosen in Settings and followed
  live.
- **A switched-off button can be read.** It is not faded. `AppButton` draws it
  as an opaque `surface` plate with an `outline` edge and its label and icons in
  `inkDisabled`, in every look it has: a secondary button, a chip, the chosen
  chip, a primary, a ghost, and any of them over the camera. The label is
  3.15:1 on the plate in the light theme and 3.57:1 in the dark, and the edge
  3.41:1 and 3.95:1 on the page. The plate is opaque, so the label is measured
  on it and never on the camera picture behind the scan screen's buttons, which
  cannot be measured. The chosen one of a set (the speed chip that is selected
  while Auto-rotate is off) has a second ring inside its edge, so that which is
  chosen is not told by colour alone; a screen reader still says it is checked,
  and says the button is disabled. Fading the whole button was tried first
  (`Theme.disabledOpacity`, 0.45 and then 0.6) and does not work: the label and
  the plate fade towards the page together, which left a filled button's label
  at 1.7:1 and a ghost's red at 2.4:1. `check_tokens.py` measures every look and
  holds `Theme.qml` and `AppButton.qml` to the tokens, `tests/unit/disabled_contrast_test.cpp`
  measures the real button in both themes and the scan screen's own Start over,
  and `tests/system/settings_test.cpp` reads the speed chips on the Settings
  screen.
- **Reduced motion.** `Theme.reducedMotion` is on when Settings > Appearance >
  Motion says **Reduced**, or says **Same as this device** (the default) and the
  device asks (`Qt.styleHints.accessibility.motionPreference`, new in Qt 6.12,
  which each platform plugin fills in from its own setting - Windows "Show
  animations", the macOS and iOS "Reduce motion", the browser's
  `prefers-reduced-motion` - or does not; which of them do was not checked, so
  the row in Settings is the way that always works). Then the
  home and settings cube does not spin; the layer to turn is drawn leaning, still,
  and a turn snaps; the cube's view does not swing to look at the back; the
  finishing cube does not bob and the underline under "Solved" is drawn whole; a
  chip does not pop and six do not ripple; the camera preview on the solve screen
  does not glide and does not flash when a move is seen; the working spinner
  stands still; screens fade in 120 ms and not 280; the solution plays more
  slowly (it had that already). Changing the setting changes a screen that is
  on show. `Same as this device` is the default, so a device that has not asked
  for less sees no change; the saved key is `appearance/motion`. There is no
  "shake" in the app to still.
- **Right to left.** Hebrew, Arabic and Yiddish mirror the layout; the cube, the
  camera picture and the nine stickers of a side opt out of mirroring; the
  solution's arrow keys swap. The i18n screen tests check the mirroring.
- **A "?" on a sticker the camera doubted**, on the colours screen, so that the
  doubt does not depend on telling colours apart (it is a mark; the cube colours
  themselves carry none).

### Designed, not built

- **Keys:** Alt+Left, Cmd+[ and the mouse back button; the scan keys R, M and H;
  Ctrl+Enter on the colours screen; P, H, Page Up and Page Down on the solve
  screen; Esc on Solve, Complete and Home (Solve has no "leave" to say it with:
  its only way out is the rescan button).
- **Marks on the cube colours**, and the retuned swatches: `Theme.qml` still
  draws the old six (orange and green 5.4 apart under protanopia), and the dots
  on the scan screen are still yellow, then green. `settings.marks` is a string
  with nothing behind it.
- **Text size.** Sizes are pixels in the QML. Nothing reads the system's text
  size, and Settings has no row for it; nothing tests a larger text size. Qt 6.12
  has no cross-platform text scale to read (`QStyleHints` and
  `QAccessibilityHints` give the colour scheme, the contrast preference and the
  motion preference only), and nothing in the Windows plugin refers to the
  "Text size" setting. What a phone's own scale does to `QGuiApplication::font()`
  is not used either: every size in the screens is a pixel count that would have
  to be multiplied (127 of them across the screens and widgets when this was written).
- **The contrast gate on the screens.** `check_tokens.py` passes for
  `tokens.json`, but `Theme.qml` is still the legacy palette (the brand red
  `#EC3013` as text, `inkMuted(0.55)`), which the design says misses 4.5:1. The
  figures in this page describe the tokens, not what the screens draw. A
  switched-off button is the exception: it is drawn in the tokens' own colours
  (see Built).
- **Describing the cube, reading turns aloud, durations spoken as durations**
  (the solved screen says "62 turns · 12:40"), **the chips read out as nine
  colours**, **a quarter-turn's direction by side** (the solve screen says
  clockwise or anticlockwise as the person looks at the side, not "to the
  left").
- **Target sizes of 48 px** are not measured; many buttons are 30 to 44 high.

### Not tried

- **No screen reader** (Narrator, NVDA, VoiceOver, TalkBack, Orca) has been run
  against the app, as far as the repository records. What the tests show is
  that Qt's accessibility tree has the right names, roles, states and focus and
  that the announcement events are raised; how a reader speaks them, whether
  the announcement events are turned into speech on every platform, and whether
  `Accessible` on the custom items is read well on a phone are not known. The
  iPhone build has not been tried by anyone but its author.
- **Focus on entering a screen.** Each screen's root takes focus when it appears
  and is a named pane, so that a reader starts on the title; whether a reader
  announces the pane's name on that is a reader's behaviour and has not been
  seen.
- **The people tests** the page names (five people who cannot solve a cube, one
  of them with a colour vision deficiency) are E6; nothing in the repository
  records them.

## The target

**WCAG 2.2 level AA is the reference.** It is written for the web;
Cubeulator is a native app, so each criterion is read the way WCAG2ICT
reads it for software. Where a platform asks for more (48 px targets on
Android), the stricter rule is the kit's rule everywhere.

The app on 2026-10-13 had no accessible names, no keyboard focus and state
carried by colour alone (plan, "What was found"). Of the three, names and
focus now exist on every screen, and the dots on the scan screen still change
by colour alone ("What the app has today", above). So this is the design for
the first accessible version, not a refinement.

What "done" means, and who checks it:

| check | by |
|---|---|
| Contrast of every token pairing | `docs/design/tools/check_tokens.py`, on every change |
| Every interactive item has a name, a role and a big enough target | System tests (E0a), walking the journey |
| The whole journey by keyboard alone | System tests |
| Nothing truncated at 200% text or in the pseudo-locale | System tests, from the screenshot tour |
| A real screen reader on each OS | A person, in E6. Not automatable |
| Five people who cannot solve a cube, one of them with a colour vision deficiency if at all possible | E6 |

## The criteria that shape the design

| criterion | how it is met |
|---|---|
| 1.1.1 Non-text content | The 3D cube, the camera picture, every icon, chip and sticker has a text alternative (below) |
| 1.3.1 Info and relationships | Headings are headings; a group of choices is a group with a name; the stage list is a list |
| 1.3.4 Orientation | Nothing is locked to portrait or landscape (`platforms.md`) |
| 1.4.1 Use of colour | Cube colours carry marks and names. No chrome state is colour alone (`system.md`) |
| 1.4.3 Contrast, 1.4.11 Non-text contrast | 4.5:1 for all text, 3:1 for control edges, icons and marks. Measured |
| 1.4.4 Resize text | Layouts hold at a text scale of 2.0 |
| 1.4.10 Reflow | One column at 320 wide; no sideways scrolling |
| 1.4.12 Text spacing | Text boxes grow with their text; nothing has a fixed height |
| 2.1.1 Keyboard, 2.1.2 No trap | Everything is reachable and leavable by keyboard. Dialogs hold focus and release it on close |
| 2.2.1 Timing adjustable | The only timer is the optional paced solve. It is off by default, has three paces, and any key or tap pauses it. The 10-second scan hint is advice, not a limit |
| 2.2.2 Pause, stop, hide | No animation loops beyond five seconds except the lean of the next layer, which reduced motion stops |
| 2.3.3 Animation from interactions | Reduced motion (below) |
| 2.4.3 Focus order | Reading order: leading to trailing, top to bottom, with the primary action last in its group |
| 2.4.7 Focus visible, 2.4.11 Focus not obscured | The focus ring; sheets and dialogs never cover the focused item |
| 2.5.1 Pointer gestures, 2.5.7 Dragging | No gesture is required. Every action is a single tap or key. Dragging the home cube is decoration |
| 2.5.8 Target size | 48 px minimum; 56 on scan; 96 on solve |
| 3.1.1 Language, 3.1.2 Language of parts | The app's locale is set on the window. Notation is marked as not-language so it is spelt, not pronounced |
| 3.2.x Predictable | Focus never moves by itself except to a dialog. A setting applies when it is changed, and says so |
| 3.3.1 Error identification, 3.3.3 Suggestion | Every failure state names what happened and offers the next step (`journey.md`) |
| 4.1.2 Name, role, value | Every kit component sets `Accessible.role`, `Accessible.name` and, where it has one, its value or checked state |
| 4.1.3 Status messages | Live announcements, listed per screen below |

## Colour vision

About one man in twelve and one woman in two hundred has a colour vision
deficiency, nearly always on the red-green axis. A cube's red, orange and
green sit on it.

**Measured, not assumed.** `check_tokens.py` simulates protanopia,
deuteranopia and tritanopia and no colour at all (Machado, Oliveira and
Fernandes, 2009) and measures how far apart each pair of the six lands.
The table is in `system.md`. With the app's colours as they are today,
orange and green are 5.4 apart under protanopia and red and green 6.5
under deuteranopia, where about 10 is the least that can be relied on.

The treatment has three layers. Each works without the others.

1. **Retuned swatches.** The closest pair under any dichromacy is now 11.7
   (in `tokens.json`; the app has not ported them).
2. **A mark on every colour**, on by default: ring, slash, plus, dot, bar,
   and plain for white. Shown on the drawn cube, the scan chips, the
   colour picker, the review grid and every in-line swatch. With no colour
   at all, the six are still six.
3. **The word.** Wherever one colour is meant, it is named: "Yellow centre
   on top", "Too many: red (10)".

What the app cannot fix is the cube in the person's hand. So no
instruction depends on recognising a colour on the real cube, with one
exception: the hold. "Yellow centre on top, green centre facing you" asks
for two colours. It is backed by the picture of *their* cube as scanned:
the pattern of light and dark on the top and front can be matched even
when the hues cannot. E6 should test this step with someone who has a
red-green deficiency.

Chrome states that used to be colour alone, and what they are now:

| where | was | is |
|---|---|---|
| Dots on the side in view | yellow, then green | A hollow ring, then a filled disc; and the headline changes |
| Chip for a side in view again | a lighter border | A 3 px border and a pointer above the chip |
| Chips when the scan completes | green borders | A 3 px border and a check in each corner; and "Cube read" |
| Conflict and camera trouble | orange text | A triangle icon, the heading, and a button that fixes it |
| Stage done, current, to come | opacity and a red box | A check; a filled box and a tinted row; an outlined box |
| A switch | - | Thumb position and a check |
| A chosen option | red fill | Inverted fill and a check |

## Screen readers

Narrator and NVDA on Windows, VoiceOver on macOS and iOS, TalkBack on
Android, Orca on Linux. All reach Qt Quick through `Accessible`.

Rules for every screen:

- **Entering a screen** moves screen-reader focus to its title, which is a
  heading. The title is what is announced.
- **Order** follows reading order. In a two-pane layout, the pane with the
  instruction comes before the pane with the picture.
- **A change the person did not cause** is announced through a live region:
  `polite` for progress, `assertive` only for something that stops them.
  Qt 6.8 adds `Accessible.announce`; E2a confirms it reaches each
  platform's reader and wraps it in one kit function.
- **Decorative things are ignored** (`Accessible.ignored`): rules, the
  home cube, the underline on "Solved".
- **Marks are not read as shapes.** A sticker's name is its colour.

### Things that are opaque, and what stands in for them

| thing | why it is opaque | text alternative |
|---|---|---|
| The 3D cube on the solve screen | One GPU surface | The instruction block is the alternative: it carries the same turn in words. The cube item's own name is "The cube as it should look now", with the turn sentence as its description. A **Describe the cube** action reads the nine colours of the top, front and right sides, row by row. Needs a list of stickers from the solve lane, which `SolveController.stickers` already gives |
| The camera picture | Live video | Name: "Camera picture". Everything the picture tells a sighted person is also in the status panel, which is live |
| The dots on the stickers in view | Drawn on the picture | Nothing of their own. "A side is in view" is announced once; "Reading this side" follows from the status panel |
| A side chip | Nine coloured squares | "Side 3: read. Red side." Activating it reads the nine colours row by row |
| A canvas-drawn icon | A painted item | Icons become `Shape` items and are ignored; the control they sit in carries the name |
| A direction arrow | An icon | Ignored; the direction is in the words beside it |

### What is announced, screen by screen

| screen | on entry | live announcements |
|---|---|---|
| home | "Cubeulator. Scan. Solve. In your hands." then the lede | "No camera found" when that notice appears |
| first-run | The headline | - |
| permission | "Camera access" | After the system prompt: the new title, "Camera access is off", if refused |
| scan | The status headline, then the guidance | Each change of headline or guidance, politely, with repeats within two seconds dropped. "A side is in view." "Side 3 of 6 read." "Cube read." A camera failure is assertive |
| review | The title, then the status line | The status line when validity changes: "This is a valid cube", "Too many: red (10)". Not on every sticker |
| solve | The hold headline and body; then, on the first turn, the turn sentence | Each new turn: "Turn the top a quarter turn to the left." Then, only if asked for (a second action on the instruction), "Turn 12 of 62. White corners, stage 2 of 5." Stage changes: "White cross done. Check your cube matches the picture. Next: White corners." In timer mode the turn is announced as it appears |
| complete | "Solved. 62 turns. 12 minutes 40 seconds." | - |
| solution | The title and summary | The turn sentence when the selected turn changes |
| history | The title, then how many entries | "Deleted." |
| settings | The title | The new value when a setting is changed by a key |
| about | The title | "Copied", after either copy |
| licences, privacy, help | The title | - |

Durations are spoken as durations ("12 minutes 40 seconds"), not as
"twelve forty". Notation, when on, is spelt out letter by letter and
"prime" is said for the apostrophe; it follows the plain sentence, never
replaces it.

### The solve screen without sight

It is possible to follow the solve by ear: hold as told, then one sentence
a turn, advancing with Space or a double tap. Two things make it workable
and both are proposals:

- **Read turns aloud**, using the platform's own speech, so that a screen
  reader is not needed just for this and a sighted person with their eyes
  on the cube gets it too. **Out of the first release** *(the maintainer,
  2026-10-13)*. Until it exists, following by ear needs the platform's
  screen reader, which reads the same sentences.
- **Describe the cube**, for checking by touch-free comparison at a stage
  change. A person who cannot see the cube's colours cannot make that
  comparison at all; for them the stage check is skipped by one key.

The scan cannot be done without sight today: nothing says which way to
tilt the cube. That is out of scope for the first release and is said
here so that it is not claimed.

## Keyboard

Every screen works with a keyboard and nothing else. This is also how a
switch, a page-turner pedal or a presenter's clicker drives the app.

### Everywhere

| key | does |
|---|---|
| Tab, Shift+Tab | Next, previous control. Order is reading order |
| Space, Enter | Activate the focused control |
| Arrow keys | Move within a group: a choice, the colour picker, a list, the review grid |
| Esc | Back. In a dialog or sheet: close it, choosing the safe option |
| Alt+Left (Windows, Linux), Cmd+[ (macOS) | Back |
| F1 (Windows, Linux), Cmd+? (macOS) | Help |
| Ctrl+, / Cmd+, | Settings |
| Ctrl+N / Cmd+N | Scan a cube, from Home |
| F11 / Ctrl+Cmd+F | Full screen |
| Ctrl+Q / Cmd+Q | Quit |

In right-to-left layouts the arrow keys that mean "forward" and "back"
swap, as the layout does. Arrow keys that move across the review grid do
not: the grid is the cube.

### Scan

| key | does |
|---|---|
| Esc | Cancel (asks first if any side is read) |
| R | Start over (asks first) |
| M | More |
| H | Enter the rest by hand |

### Review and hand entry

| key | does |
|---|---|
| 1-6 | Choose the colour to paint with, in the order shown |
| Arrow keys | Move the cursor across the stickers of the side |
| Space, Enter | Paint the sticker under the cursor |
| Backspace, Delete | Unset it |
| Page Down, Page Up | Next, previous side |
| Ctrl+Enter / Cmd+Enter | Solve, when the cube is valid |

### Solve

Focus does not need to be on anything. These work from anywhere on the
screen, so that a knuckle on the space bar is enough.

| key | does |
|---|---|
| Space, Enter, Page Down, forward arrow | Next turn. On the hold step: start. At a stage change: carry on. In timer mode: pause, or carry on |
| Page Up, back arrow | Previous |
| Esc | Leave. The solve is kept |
| P | Pause: opens the menu |
| H | How to hold the cube |

Space and Enter act on a focused button instead when focus is on one. The
main button shows `Space` as a reminder on desktop.

### Solution

| key | does |
|---|---|
| Forward arrow, back arrow | Next, previous turn |
| Space | Play, pause |
| Home, End | First turn, solved |

## Text size

- The type scale is in rem. One rem is 16 px times `Theme.textScale`.
- `textScale` is the operating system's text size times the app's own
  setting (1, 1.25 or 1.5), clamped to 0.85-2.0. How each platform's
  setting is read is in `platforms.md`.
- Controls grow with their labels. Nothing has a fixed height.
- At large scales the solve screen gives up, in order: the stage
  explanation, the hold reminder (it stays in the menu), then cube size
  down to 140 px. The instruction and the two buttons are never dropped
  or shrunk.
- At 2.0 on a compact screen some screens scroll. None scrolls sideways.

## Reduced motion

Follows the operating system, with an override in Settings. The full
table of what changes is in `system.md` under Motion. In short: nothing
moves except by the person's own action, and a turn snaps instead of
sweeping. The direction of a turn is then carried by the still lean of
the layer, the arrow icon and the words.

## Right to left

Hebrew, Arabic and Yiddish ship in the first release, so this is tested,
not planned.

**Mirrors:** the order of things across the screen; leading and trailing
margins; which side the back control, the chevrons and the primary action
are on; progress bars and the countdown; the order of the six scan chips
(a sequence); the order of the solve screen's two buttons, and with them
which arrow key means "next".

**Never mirrors:**

- the 3D cube and anything it does;
- the camera picture;
- the nine stickers inside a chip, a side in the review screen, or the
  unfolded cube;
- direction arrows, and the words *left*, *right*, *clockwise*,
  *anticlockwise*: they describe the object in the hand;
- notation, key names, version numbers and times: always left to right;
- the six swatches of the colour picker keep their order and their 1-6
  keys.

The kit enforces the second list: the cube view, the chip and the
direction icon set `LayoutMirroring.enabled: false` on themselves and
their children. A screen cannot mirror them by accident.

Numbers are formatted by the locale. Arabic may show Arabic-Indic digits;
notation never does.

## What is not known

- Whether `Accessible` on custom Qt Quick items is read well by TalkBack
  and VoiceOver on phones. Desktop is the safer ground. E2a builds the
  button first and tries it on all five before building the rest.
- Whether `Accessible.announce` exists and behaves the same on all five
  in Qt 6.8.
- Whether the hold step works for someone who cannot tell green from
  orange on their own cube. E6.
- Whether the marks help or distract people with ordinary colour vision.
  They are a switch, so the default can change after E6.
