// SPDX-FileCopyrightText: 2026 Iain Chesworth
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import Cubeulator

// About (docs/design/screens/about.md): what the app is, which version, who made
// it and under what licence, and the way to the notices and to the source.
//
// One scrolling column at every size, at most 640 wide, centred from medium up,
// under a top bar of back and the title. Back is Esc: to where About was opened
// from. Built here: the lock-up's name and version, the three paragraphs, the
// Licences and Source code rows and the small print. Not built, because what
// they lead to is not: Privacy, the build line with Copy, and Copy details.
Rectangle {
    id: root
    color: Theme.paper

    function goBack() {
        AppState.closeAbout();
    }

    // Hands are on the keys: back leaves.
    focus: true
    Component.onCompleted: forceActiveFocus()
    Keys.onEscapePressed: root.goBack()
    Keys.onBackPressed: root.goBack()

    Item {
        id: column
        width: Math.min(root.width, 640)
        height: parent.height
        anchors.horizontalCenter: parent.horizontalCenter

        // Top bar: back, and the title, which wraps onto as many lines as it
        // needs.
        Item {
            id: bar
            width: parent.width
            height: Math.max(64, titleText.implicitHeight + 2 * Theme.space3)

            AppButton {
                id: back
                objectName: "about.back"
                anchors.left: parent.left; anchors.leftMargin: Theme.space3
                anchors.verticalCenter: parent.verticalCenter
                variant: "ghost"; leadingIcon: "chevron-left"
                text: qsTrId("common.back")
                onClicked: root.goBack()
            }
            Text {
                id: titleText
                objectName: "about.title"
                anchors.left: back.right; anchors.leftMargin: Theme.space2
                anchors.right: parent.right; anchors.rightMargin: Theme.space3
                anchors.verticalCenter: parent.verticalCenter
                text: qsTrId("about.title")
                wrapMode: Text.Wrap
                font.family: Theme.fontHeading; font.weight: Font.ExtraBold; font.pixelSize: 22
                color: Theme.ink
                Accessible.role: Accessible.Heading
                Accessible.name: text
            }
            Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 2; color: Theme.divider }
        }

        Flickable {
            id: scroller
            objectName: "about.scroll"
            anchors.top: bar.bottom; anchors.bottom: parent.bottom
            width: parent.width
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            contentWidth: width
            contentHeight: body.height + Theme.space6 * 2

            Column {
                id: body
                x: Theme.space6; y: Theme.space6
                width: scroller.width - Theme.space6 * 2
                spacing: Theme.space4

                // The lock-up: the name, never translated, and the version in
                // the mono face. Both can be selected, for a bug report.
                Column {
                    width: parent.width
                    spacing: Theme.space1
                    Text {
                        objectName: "about.name"
                        width: parent.width
                        text: qsTrId("app.name")
                        wrapMode: Text.Wrap
                        font.family: Theme.fontHeading; font.weight: Font.ExtraBold; font.pixelSize: 32
                        color: Theme.ink
                        Accessible.role: Accessible.Heading
                        Accessible.name: text
                    }
                    TextEdit {
                        objectName: "about.version"
                        width: parent.width
                        readOnly: true
                        selectByMouse: true
                        activeFocusOnTab: true
                        // Isolated left to right, so that a right-to-left line does not
                        // turn the number's parts round its hyphen.
                        text: qsTrId("about.version").arg(AboutInfo.displayVersion)
                        wrapMode: TextEdit.Wrap
                        font.family: Theme.fontMono; font.pixelSize: 14
                        color: Theme.inkMuted(0.7)
                        selectionColor: Theme.accent
                        Accessible.role: Accessible.StaticText
                        Accessible.name: text
                    }
                }

                Text {
                    objectName: "about.what"
                    width: parent.width
                    text: qsTrId("about.what")
                    wrapMode: Text.Wrap
                    font.family: Theme.fontHeading; font.pixelSize: 15
                    color: Theme.ink
                }
                Text {
                    objectName: "about.author"
                    width: parent.width
                    text: qsTrId("about.author").arg(AboutInfo.publisher)
                    wrapMode: Text.Wrap
                    font.family: Theme.fontHeading; font.pixelSize: 15
                    color: Theme.ink
                }
                Text {
                    objectName: "about.licence"
                    width: parent.width
                    text: qsTrId("about.licence")
                    wrapMode: Text.Wrap
                    font.family: Theme.fontHeading; font.pixelSize: 15
                    color: Theme.inkMuted(0.7)
                }

                // The rows run to the column's edges, not its margins, as a
                // list does: they are in the scroller's width, not the body's.
                Column {
                    id: rows
                    x: -Theme.space6
                    width: scroller.width
                    ListRow {
                        id: licencesRow
                        objectName: "about.licences"
                        title: qsTrId("about.licences")
                        inset: Theme.space6
                        handlesMoves: true
                        onMoved: (delta) => { if (delta > 0) sourceRow.forceActiveFocus(Qt.TabFocusReason); }
                        onClicked: AppState.openLicences()
                    }
                    // The GPL obliges the app to say where its source is, so
                    // this is on the screen, not a level down. The address is
                    // shown whole, so that it can be read and typed.
                    ListRow {
                        id: sourceRow
                        objectName: "about.source"
                        handlesMoves: true
                        onMoved: (delta) => { if (delta < 0) licencesRow.forceActiveFocus(Qt.BacktabFocusReason); }
                        title: qsTrId("about.source")
                        value: AboutInfo.sourceUrl
                        inset: Theme.space6
                        link: true
                        chevron: false
                        onClicked: Qt.openUrlExternally(AboutInfo.sourceUrl)
                    }
                }

                Text {
                    objectName: "about.trademark"
                    width: parent.width
                    text: qsTrId("about.trademark")
                    wrapMode: Text.Wrap
                    font.family: Theme.fontHeading; font.pixelSize: 12
                    color: Theme.inkMuted(0.6)
                }
            }
        }
    }
}
