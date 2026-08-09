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
            minimap.refresh()
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

        RowLayout {
            SplitView.preferredWidth: 700
            SplitView.minimumWidth: 400
            spacing: 0

            HexTableView {
                id: hexView
                Layout.fillWidth: true
                Layout.fillHeight: true
            }

            MinimapView {
                id: minimap
                Layout.preferredWidth: 64
                Layout.fillHeight: true
                targetView: hexView.mainGrid
            }
        }

        SplitView {
            orientation: Qt.Vertical
            SplitView.fillWidth: true
            SplitView.minimumWidth: 250

            PropertiesList {
                SplitView.fillHeight: true
                SplitView.preferredHeight: 400
            }

            StringPayloadEditor {
                SplitView.fillHeight: true
                SplitView.preferredHeight: 300
                visible: HexControllerInst.currentPacketHasString
            }
        }
    }
}