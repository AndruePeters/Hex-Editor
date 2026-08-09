import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import AppBackend 1.0

ColumnLayout {
    signal changesApplied()
    spacing: 5
    visible: HexControllerInst.currentProperties["Decoded ASCII"] !== undefined

    Rectangle { Layout.fillWidth: true; height: 1; color: "#aaaaaa" }

    Text {
        text: "String Payload Editor"
        font.bold: true
        color: palette.text
        Layout.margins: 10
        Layout.bottomMargin: 0
    }

    ScrollView {
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.margins: 10
        clip: true

        TextArea {
            id: payloadArea
            text: HexControllerInst.currentProperties["Decoded ASCII"] || ""
            wrapMode: Text.WrapAnywhere
            selectByMouse: true
            font.family: "Monospace"

            function updateHighlight() {
                let pktStart = HexControllerInst.currentProperties["PacketStartOffset"];
                if (pktStart !== undefined) {
                    let start = selectionStart;
                    let end = selectionEnd;

                    if (start === end) {
                        start = Math.max(0, Math.min(cursorPosition, text.length - 1));
                        if (text.length === 0) start = 0;
                        end = start + 1;
                    }

                    HexModelInst.setEditSelection(pktStart + 6 + start, end - start);
                }
            }

            onCursorPositionChanged: updateHighlight()
            onSelectedTextChanged: updateHighlight()

            onActiveFocusChanged: {
                if (!activeFocus) {
                    HexModelInst.setEditSelection(-1, 0);
                } else {
                    updateHighlight();
                }
            }
        }
    }

    Button {
        text: "Apply Changes"
        Layout.alignment: Qt.AlignRight
        Layout.margins: 10
        Layout.topMargin: 0
        onClicked: {
            HexControllerInst.updateStringPayload(HexControllerInst.currentProperties["PacketStartOffset"], payloadArea.text);
            HexModelInst.setEditSelection(-1, 0);
            changesApplied();
        }
    }
}