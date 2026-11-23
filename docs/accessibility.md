---
template: marketing.html
layout: prose
title: Accessibility
title_tag: "Accessibility: what Cubeulator supports today, and what it does not"
description: "A plain statement of what Cubeulator supports for keyboard use, screen readers, colour, text size and language today, and what is not built or not tested yet."
---
# Accessibility

This page says what is true today, and says plainly what is not. Cubeulator is pre-release software, and accessibility has
not had the work it needs yet: the app was built for a person who can see the cube, hold it and use a mouse or a touch
screen. The design sets a target (WCAG 2.2 level AA, read for software); the app has not reached it.

## What is there

- **Light and dark.** *Settings*, then *Appearance*: the same as the device, light, or dark.
- **Keyboard shortcuts.** Settings, a new scan, full screen and quit have shortcuts. The solve screen steps with
  <kbd>→</kbd>, <kbd>Space</kbd> or <kbd>Enter</kbd> and goes back with <kbd>←</kbd>. The solution screen has the arrow
  keys, <kbd>Space</kbd> to play and pause, and <kbd>Home</kbd> and <kbd>End</kbd>. <kbd>Esc</kbd> goes back on the
  history, solution, About and Licences screens, and About and Licences can be moved through with <kbd>Tab</kbd> and the
  arrow keys. The [full list](guide/settings.md#keyboard) is in the guide.
- **Moves in words.** Every move is shown in notation and also said in words with the colour of the side's centre
  (*Top · quarter turn*, *Right · layer 2 · quarter turn*), and each stage says what it does.
- **Messages in words.** The state of the camera on the solve screen (matched, ambiguous, not the side expected) is a
  coloured frame and also a sentence, and the scan's guidance is a sentence.
- **Nine languages**, with the layout mirrored for Hebrew, Arabic and Yiddish. See [Settings](guide/settings.md#language)
  for how complete each is.
- **A typed way in.** A side the camera misread can be put right by typing its colours as letters (`WWW/WRW/WWW`), and a
  scramble can be pasted in as text.
- **Checked colours of the design.** The app's colours come from a set of design tokens, and a script checks the contrast of
  each pairing against WCAG 2.2 AA. That checks the tokens, not every screen.

## What is not there

- **Screen readers.** Most controls have no accessible name or role. Only a few screens (the colours screen, History,
  the solution review, About and Licences) name some of theirs; the start, scan and solve screens do not. A screen reader
  cannot be relied on to read or press their buttons. It has not been tried with a screen reader on any system.
- **Keyboard use of the buttons.** Most buttons are for the mouse and touch and cannot be reached with <kbd>Tab</kbd>. The
  scan screen cannot be operated by keyboard alone.
- **Cube colours.** The cube is told apart by colour, and the colours carry no marks or patterns. The moves are given in
  words so you do not have to tell colours apart to follow them, but scanning and checking a cube need you to see its
  colours. It has not been tested with colour-vision differences.
- **Text size.** It has not been tested with larger text sizes, and nothing says it follows the system's text-size
  setting.
- **Reduced motion.** There is no setting for it. The cube nudges and turns.
- **Touch targets** have not been measured against a minimum size.

## Not tested

No assistive technology has been used with Cubeulator: no screen reader, switch device, voice control or magnifier. No
audit has been made, and none of the claims above is a conformance claim.

## Tell me about a barrier

If something stops you using Cubeulator, please [open an issue](https://github.com/iainchesworthlabs/cubeulator/issues/new/choose)
and say what you were trying to do, what got in the way, and what you use. The design notes for this work are in
[docs/design/accessibility.md](https://github.com/iainchesworthlabs/cubeulator/blob/develop/docs/design/accessibility.md) on
GitHub.
