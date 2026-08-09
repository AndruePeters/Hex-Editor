import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import AppBackend 1.0

RowLayout {
    spacing: 10
    property alias mainGrid: hexGrid

    GridView {
        id: hexGrid
        Layout.preferredWidth: 16 * cellWidth
        Layout.fillHeight: true
        model: HexModelInst
        cellWidth: 25
        cellHeight: 25
        clip: true

        delegate: Rectangle {
            width: hexGrid.cellWidth
            height: hexGrid.cellHeight

            property bool isPrimarySelected: index >= HexModelInst.selectionOffset && index < (HexModelInst.selectionOffset + HexModelInst.selectionLength)
            property bool isHighlighted: index >= HexModelInst.highlightOffset && index < (HexModelInst.highlightOffset + HexModelInst.highlightLength)

            color: {
                if (isHighlighted) return "yellow"
                if (isPrimarySelected) return "#0078D7"
                return "transparent"
            }

            Text {
                anchors.centerIn: parent
                text: model.hex !== undefined ? model.hex : ""
                color: {
                    if (isHighlighted) return "black"
                    if (isPrimarySelected) return "white"
                    if (model.isError) return "#cc0000"
                    return palette.text
                }
                font.family: "Monospace"
            }

            MouseArea {
                anchors.fill: parent
                onClicked: HexControllerInst.selectOffset(index)
            }
        }
    }

    GridView {
        id: asciiGrid
        Layout.preferredWidth: 16 * cellWidth
        Layout.fillHeight: true
        model: HexModelInst
        cellWidth: 15
        cellHeight: 25
        clip: true

        delegate: Rectangle {
            width: asciiGrid.cellWidth
            height: asciiGrid.cellHeight

            property bool isPrimarySelected: index >= HexModelInst.selectionOffset && index < (HexModelInst.selectionOffset + HexModelInst.selectionLength)
            property bool isHighlighted: index >= HexModelInst.highlightOffset && index < (HexModelInst.highlightOffset + HexModelInst.highlightLength)

            color: {
                if (isHighlighted) return "yellow"
                if (isPrimarySelected) return "#0078D7"
                return "transparent"
            }

            Text {
                anchors.centerIn: parent
                text: model.ascii !== undefined ? model.ascii : ""
                color: {
                    if (isHighlighted) return "black"
                    if (isPrimarySelected) return "white"
                    if (model.isError) return "#cc0000"
                    return palette.text
                }
                font.family: "Monospace"
            }

            MouseArea {
                anchors.fill: parent
                onClicked: HexControllerInst.selectOffset(index)
            }
        }
    }
}