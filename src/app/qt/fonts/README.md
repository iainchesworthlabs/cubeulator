# Bundled fonts

Compiled into the app under `:/fonts` and registered at start-up by
`kit::installBundledFonts()` (`../kit/bundled_fonts.cpp`), so that the design's
Latin type (`docs/design/system.md`, "Type") is what is shown whether or not the
machine has these families installed, and so that Qt is never asked for a family
it does not have: that costs about 350 ms at the first text, while it looks
through every font the system has (`docs/performance.md`).

Every `.ttf` and `.otf` in this folder is picked up by `src/app/CMakeLists.txt`
on the platforms whose fragment sets `CUBEULATOR_PLATFORM_BUNDLE_LATIN_FONTS`
(every one but the browser, which draws with the face Qt's kit carries and has
its own folder for its Japanese and Chinese faces,
`src/app/platform/web/fonts/`). Adding one needs no code change.

| file | family | weight | used for | licence |
|---|---|---|---|---|
| `Archivo-Regular.ttf` | Archivo | 400 | running text | `OFL-Archivo.txt` |
| `Archivo-Bold.ttf` | Archivo | 700 | headline, direction, bold body | `OFL-Archivo.txt` |
| `Archivo-ExtraBold.ttf` | Archivo | 800 | display, title, instruction, labels | `OFL-Archivo.txt` |
| `IBMPlexMono-Medium.ttf` | IBM Plex Mono | 500 | numerals, notation, key names | `OFL-IBMPlexMono.txt` |

Japanese and Simplified Chinese use the system's own font, chosen by language
(`Theme.fontHeading`). Hebrew, Arabic and Yiddish are named Archivo too; Archivo
has no glyphs for them, so Qt takes them from the system's fallback for the
script. The kit's plan is to bundle Noto Sans Hebrew and Noto Sans Arabic for
them (`docs/design/family.md`); their manifest entries are dormant until the
files exist.

All are under the SIL Open Font Licence 1.1, which allows bundling and
redistribution with the app and asks that the licence travel with the fonts: the
generated third-party notices carry each family's licence text and copyright
line (`third_party/notices/manifest.json`, `font-archivo` and
`font-ibm-plex-mono`; a font file that no emitted entry claims is an error).
The files are unmodified.

## Where they came from

Copied 2026-10-13, unmodified, with the licence text each project ships.

| family | source | version |
|---|---|---|
| Archivo | https://github.com/Omnibus-Type/Archivo, `fonts/ttf/` and `OFL.txt` | commit `211127690e8ff106c36c935f7e5e697114cff103` (2026-09-02) |
| IBM Plex Mono | https://github.com/IBM/plex release `@ibm/plex-mono@2.5.0`, `ibm-plex-mono.zip`, `fonts/complete/ttf/` | 2.5.0 |

SHA-256 of what is in this folder:

```
951a0ebab63b1bb0d90a26c27625bda803d570dace3851fa2f1eea65852d8983  Archivo-Bold.ttf
9fff67922ae2bc376a9e5c6d644eebf8c5b17f62a0f38e5f016e4b7e3903662f  Archivo-ExtraBold.ttf
59fbfd8a9bb41301bfe950e7b097fd42d52af224a07761e2c1b2c959a4c9cf9a  Archivo-Regular.ttf
98fbd727aae340b236955879dabed4d991aac9e8e90b3b2a67ce4a59221cc97c  IBMPlexMono-Medium.ttf
```

## Notes for whoever uses them

- Archivo ExtraBold and IBM Plex Mono Medium carry legacy family names
  ("Archivo ExtraBold", "IBM Plex Mono Medm") alongside the typographic ones.
  `tests/unit/bundled_fonts_test.cpp` checks that Qt resolves weights 400, 700
  and 800 of Archivo and weight 500 of IBM Plex Mono to the right faces on the
  platform the suite runs on; run it on each platform before trusting it
  there.
- The test programs register the bundle as the app does, so the layout checks
  run in Archivo on every machine; `CUBE_I18N_TEST_FONT` leaves it out to try
  another face (`docs/i18n.md`, "Which face the layout checks run in").
- An update is: replace the file, update this table and the hashes, run
  `cube_qml_tests "[fonts]"`.
