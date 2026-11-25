---
template: marketing.html
layout: prose
title: Accessibility
title_tag: "Accessibility: what Cubeulator supports today, and what it does not"
description: "What Cubeulator supports for keyboard use, screen readers, reduced motion, colour, text size and language, what it does not support yet, and how to tell us about a barrier."
---
# Accessibility

Cubeulator can be used from the keyboard alone, tells a screen reader what is on each screen, and moves less when you
ask it to. This page says what works, what does not yet, and how to tell us about a barrier. The app was designed for
someone who can see the cube and hold it, and the camera scan still needs sight. The target is WCAG 2.2 level AA; the
app has not reached it, and nothing here claims that it has.

The names, roles and announcements a screen reader is given are built into the app and checked automatically on every
change. Nobody using NVDA, Narrator, VoiceOver or another screen reader has told us how it sounds yet, so if you use
one, please [tell us](#tell-me-about-a-barrier) what you find.

## What is there

- **Keyboard.** Every button, choice and switch, and the rows of History, About and Licences, can be reached with
  <kbd>Tab</kbd> (<kbd>Shift</kbd>+<kbd>Tab</kbd> goes back) and pressed with <kbd>Enter</kbd> or <kbd>Space</kbd>, and a
  ring of two bands shows where you are. The whole journey works with the keyboard alone: start a scan, step through the solve, finish, review the
  solution and scan again. <kbd>Esc</kbd> goes back on Settings, history, the solution, About, Licences, *Another size of cube* and the colours
  screen, is **Not now** on the camera permission screen and **Cancel** on the scan screen, and chooses the safe answer in a
  question that asks before it deletes. A click or a tap does not move keyboard focus, so the solve screen's keys
  (<kbd>→</kbd>, <kbd>Space</kbd> or <kbd>Enter</kbd> for the next move, <kbd>←</kbd> for the one before) work whichever
  button you last clicked. On the colours screen the stickers of a side are one stop for <kbd>Tab</kbd>: the arrow keys move
  over them, <kbd>Space</kbd> paints, <kbd>Backspace</kbd> clears, <kbd>1</kbd> to <kbd>6</kbd> choose the colour, and
  <kbd>Page&nbsp;Up</kbd> and <kbd>Page&nbsp;Down</kbd> change side. The [full list](guide/settings.md#keyboard) is in the guide.
- **Names, roles and headings for a screen reader.** Each screen has a name and a heading. Buttons say they are buttons and
  are named by what they say on screen, so a translation is read in its own language. A choice (a theme, a size of cube, a
  method, a side) says whether it is the one that is on; a switch says whether it is on; a button that cannot be pressed
  now says so. The cube on the solve screen is described by the move to make, and the camera picture, the six side chips
  and the colour palette are named. Icons are not read out.
- **Changes are announced.** On the scan screen the headline and the guidance are spoken as they change, and a camera that
  will not start interrupts. On the solve screen each move is spoken with its place in the solve (and the stage, when it
  changes). The solution says where you are as you step through it. A mistake in a typed scramble interrupts, and the
  history says when a solve is deleted. How a reader speaks these announcements depends on the reader and your system.
- **Less motion.** The app follows your device's request for less motion, and Settings has a **Motion** row that always works: the same as the device (the default), reduced, or
  full. When it is reduced, the cube on the start screen does not turn by itself, the layer to turn on the solve screen is drawn
  leaning and still instead of swinging, turns snap, the cube does not swing to show its back, the finished screen does not
  bob, and the pulses and fades are cut or shortened.
- **Light and dark.** *Settings*, then *Appearance*: the same as the device, light, or dark.
- **Moves in words.** Every move is shown in notation and also said in words with the colour of the side's centre
  (*Top · quarter turn*, *Right · layer 2 · quarter turn*), and each stage says what it does.
- **Messages in words.** The state of the camera on the solve screen (matched, ambiguous, not the side expected) is a
  coloured frame and also a sentence, and the scan's guidance is a sentence.
- **Nine languages**, with the layout mirrored for Hebrew, Arabic and Yiddish. See [Settings](guide/settings.md#language).
- **A typed way in.** A side the camera misread can be put right by typing its colours as letters (`WWW/WRW/WWW`), or by
  keyboard on the colours screen, and a scramble can be pasted in as text.
- **Checked colours.** The app's colours are checked against WCAG 2.2 AA contrast. Some older screens are still short of it
  (see below).

## What is not there

- **A scan without sight.** The camera scan needs you to see the cube and to hold it so the camera can: nothing says which
  way to tilt it. You can type a cube in instead (another size of cube, or its colours by hand), but a person who cannot
  see the colours of their cube cannot read them for the app.
- **Cube colours.** The cube is told apart by colour, and the colours carry no marks or patterns (the colours screen marks a
  sticker the camera was unsure of with a "?"). The moves are given in words so you do not have to tell colours apart to
  follow them, but scanning and checking a cube need you to see its colours. It has not been tested with colour-vision
  differences.
- **Text size.** The app does not follow the system's text-size setting, and has no setting of its own for it. Sizes are
  fixed in the layouts, and it has not been tested with larger text.
- **Esc on every screen.** <kbd>Esc</kbd> does nothing on the start screen, the solve screen or the solved screen. The scan
  screen's <kbd>R</kbd>, <kbd>M</kbd> and <kbd>H</kbd> keys, <kbd>Alt</kbd>+<kbd>←</kbd> and the mouse's back button are not
  built.
- **Spoken durations and directions.** A time is read as it is written (*12:40*), and a quarter turn is said as clockwise or
  anticlockwise as you look at that side, not as left or right.
- **Contrast on every screen.** Some screens still use an older palette that misses 4.5:1 in places (the red text, and
  some of the muted text).
- **Touch targets** have not been measured against a minimum size, and several buttons are shorter than 48 pixels.

## Still to do

Nobody has yet used Cubeulator with a screen reader, switch device, voice control or magnifier, and it has not had an
outside accessibility audit, so none of the above is a conformance claim. The iPhone and web builds have the same names
and roles but have not been tried with a screen reader either. What is checked on every change is the information the app
gives such tools, and the keyboard.

## Tell me about a barrier

If something stops you using Cubeulator, please [open an issue](https://github.com/iainchesworthlabs/cubeulator/issues/new/choose)
and say what you were trying to do, what got in the way, and what you use. The design notes for this work are in
[docs/design/accessibility.md](https://github.com/iainchesworthlabs/cubeulator/blob/develop/docs/design/accessibility.md) on
GitHub.
