// SPDX-FileCopyrightText: 2026 Iain Chesworth
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import Cubeulator

// Says something to a screen reader without moving focus: a change the person
// did not ask for and cannot see (the scan's guidance, the next move, an
// error). docs/design/accessibility.md, "Screen readers".
//
// Declare it as a direct child of a screen's root item and bind `message` to
// the words; every change of them is said, politely, once it has stopped
// changing for `settle` ms (a side being read changes the guidance three times
// in a second, and a reader that is told all three says the last), and the
// same words are not said again within `repeatAfter` ms. The announcement is
// raised on the parent, which is an item the reader lists (the screen's
// root has a role and a name), through Qt's Accessible.announce
// (QAccessibleAnnouncementEvent, Qt 6.8): UI Automation's notification event
// on Windows, the announcement notification on macOS and iOS.
//
// `lead` is a second line that is said before the message when it has changed
// since it was last said: the headline above the guidance, the stage above the
// move. When only the message changes, only the message is said.
//
// Nothing is drawn, and the item itself is not in the reader's tree: the words
// are spoken once, not left for a person to find again.
Item {
    id: root

    property string message: ""
    property string lead: ""
    // Interrupts what the reader is saying: for something that stops the
    // person, a camera that will not start, not for progress.
    property bool assertive: false
    property int settle: 300
    property int repeatAfter: 2000

    // The last thing said, and when; what a test reads back. An announcement
    // is an event, so it leaves nothing on the item for a test to find.
    readonly property alias lastSaid: memory.said
    readonly property alias saidCount: memory.count

    width: 0
    height: 0
    Accessible.ignored: true

    QtObject {
        id: memory
        property string said: ""
        property string leadSaid: ""
        property double at: 0
        property int count: 0
    }

    onMessageChanged: hold.restart()
    onLeadChanged: hold.restart()

    // Says `text` now, whatever it is and wherever the timer is.
    function say(text) {
        if (text === "")
            return;
        const now = Date.now();
        if (text === memory.said && now - memory.at < root.repeatAfter)
            return;
        memory.said = text;
        memory.at = now;
        memory.count += 1;
        if (root.parent)
            root.parent.Accessible.announce(text, root.assertive ? Accessible.Assertive : Accessible.Polite);
    }

    Timer {
        id: hold
        interval: root.settle
        onTriggered: {
            let text = root.message;
            if (root.lead !== "" && root.lead !== memory.leadSaid) {
                memory.leadSaid = root.lead;
                text = text === "" ? root.lead : root.lead + ". " + text;
            }
            root.say(text);
        }
    }
}
