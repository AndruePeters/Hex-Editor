import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import AppBackend 1.0

ApplicationWindow {
    id: root
    width: 1024
    height: 768
    visible: true
    title: "Hex & Packet Analyzer"

    menuBar: MenuBar {
        Menu {
            title: "File"
            MenuItem {
                text: "Open..."
                onTriggered: fileDialog.open()
            }
            MenuItem {
                text: "Save As..."
                onTriggered: saveDialog.open()
            }
        }
    }

    FileDialog {
        id: fileDialog
        title: "Please choose a file"
        fileMode: FileDialog.OpenFile
        onAccepted: {
            HexModelInst.loadFile(selectedFile)
            HexControllerInst.parseCurrentBuffer()
            minimapImage.source = "image://hexminimap/render?" + Math.random()
        }
    }

    FileDialog {
        id: saveDialog
        title: "Save File As"
        fileMode: FileDialog.SaveFile
        onAccepted: {
            HexModelInst.saveFile(selectedFile)
        }
    }

    SplitView {
        anchors.fill: parent
        orientation: Qt.Horizontal

        // Left Container: Hex View + Minimap
        RowLayout {
            SplitView.preferredWidth: 700
            SplitView.minimumWidth: 400
            spacing: 0

            TableView {
                id: hexView
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                model: HexModelInst

                delegate: Rectangle {
                    implicitWidth: 32
                    implicitHeight: 24
                    
                    color: {
                        if (model.isEditCursor) return "#ffaa00" // Distinct orange for text cursor/selection
                        if (model.isSelected) return "#3399ff"    // Blue for packet boundary selection
                        if (model.isError) return "#ffcccc"
                        if (model.packetId !== -1) return (model.packetId % 2 === 0) ? "#2a2a2a" : "#353535"
                        return "transparent"
                    }

                    Rectangle { anchors.top: parent.top; anchors.left: parent.left; anchors.right: parent.right; height: 1; color: "#aaaaaa"; visible: model.edgeTop }
                    Rectangle { anchors.bottom: parent.bottom; anchors.left: parent.left; anchors.right: parent.right; height: 1; color: "#aaaaaa"; visible: model.edgeBottom }
                    Rectangle { anchors.left: parent.left; anchors.top: parent.top; anchors.bottom: parent.bottom; width: 1; color: "#aaaaaa"; visible: model.edgeLeft }
                    Rectangle { anchors.right: parent.right; anchors.top: parent.top; anchors.bottom: parent.bottom; width: 1; color: "#aaaaaa"; visible: model.edgeRight }

                    Text {
                        anchors.centerIn: parent
                        text: model.display || ""
                        color: {
                            if (model.isSelected || model.isEditCursor) return "#ffffff"
                            if (model.isError) return "#cc0000"
                            return palette.text
                        }
                        font.family: "Monospace"
                        font.pixelSize: 12
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            let absoluteOffset = (model.row * 16) + model.column;
                            HexControllerInst.selectOffset(absoluteOffset);
                        }
                    }
                }
            }

            Item {
                id: minimap
                Layout.preferredWidth: 64
                Layout.fillHeight: true

                Image {
                    id: minimapImage
                    anchors.fill: parent
                    source: "image://hexminimap/render"
                    fillMode: Image.Stretch
                }

                Rectangle {
                    id: viewportIndicator
                    width: parent.width
                    color: palette.highlight
                    opacity: 0.3
                    border.color: palette.highlight
                    border.width: 1
                    height: Math.max(16, (hexView.height / Math.max(1, hexView.contentHeight)) * minimap.height)
                    y: (hexView.contentY / Math.max(1, hexView.contentHeight)) * minimap.height

                    MouseArea {
                        anchors.fill: parent
                        drag.target: viewportIndicator
                        drag.axis: Drag.YAxis
                        drag.minimumY: 0
                        drag.maximumY: minimap.height - viewportIndicator.height
                        onPositionChanged: {
                            if (drag.active) {
                                let ratio = viewportIndicator.y / (minimap.height - viewportIndicator.height);
                                hexView.contentY = ratio * (hexView.contentHeight - hexView.height);
                            }
                        }
                    }
                }
            }
        }

        // Right Container: Properties View & Payload Editor
        SplitView {
            orientation: Qt.Vertical
            SplitView.fillWidth: true
            SplitView.minimumWidth: 250

            ListView {
                SplitView.fillHeight: true
                SplitView.preferredHeight: 300
                Layout.margins: 10
                clip: true
                spacing: 8
                
                model: Object.keys(HexControllerInst.currentProperties).filter(k => k !== "HasError" && k !== "ErrorMessage" && k !== "Decoded ASCII" && k !== "PacketStartOffset")

                delegate: RowLayout {
                    width: ListView.view.width
                    
                    Text {
                        text: modelData
                        font.bold: true
                        Layout.preferredWidth: 120
                        Layout.alignment: Qt.AlignTop
                        color: HexControllerInst.currentProperties["HasError"] ? "#cc0000" : palette.text
                    }
                    
                    Text {
                        text: HexControllerInst.currentProperties[modelData]
                        Layout.fillWidth: true
                        wrapMode: Text.WrapAnywhere
                        Layout.alignment: Qt.AlignTop
                        color: HexControllerInst.currentProperties["HasError"] ? "#cc0000" : palette.text
                    }
                }
            }

            // Dedicated Multi-line Payload Editor
            ColumnLayout {
                SplitView.fillHeight: true
                SplitView.preferredHeight: 300
                visible: HexControllerInst.currentProperties["Decoded ASCII"] !== undefined
                spacing: 5

                Rectangle {
                    Layout.fillWidth: true
                    height: 1
                    color: "#aaaaaa"
                }

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
                        
                        function updateEditSelection() {
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

                        onCursorPositionChanged: updateEditSelection()
                        onSelectionStartChanged: updateEditSelection()
                        onSelectionEndChanged: updateEditSelection()
                        
                        onActiveFocusChanged: {
                            if (!activeFocus) {
                                HexModelInst.setEditSelection(-1, 0);
                            } else {
                                updateEditSelection();
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
                        minimapImage.source = "image://hexminimap/render?" + Math.random();
                        HexModelInst.setEditSelection(-1, 0);
                    }
                }
            }

            Rectangle {
                SplitView.preferredHeight: 50
                color: "#ffe6e6"
                border.color: "red"
                border.width: 1
                visible: HexControllerInst.currentProperties["HasError"] === true
                Layout.margins: 10

                Text {
                    anchors.centerIn: parent
                    text: HexControllerInst.currentProperties["ErrorMessage"] || ""
                    color: "red"
                    font.bold: true
                }
            }
        }
    }
}