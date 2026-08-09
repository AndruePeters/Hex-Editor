import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import AppBackend 1.0

ColumnLayout {
    id: root
    spacing: 8
    Layout.fillWidth: true
    Layout.fillHeight: true
    visible: HexControllerInst.currentPacketHasString

    property string currentText: {
        let props = HexControllerInst.currentProperties;
        for (let key in props) {
            if (HexControllerInst.isStringField(key)) {
                return props[key];
            }
        }
        return "";
    }

    property string stringFieldName: {
        let props = HexControllerInst.currentProperties;
        for (let key in props) {
            if (HexControllerInst.isStringField(key)) {
                return key;
            }
        }
        return "Decoded ASCII";
    }

    property bool isUpdating: false

    onCurrentTextChanged: {
        if (!isUpdating && textArea.text !== currentText) {
            isUpdating = true;
            textArea.text = currentText;
            isUpdating = false;
        }
    }

    Text {
        text: "String Payload Editor (" + root.stringFieldName + ")"
        font.bold: true
        color: palette.text
    }

    ScrollView {
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.minimumHeight: 150

        TextArea {
            id: textArea
            font.family: "Monospace"
            wrapMode: TextArea.Wrap
            color: palette.text
            selectByMouse: true

            onCursorPositionChanged: {
                if (!root.isUpdating && activeFocus && selectionStart === selectionEnd) {
                    HexControllerInst.selectStringRange(cursorPosition, cursorPosition + 1);
                }
            }

            onSelectionStartChanged: handleSelection()
            onSelectionEndChanged: handleSelection()

            function handleSelection() {
                if (!root.isUpdating && activeFocus && selectionStart !== selectionEnd) {
                    HexControllerInst.selectStringRange(selectionStart, selectionEnd);
                }
            }
        }
    }

    Button {
        Layout.alignment: Qt.AlignRight
        text: "Apply Changes"
        onClicked: {
            let changes = {};
            changes[root.stringFieldName] = textArea.text;
            HexControllerInst.applyStagedChanges(HexControllerInst.currentProperties["PacketStartOffset"], changes);
        }
    }
}