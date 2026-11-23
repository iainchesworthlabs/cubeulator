// SPDX-FileCopyrightText: 2026 Iain Chesworth
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import Cubeulator

// btn / btn-primary / btn-secondary / btn-ghost from the Modernist system:
// flat fills, zero radius, flush-left labels in block buttons.
//
// It is a button to a screen reader (a radio button when it is one of a set of
// choices, `checkable`), named by its own label, and to the keyboard: Tab
// reaches it, Space and Enter press it, and a ring says where focus is. A
// click or a tap does not take focus from the screen (the solve screen's keys
// act on the screen, not on the last button that was pressed), so the ring is
// for the keyboard alone.
Item {
    id: root

    property string text: ""
    property string variant: "primary" // primary | secondary | ghost
    property string trailingIcon: ""
    // Keeps the room for a trailing icon when there is none, so that a button
    // that shows a mark only sometimes (a tick on a side that is done) is the
    // same size, and its label in the same place, with and without it.
    property bool trailingSlot: false
    property string leadingIcon: ""
    property bool block: false // fill width; label flush-left instead of centered
    // The widest the button may be, for one that is as wide as its label (a
    // chip in a row). A label that does not fit wraps onto another line and
    // the button grows taller; it is never cut off or run past the edge. -1:
    // as wide as the label, as before. A block button is as wide as it is
    // given, and wraps the same way.
    property real maxWidth: -1

    readonly property bool hasTrailing: root.trailingIcon !== "" || root.trailingSlot
    readonly property real chrome: (root.leadingIcon ? 26 : 0) + (root.hasTrailing ? 26 : 0) + Theme.space6
    readonly property real textRoom: root.block
        ? root.width - Theme.space4 * 2 - (root.leadingIcon ? 26 : 0) - (root.trailingIcon ? 26 : 0)
        : root.width - root.chrome

    implicitHeight: Math.max(44, label.height + Theme.space3 * 2)
    // What the label measures on one line is the label's own implicitWidth,
    // not a second measurement of the same string: a button sized by one
    // measure and a text laid out by another disagree by a pixel or two on
    // some platforms and scripts, and a string a pixel wider than its box is a
    // string cut off.
    implicitWidth: block ? 200 : (maxWidth > 0 ? Math.min(label.implicitWidth + chrome, maxWidth)
                                               : label.implicitWidth + chrome)

    // What a screen reader says: the label unless it is told otherwise (a
    // button whose label reads badly alone), and, where the label is not
    // enough, what pressing it does.
    property string accessibleName: root.text
    property string accessibleDescription: ""
    // One of a set of choices, of which `checked` says whether this is the one
    // that is on: a chip, a method, a size. Its look is the caller's (the
    // variant); this is what a screen reader hears.
    property bool checkable: false
    property bool checked: false
    // Over the camera picture, where the focus ring takes the overlay's colours.
    property bool overCamera: false

    signal clicked()

    readonly property bool primary: variant === "primary"
    readonly property bool secondary: variant === "secondary"
    readonly property bool ghost: variant === "ghost"

    opacity: enabled ? 1.0 : 0.45

    // `enabled` is Item's own (this used to shadow it with a property of its
    // own, which left the item itself enabled): a disabled button is out of the
    // tab chain and says so to a screen reader, and so is one whose parent is
    // disabled.
    activeFocusOnTab: true
    Accessible.role: root.checkable ? Accessible.RadioButton : Accessible.Button
    Accessible.name: root.accessibleName
    Accessible.description: root.accessibleDescription
    Accessible.focusable: true
    Accessible.checkable: root.checkable
    Accessible.checked: root.checked
    Accessible.onPressAction: root.clicked()

    Keys.onPressed: (event) => {
        if (event.key === Qt.Key_Space || event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
            event.accepted = true;
            // Held down it does not press again and again.
            if (!event.isAutoRepeat)
                root.clicked();
        }
    }

    Rectangle {
        anchors.fill: parent
        color: root.primary ? (mouse.pressed ? Theme.accent700 : (mouse.containsMouse ? Theme.accent600 : Theme.accent))
             : root.secondary ? (mouse.containsMouse ? Theme.inkMuted(0.07) : "transparent")
             : (mouse.containsMouse ? Qt.rgba(0.925, 0.188, 0.075, 0.10) : "transparent")
        border.width: root.secondary ? 1 : 0
        border.color: Theme.divider
    }

    Row {
        id: content
        anchors.verticalCenter: parent.verticalCenter
        anchors.left: root.block ? parent.left : undefined
        anchors.leftMargin: root.block ? Theme.space4 : 0
        anchors.horizontalCenter: root.block ? undefined : parent.horizontalCenter
        spacing: 8

        Icon {
            visible: root.leadingIcon !== ""
            name: root.leadingIcon
            width: 16; height: 16
            anchors.verticalCenter: parent.verticalCenter
            color: root.ghost ? Theme.accent : Theme.ink
        }

        Text {
            id: label
            text: root.text
            font.family: Theme.fontHeading
            font.weight: Font.ExtraBold
            font.pixelSize: 14
            anchors.verticalCenter: parent.verticalCenter
            color: root.primary ? Theme.paper : (root.ghost ? Theme.accent : Theme.ink)
            // As wide as it is, up to the room the button leaves it; past that
            // it wraps, at a word if it can and inside a word if it must, so
            // that nothing is ever wider than the box. (Before the button has
            // a width there is no room to speak of, so it is left whole.)
            wrapMode: Text.Wrap
            width: root.textRoom > 0 ? Math.min(implicitWidth, root.textRoom) : implicitWidth
            // The button says it; reading the label as well says it twice.
            Accessible.ignored: true
        }

        // Inside the row, so that the label and the icon are centred together:
        // beside the row the icon stood outside the label's own centre and
        // stuck out past the edge of the button.
        Item {
            visible: !root.block && root.hasTrailing
            width: 18; height: 18
            anchors.verticalCenter: parent.verticalCenter
            Icon {
                visible: root.trailingIcon !== ""
                anchors.fill: parent
                name: root.trailingIcon
                color: root.primary ? Theme.paper : Theme.ink
            }
        }
    }

    Icon {
        visible: root.trailingIcon !== "" && root.block
        name: root.trailingIcon
        width: 18; height: 18
        anchors.verticalCenter: parent.verticalCenter
        anchors.right: parent.right
        anchors.rightMargin: Theme.space4
        color: root.primary ? Theme.paper : Theme.ink
    }

    FocusRing { overCamera: root.overCamera }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }
}
