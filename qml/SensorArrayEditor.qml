import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import AppBackend 1.0

ColumnLayout {
    signal changesApplied()
    spacing: 5
    visible: HexControllerInst.currentProperties["X Axis"] !== undefined

    Rectangle { Layout.fillWidth: true; height: 1; color: "#aaaaaa" }

    Text {
        text: "Sensor Array Editor"
        font.bold: true
        color: palette.text
        Layout.margins: 10
        Layout.bottomMargin: 0
    }

    GridLayout {
        columns: 2
        Layout.fillWidth: true
        Layout.margins: 10

        Text { text: "X Axis"; color: palette.text; font.bold: true }
        TextField {
            id: xAxisField
            Layout.fillWidth: true
            text: HexControllerInst.currentProperties["X Axis"] || ""
            validator: DoubleValidator {}
            onActiveFocusChanged: {
                if (activeFocus) HexModelInst.setEditSelection(HexControllerInst.currentProperties["PacketStartOffset"] + 6, 4);
                else HexModelInst.setEditSelection(-1, 0);
            }
        }

        Text { text: "Y Axis"; color: palette.text; font.bold: true }
        TextField {
            id: yAxisField
            Layout.fillWidth: true
            text: HexControllerInst.currentProperties["Y Axis"] || ""
            validator: DoubleValidator {}
            onActiveFocusChanged: {
                if (activeFocus) HexModelInst.setEditSelection(HexControllerInst.currentProperties["PacketStartOffset"] + 10, 4);
                else HexModelInst.setEditSelection(-1, 0);
            }
        }

        Text { text: "Z Axis"; color: palette.text; font.bold: true }
        TextField {
            id: zAxisField
            Layout.fillWidth: true
            text: HexControllerInst.currentProperties["Z Axis"] || ""
            validator: DoubleValidator {}
            onActiveFocusChanged: {
                if (activeFocus) HexModelInst.setEditSelection(HexControllerInst.currentProperties["PacketStartOffset"] + 14, 4);
                else HexModelInst.setEditSelection(-1, 0);
            }
        }
    }

    Button {
        text: "Apply Changes"
        Layout.alignment: Qt.AlignRight
        Layout.margins: 10
        Layout.topMargin: 0
        onClicked: {
            HexControllerInst.updateSensorPayload(
                HexControllerInst.currentProperties["PacketStartOffset"],
                parseFloat(xAxisField.text),
                parseFloat(yAxisField.text),
                parseFloat(zAxisField.text)
            );
            HexModelInst.setEditSelection(-1, 0);
            changesApplied();
        }
    }
}