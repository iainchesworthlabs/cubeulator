// SPDX-FileCopyrightText: 2026 Iain Chesworth
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import Cubeulator

// The keyboard focus ring (docs/design/system.md, "Focus ring"), for whatever
// it is put inside: it fills its parent and shows while the parent has active
// focus.
//
// Two bands, both inside the parent's edge so that no list or Flickable that
// clips its content can cut it: 2 px of `ink`, then 2 px of the page's own
// colour (the page "showing between", as the design has it, even on a button
// that has a fill of its own). Each band is held to 3:1 by the pairs in
// docs/design/tokens.json (`ink` on `paper`, `paper` on the button's fill) and
// by docs/design/tools/check_tokens.py.
//
// Over the camera picture the page is not `paper` but whatever the camera is
// looking at, so there it is `overlay.ink` and the overlay panel's own colour:
// one of the two bands is then visible against any picture.
//
// Nothing here takes focus, and a click or a tap never gives focus to the
// things that have this ring (they take it by Tab), so the ring is for the
// keyboard, as the design asks.
Item {
    id: root

    // True where the control sits over the camera picture.
    property bool overCamera: false
    // Whether to show: the parent's focus, unless the thing in focus is a part
    // of the parent (the cell a cursor is on) that says so itself.
    property bool active: !!parent && parent.activeFocus

    anchors.fill: parent
    visible: root.active
    // Neither is a thing a screen reader should list.
    Accessible.ignored: true

    // Focus that arrives by Tab is no use if it arrives below the fold of a
    // list or a scrolling column: the thing in focus is scrolled into view, in
    // every Flickable (a ListView is one) it is inside.
    onActiveChanged: {
        if (active)
            root.bringIntoView();
    }
    function bringIntoView() {
        const target = root.parent;
        if (!target)
            return;
        for (let up = target.parent; up; up = up.parent) {
            if (up.contentItem === undefined || up.contentY === undefined || up.contentHeight === undefined)
                continue;
            const top = target.mapToItem(up.contentItem, 0, 0).y;
            const bottom = top + target.height;
            let y = up.contentY;
            if (top < y)
                y = top;
            else if (bottom > y + up.height)
                y = bottom - up.height;
            const least = up.originY;
            const most = Math.max(least, up.originY + up.contentHeight - up.height);
            up.contentY = Math.max(least, Math.min(most, y));
        }
    }

    Rectangle {
        anchors.fill: parent
        color: "transparent"
        border.width: 2
        border.color: root.overCamera ? "#F3F2F2" : Theme.ink

        Rectangle {
            anchors.fill: parent
            anchors.margins: 2
            color: "transparent"
            border.width: 2
            border.color: root.overCamera ? "#141312" : Theme.paper
        }
    }
}
