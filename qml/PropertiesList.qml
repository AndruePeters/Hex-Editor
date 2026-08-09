import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import AppBackend 1.0

ColumnLayout {
    spacing: 10

    property var stagedChanges: ({})

    Connections {
        target: HexControllerInst
        function onPropertiesChanged() {
            stagedChanges = {};
        }
    }

    function getOrderedProperties() {
        let keys = Object.keys(HexControllerInst.currentProperties || {});
        let ordered = [];

        let topKeys = ["Hex Address", "Absolute Offset", "Struct", "CRC", "Calculated CRC"];
        topKeys.forEach(k => {
            if (keys.includes(k)) ordered.push(k);
        });

        let skipKeys = topKeys.concat(["HasError", "ErrorMessage", "PacketStartOffset"]);

        keys.forEach(k => {
            if (!skipKeys.includes(k)) ordered.push(k);
        });

        return ordered;
    }

    ListView {
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.margins: 10
        clip: true
        spacing: 8

        model: getOrderedProperties()

        delegate: RowLayout {
            width: ListView.view.width

            Text {
                text: modelData
                font.bold: true
                Layout.preferredWidth: 120
                Layout.alignment: Qt.AlignVCenter
                color: HexControllerInst.currentProperties["HasError"] ? "#cc0000" : palette.text

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: HexControllerInst.selectPropertyBytes(modelData)
                }
            }

            Loader {
                Layout.fillWidth: true
                Layout.preferredHeight: 35
                Layout.alignment: Qt.AlignVCenter

                sourceComponent: {
                    let opts = HexControllerInst.propertyOptions[modelData];
                    return (opts !== undefined && opts.length > 0) ? enumDelegate : textDelegate;
                }

                Component {
                    id: textDelegate
                    TextField {
                        anchors.fill: parent
                        text: stagedChanges[modelData] !== undefined ?
                            stagedChanges[modelData] :
                            HexControllerInst.currentProperties[modelData]

                        readOnly: {
                            let systemKeys = ["Hex Address", "Absolute Offset", "Struct", "CRC", "Calculated CRC"];
                            if (systemKeys.includes(modelData)) return true;
                            return !HexControllerInst.isConfigEditable(modelData);
                        }

                        color: HexControllerInst.currentProperties["HasError"] ? "#cc0000" : palette.text

                        background: Rectangle {
                            color: parent.readOnly ? "transparent" : palette.base
                            border.color: parent.readOnly ? "transparent" : palette.mid
                        }

                        onTextEdited: {
                            if (!readOnly) {
                                let tmp = stagedChanges;
                                tmp[modelData] = text;
                                stagedChanges = Object.assign({}, tmp);
                                HexControllerInst.selectPropertyBytes(modelData);
                            }
                        }
                    }
                }

                Component {
                    id: enumDelegate
                    ComboBox {
                        anchors.fill: parent
                        model: HexControllerInst.propertyOptions[modelData] || []

                        currentIndex: find(stagedChanges[modelData] !== undefined ?
                            stagedChanges[modelData] :
                            HexControllerInst.currentProperties[modelData])

                        onActivated: {
                            let tmp = stagedChanges;
                            tmp[modelData] = currentValue;
                            stagedChanges = Object.assign({}, tmp);
                            HexControllerInst.selectPropertyBytes(modelData);
                        }
                    }
                }
            }
        }
    }

    Button {
        Layout.alignment: Qt.AlignRight
        Layout.margins: 10
        text: "Apply Changes"
        enabled: Object.keys(stagedChanges).length > 0

        onClicked: {
            HexControllerInst.applyStagedChanges(HexControllerInst.currentProperties["PacketStartOffset"], stagedChanges);
            stagedChanges = {};
        }
    }

    Rectangle {
        Layout.fillWidth: true
        Layout.margins: 10
        Layout.preferredHeight: 50
        color: "#ffe6e6"
        border.color: "red"
        border.width: 1
        visible: HexControllerInst.currentProperties["HasError"] === true

        Text {
            anchors.centerIn: parent
            text: HexControllerInst.currentProperties["ErrorMessage"] || ""
            color: "red"
            font.bold: true
        }
    }
}