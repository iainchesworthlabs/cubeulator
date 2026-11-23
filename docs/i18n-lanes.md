# Literals not yet converted

None. Every user-visible literal in the QML asks for its words by id
(`qsTrId`), so there is nothing to list: there were 73 at 0.2.0-beta.4, grouped
by the lane that owned each file, and the scan, solve, kit and experience lanes'
literals were converted together on 2026-11-23 (`i18n.md`, "Completing the
translations"). The allowlist, `tools/i18n/lint_allowlist.json`, has no entry.

This page is the output of `python tools/i18n/lint_strings.py --report`, which
prints a table for each lane that has an entry in the allowlist, with the id in
`strings.json` to use where the design has one. Regenerate it rather than
editing it if an entry is ever added: a literal that is added without one fails
the lint.
