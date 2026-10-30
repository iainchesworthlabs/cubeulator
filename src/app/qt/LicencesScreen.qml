// SPDX-FileCopyrightText: 2026 Iain Chesworth
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import Cubeulator

// Licences (docs/design/screens/licences.md): the notices the app owes to the
// components it is built from, readable in the app.
//
// Nothing here is written by hand. The list and the texts are what the build
// generated from what actually ships in it (AboutInfo, from the embedded
// third-party-notices.json), so they cannot drift when a dependency is added or
// removed: the bundled fonts are in it as components, like the libraries.
//
// One column at every size, at most 640 wide, centred from medium up, under a top
// bar of back and the title.
//   - licences.list: the introduction, Cubeulator's own row, and a row for each
//     component, by name, with its licence identifier. A row opens
//   - licences.detail: the component's name as a heading, its version and licence
//     beneath, and its notice in full, in the mono face, in a block that wraps and
//     can be selected. Back: to the list, at the place it was left.
//   - licences.unavailable: the notices could not be read.
//
// Component names and licence identifiers are data from the build. Notice and
// licence texts are never translated, whatever the app's language, and are set
// left to right: they are legal texts in the words their authors chose.
Rectangle {
    id: root
    color: Theme.paper

    // Cubeulator's own row first, then the components by name.
    readonly property var entries: AboutInfo.noticesAvailable
        ? [Object.assign({ own: true }, AboutInfo.own)].concat(AboutInfo.components)
        : []
    // The entry being read: -1 is the list.
    property int selected: -1
    readonly property bool reading: selected >= 0 && selected < entries.length
    // Null when no notice is open; the bindings below test it, not `reading`,
    // because the two change one after the other and a binding that read one
    // and then the other could see them disagree.
    readonly property var current: reading ? entries[selected] : null

    function goBack() {
        if (root.reading) {
            const row = root.selected;
            root.selected = -1;
            const item = list.itemAtIndex(row);
            if (item)
                item.forceActiveFocus(Qt.BacktabFocusReason);
            else
                root.forceActiveFocus();
        } else {
            AppState.closeLicences();
        }
    }

    function open(index) {
        root.selected = index;
        notice.forceActiveFocus();
        scroller.contentY = 0;
    }

    // Hands are on the keys: back leaves, one level at a time.
    focus: true
    Component.onCompleted: forceActiveFocus()
    Keys.onEscapePressed: root.goBack()
    Keys.onBackPressed: root.goBack()

    Item {
        id: column
        width: Math.min(root.width, 640)
        height: parent.height
        anchors.horizontalCenter: parent.horizontalCenter

        // Top bar: back and the title, which stays "Licences" in the detail.
        Item {
            id: bar
            width: parent.width
            height: Math.max(64, titleText.implicitHeight + 2 * Theme.space3)

            AppButton {
                id: back
                objectName: "licences.back"
                anchors.left: parent.left; anchors.leftMargin: Theme.space3
                anchors.verticalCenter: parent.verticalCenter
                variant: "ghost"; leadingIcon: "chevron-left"
                text: qsTrId("common.back")
                onClicked: root.goBack()
            }
            Text {
                id: titleText
                objectName: "licences.title"
                anchors.left: back.right; anchors.leftMargin: Theme.space2
                anchors.right: parent.right; anchors.rightMargin: Theme.space3
                anchors.verticalCenter: parent.verticalCenter
                text: qsTrId("licences.title")
                wrapMode: Text.Wrap
                font.family: Theme.fontHeading; font.weight: Font.ExtraBold; font.pixelSize: 22
                color: Theme.ink
                Accessible.role: Accessible.Heading
                Accessible.name: text
            }
            Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 2; color: Theme.divider }
        }

        // licences.list. The list is named, so the test harness keys every row as
        // one "licences.list.row": a list scrolls, so the rows that exist depend on
        // where it has got to, and a row is one kind of control.
        Item {
            anchors.top: bar.bottom; anchors.bottom: parent.bottom
            width: parent.width
            visible: AboutInfo.noticesAvailable && !root.reading

            ListView {
                id: list
                objectName: "licences.list"
                anchors.fill: parent
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                model: root.entries
                // The introduction scrolls with the rows.
                header: Item {
                    width: list.width
                    height: intro.implicitHeight + Theme.space4 * 2
                    Text {
                        id: intro
                        objectName: "licences.intro"
                        x: Theme.space4; y: Theme.space4
                        width: parent.width - Theme.space4 * 2
                        text: qsTrId("licences.intro")
                        wrapMode: Text.Wrap
                        font.family: Theme.fontHeading; font.pixelSize: 14
                        color: Theme.inkMuted(0.7)
                    }
                }

                delegate: ListRow {
                    required property var modelData
                    required property int index
                    // Cubeulator's own row is "licences.own"; the others are
                    // numbered from 1 in the order they are listed.
                    objectName: modelData.own === true ? "licences.own" : "licences.row" + index
                    width: ListView.view.width
                    title: modelData.own === true ? qsTrId("licences.own") : modelData.name
                    value: modelData.licence
                    onClicked: root.open(index)
                    onActiveFocusChanged: if (activeFocus) ListView.view.positionViewAtIndex(index, ListView.Contain)
                    // Up and Down move along the list itself, not the tab chain
                    // (which on macOS skips what Tab does not reach).
                    handlesMoves: true
                    onMoved: (delta) => {
                        const next = index + delta;
                        if (next < 0 || next >= ListView.view.count)
                            return;
                        ListView.view.positionViewAtIndex(next, ListView.Contain);
                        const row = ListView.view.itemAtIndex(next);
                        if (row)
                            row.forceActiveFocus(delta > 0 ? Qt.TabFocusReason : Qt.BacktabFocusReason);
                    }
                }
            }
        }

        // licences.detail
        Item {
            id: detail
            objectName: "licences.detail"
            anchors.top: bar.bottom; anchors.bottom: parent.bottom
            width: parent.width
            visible: root.reading

            Column {
                id: heading
                x: Theme.space4; y: Theme.space4
                width: parent.width - Theme.space4 * 2
                spacing: Theme.space1
                Text {
                    objectName: "licences.detail.name"
                    width: parent.width
                    text: root.current ? (root.current.own === true ? qsTrId("licences.own") : root.current.name) : ""
                    wrapMode: Text.Wrap
                    font.family: Theme.fontHeading; font.weight: Font.ExtraBold; font.pixelSize: 20
                    color: Theme.ink
                    Accessible.role: Accessible.Heading
                    Accessible.name: text
                }
                Text {
                    objectName: "licences.detail.facts"
                    width: parent.width
                    visible: text !== ""
                    // The version and the licence identifier: data, not words.
                    text: {
                        if (!root.current)
                            return "";
                        const version = root.current.own === true ? "" : root.current.version;
                        return version === "" ? root.current.licence : version + " · " + root.current.licence;
                    }
                    wrapMode: Text.WrapAnywhere
                    font.family: Theme.fontMono; font.pixelSize: 13
                    color: Theme.inkMuted(0.7)
                }
            }

            Flickable {
                id: scroller
                anchors.top: heading.bottom; anchors.topMargin: Theme.space3
                anchors.bottom: parent.bottom
                width: parent.width
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                contentWidth: width
                contentHeight: notice.height + Theme.space6

                // The notice: the mono face on `surface`, one block that wraps and
                // can be selected. Always left to right, even in a language that
                // is not.
                Rectangle {
                    x: Theme.space4
                    width: scroller.width - Theme.space4 * 2
                    height: notice.height + Theme.space3 * 2
                    color: Theme.surface

                    TextEdit {
                        id: notice
                        objectName: "licences.text"
                        x: Theme.space3; y: Theme.space3
                        width: parent.width - Theme.space3 * 2
                        readOnly: true
                        selectByMouse: true
                        activeFocusOnTab: true
                        cursorVisible: false
                        wrapMode: TextEdit.Wrap
                        text: root.current ? root.current.text : ""
                        font.family: Theme.fontMono; font.pixelSize: 12
                        color: Theme.ink
                        selectionColor: Theme.accent
                        LayoutMirroring.enabled: false
                        horizontalAlignment: TextEdit.AlignLeft
                        Accessible.role: Accessible.StaticText
                        Accessible.name: root.current ? root.current.name : ""
                        Accessible.description: text

                        // A long text is read with the keyboard: the arrows and the
                        // page keys scroll it (the cursor has nothing to do in a
                        // block that cannot be edited); Ctrl+A and Ctrl+C select and
                        // copy as usual.
                        Keys.onPressed: (event) => {
                            const page = scroller.height - 40;
                            const limit = Math.max(0, scroller.contentHeight - scroller.height);
                            let y = scroller.contentY;
                            switch (event.key) {
                            case Qt.Key_Down: y += 40; break;
                            case Qt.Key_Up: y -= 40; break;
                            case Qt.Key_PageDown: y += page; break;
                            case Qt.Key_PageUp: y -= page; break;
                            case Qt.Key_Home: y = 0; break;
                            case Qt.Key_End: y = limit; break;
                            default: return;
                            }
                            scroller.contentY = Math.max(0, Math.min(limit, y));
                            event.accepted = true;
                        }
                    }
                }
            }
        }

        // licences.unavailable: the build could not hand the notices over. It
        // should never happen in a build that passed its tests, and a test
        // asserts it; it is designed because a build with no way to read the
        // notices would break the licences it is built on.
        Column {
            id: unavailable
            objectName: "licences.unavailable"
            visible: !AboutInfo.noticesAvailable
            anchors.top: bar.bottom; anchors.topMargin: Theme.space6
            x: Theme.space4
            width: parent.width - Theme.space4 * 2
            spacing: Theme.space3
            Accessible.role: Accessible.AlertMessage
            Accessible.name: qsTrId("licences.unavailable.title")
            Text {
                width: parent.width
                text: qsTrId("licences.unavailable.title")
                wrapMode: Text.Wrap
                font.family: Theme.fontHeading; font.weight: Font.ExtraBold; font.pixelSize: 20
                color: Theme.ink
                Accessible.role: Accessible.Heading
                Accessible.name: text
            }
            Text {
                width: parent.width
                text: qsTrId("licences.unavailable.body").arg(AboutInfo.sourceUrl)
                wrapMode: Text.Wrap
                font.family: Theme.fontHeading; font.pixelSize: 14
                color: Theme.inkMuted(0.7)
            }
        }
    }
}
