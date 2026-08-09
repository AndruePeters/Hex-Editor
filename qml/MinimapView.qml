import QtQuick
import QtQuick.Controls

Item {
    id: root
    // property TableView targetView
    property GridView targetView
    function refresh() {
        minimapImage.source = "image://hexminimap/render?" + Math.random()
    }

    Rectangle {
        anchors.fill: parent
        color: "#1e1e1e"
        opacity: 0.8
    }

    Image {
        id: minimapImage
        anchors.fill: parent
        source: "image://hexminimap/render"
        fillMode: Image.Stretch
    }

    MouseArea {
        anchors.fill: parent
        onClicked: (mouse) => {
            if (!targetView) return;
            let clickRatio = mouse.y / root.height;
            let targetY = (clickRatio * targetView.contentHeight) - (targetView.height / 2);
            targetView.contentY = Math.max(0, Math.min(targetY, targetView.contentHeight - targetView.height));
        }
    }

    Rectangle {
        id: viewportIndicator
        width: parent.width
        color: palette.highlight
        opacity: 0.3
        border.color: palette.highlight
        border.width: 1
        height: targetView ? Math.max(16, (targetView.height / Math.max(1, targetView.contentHeight)) * root.height) : 0
        y: targetView ? (targetView.contentY / Math.max(1, targetView.contentHeight)) * root.height : 0

        MouseArea {
            anchors.fill: parent
            drag.target: viewportIndicator
            drag.axis: Drag.YAxis
            drag.minimumY: 0
            drag.maximumY: root.height - viewportIndicator.height
            onPositionChanged: {
                if (drag.active && targetView) {
                    let ratio = viewportIndicator.y / (root.height - viewportIndicator.height);
                    targetView.contentY = ratio * (targetView.contentHeight - targetView.height);
                }
            }
        }
    }
}