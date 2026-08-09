import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import AppBackend 1.0

RowLayout {
    spacing: 10
    property alias mainGrid: hexGrid

    // 1. Memory Address Column
    ListView {
        id: addressList
        Layout.preferredWidth: 80
        Layout.fillHeight: true
        model: Math.ceil(HexModelInst.size / 16)
        interactive: false // Synced via contentY binding below
        clip: true

        contentY: hexGrid.contentY

        delegate: Item {
            width: addressList.width
            height: 25

            Text {
                anchors.centerIn: parent
                // Calculate absolute byte address (16 bytes per row)
                text: "0x" + (index * 16).toString(16).toUpperCase().padStart(8, '0')
                color: palette.text
                font.family: "Monospace"
                font.bold: true
            }
        }
    }

    // 2. Hex Values Grid
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

    // 3. ASCII Text Grid
    // GridView {
    //     id: asciiGrid
    //     Layout.preferredWidth: 16 * cellWidth
    //     Layout.fillHeight: true
    //     model: HexModelInst
    //     cellWidth: 15
    //     cellHeight: 25
    //     clip: true
    //     contentY: hexGrid.contentY
    //     interactive: false
    //
    //     delegate: Rectangle {
    //         width: asciiGrid.cellWidth
    //         height: asciiGrid.cellHeight
    //
    //         property bool isPrimarySelected: index >= HexModelInst.selectionOffset && index < (HexModelInst.selectionOffset + HexModelInst.selectionLength)
    //         property bool isHighlighted: index >= HexModelInst.highlightOffset && index < (HexModelInst.highlightOffset + HexModelInst.highlightLength)
    //
    //         color: {
    //             if (isHighlighted) return "yellow"
    //             if (isPrimarySelected) return "#0078D7"
    //             return "transparent"
    //         }
    //
    //         Text {
    //             anchors.centerIn: parent
    //             text: model.ascii !== undefined ? model.ascii : ""
    //             color: {
    //                 if (isHighlighted) return "black"
    //                 if (isPrimarySelected) return "white"
    //                 if (model.isError) return "#cc0000"
    //                 return palette.text
    //             }
    //             font.family: "Monospace"
    //         }
    //
    //         MouseArea {
    //             anchors.fill: parent
    //             onClicked: HexControllerInst.selectOffset(index)
    //         }
    //     }
    // }
}