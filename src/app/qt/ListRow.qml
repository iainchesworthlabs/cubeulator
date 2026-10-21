import QtQuick
import Cubeulator

// A row of a list: a title, a value in the mono face beneath it, and a chevron
// at the trailing end (docs/design/screens/about.md and licences.md: "list
// row"). The whole row is the target. It takes the keyboard: Tab reaches it,
// Return, Enter and Space press it, Up and Down move to the row before and
// after, and a ring says which one it is on.
Item {
    id: root

    property string title: ""
    // The licence identifier, the address, the version: data, not words, so it
    // is set in the mono face and is never translated.
    property string value: ""
    // Accessibility: what a screen reader says, where the title and the value
    // read badly together. Defaults to both.
    property string accessibleName: value === "" ? title : title + ", " + value
    // A row that leaves the app for a browser is a link to a reader of the
    // screen; one that goes deeper is a button.
    property bool link: false
    property bool chevron: true
    // How far the text and the chevron are from the row's edges.
    property real inset: Theme.space4
    signal clicked()

    // 64, and taller for a title or a value that needs more lines.
    implicitHeight: Math.max(64, texts.implicitHeight + Theme.space3 * 2)
    width: parent ? parent.width : implicitWidth

    activeFocusOnTab: true
    Accessible.role: root.link ? Accessible.Link : Accessible.Button
    Accessible.name: root.accessibleName
    Accessible.focusable: true
    Accessible.onPressAction: root.clicked()

    Keys.onReturnPressed: root.clicked()
    Keys.onEnterPressed: root.clicked()
    Keys.onSpacePressed: root.clicked()
    Keys.onDownPressed: {
        const next = root.nextItemInFocusChain(true);
        if (next)
            next.forceActiveFocus(Qt.TabFocusReason);
    }
    Keys.onUpPressed: {
        const previous = root.nextItemInFocusChain(false);
        if (previous)
            previous.forceActiveFocus(Qt.BacktabFocusReason);
    }

    Rectangle {
        anchors.fill: parent
        visible: area.containsMouse
        color: Theme.inkMuted(0.07)
    }
    Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.divider }

    Column {
        id: texts
        anchors.left: parent.left; anchors.leftMargin: root.inset
        anchors.right: chevronIcon.visible ? chevronIcon.left : parent.right
        anchors.rightMargin: chevronIcon.visible ? Theme.space2 : root.inset
        anchors.verticalCenter: parent.verticalCenter
        spacing: 2
        Text {
            objectName: root.objectName + ".title"
            width: parent.width
            text: root.title
            wrapMode: Text.Wrap
            font.family: Theme.fontHeading; font.weight: Font.ExtraBold; font.pixelSize: 15
            color: Theme.ink
            Accessible.ignored: true
        }
        Text {
            visible: root.value !== ""
            width: parent.width
            text: root.value
            // An address or an identifier has no break in it; it must still
            // not run off the row.
            wrapMode: Text.WrapAnywhere
            font.family: Theme.fontMono; font.pixelSize: 12
            color: Theme.inkMuted(0.6)
            // Latin letters and digits: Qt lays them out left to right in a
            // right-to-left screen, and the line sits at the row's start.
            Accessible.ignored: true
        }
    }

    Icon {
        id: chevronIcon
        visible: root.chevron
        anchors.right: parent.right; anchors.rightMargin: root.inset
        anchors.verticalCenter: parent.verticalCenter
        name: "chevron-right"
        width: 18; height: 18
        color: Theme.ink
    }

    // The ring: 2 px, inside the row, so that it is not cut by the list's clip.
    Rectangle {
        anchors.fill: parent
        anchors.margins: 1
        visible: root.activeFocus
        color: "transparent"
        border.width: 2
        border.color: Theme.accent
    }

    MouseArea {
        id: area
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: {
            root.forceActiveFocus(Qt.MouseFocusReason);
            root.clicked();
        }
    }
}
