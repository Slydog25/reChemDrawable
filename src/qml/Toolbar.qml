import QtQuick
import QtQuick.Controls

Rectangle {
    id: root

    property string activeTool: "draw"
    property bool gridSnapEnabled: false
    property bool boldLines: false
    signal toolSelected(string tool)
    signal exitRequested()
    signal undoRequested()
    signal redoRequested()
    signal clearRequested()
    signal newRequested()
    signal openRequested()
    signal saveRequested()
    signal exportMolRequested()
    signal exportSvgRequested()
    signal exportSmilesRequested()
    signal zoomInRequested()
    signal zoomOutRequested()
    signal fitRequested()
    signal cleanupRequested()
    signal rotateRequested()
    signal deleteSelectionRequested()
    signal copyRequested()
    signal pasteRequested()
    signal duplicateRequested()
    signal boldToggled(bool enabled)
    signal mirrorRequested(bool horizontal)
    signal alignRequested(string alignment)
    signal gridSnapToggled(bool enabled)

    implicitWidth: 252
    color: "#f7f5ed"
    border.color: "#171717"
    border.width: 2
    radius: 12

    component RailButton: Button {
        id: button
        property string iconName: ""
        property string caption: ""
        property bool selected: false
        width: 106
        height: 96
        checkable: selected

        background: Rectangle {
            radius: 9
            color: button.selected ? "#171717" : (button.down ? "#d6d2c5" : "#ffffff")
            border.color: "#171717"
            border.width: 2
        }
        contentItem: Item {
            Column {
            width: parent.width
            spacing: 1
            anchors.verticalCenter: parent.verticalCenter
            ToolbarIcon {
                anchors.horizontalCenter: parent.horizontalCenter
                name: button.iconName
                ink: button.selected ? "#ffffff" : "#111111"
            }
            Text {
                width: parent.width
                text: button.caption
                color: button.selected ? "#ffffff" : "#111111"
                font.pixelSize: 15
                font.bold: true
                horizontalAlignment: Text.AlignHCenter
            }
            }
        }
    }

    Flickable {
        anchors.fill: parent
        anchors.margins: 12
        clip: true
        contentWidth: width
        contentHeight: toolGrid.height
        boundsBehavior: Flickable.StopAtBounds

        Grid {
            id: toolGrid
            width: parent.width
            columns: 2
            spacing: 10

            Repeater {
                model: [
                    ["draw", "draw", "Draw"], ["select", "select", "Select"],
                    ["move", "move", "Move"], ["pan", "pan", "Pan"],
                    ["bond", "bond", "Bond"], ["stereo", "style", "Style"],
                    ["ring", "ring", "Ring"], ["chain", "chain", "Chain"], ["arrow", "arrow", "Arrow"],
                    ["label", "text", "Text"], ["erase", "erase", "Erase"]
                ]
                delegate: RailButton {
                    iconName: modelData[1]
                    caption: modelData[2]
                    selected: root.activeTool === modelData[0]
                    onClicked: root.toolSelected(modelData[0])
                }
            }

            RailButton { iconName: "undo"; caption: "Undo"; onClicked: root.undoRequested() }
            RailButton { iconName: "redo"; caption: "Redo"; onClicked: root.redoRequested() }
            RailButton {
                iconName: "grid"
                caption: "Snap grid"
                selected: root.gridSnapEnabled
                onClicked: root.gridSnapToggled(!root.gridSnapEnabled)
            }
            RailButton { iconName: "zoomIn"; caption: "Zoom in"; onClicked: root.zoomInRequested() }
            RailButton { iconName: "zoomOut"; caption: "Zoom out"; onClicked: root.zoomOutRequested() }
            RailButton { iconName: "fit"; caption: "Fit"; onClicked: root.fitRequested() }
            RailButton { iconName: "new"; caption: "New"; onClicked: root.newRequested() }
            RailButton { iconName: "load"; caption: "Open"; onClicked: root.openRequested() }
            RailButton { iconName: "save"; caption: "Save"; onClicked: root.saveRequested() }
            RailButton { iconName: "mol"; caption: ".mol"; onClicked: root.exportMolRequested() }
            RailButton { iconName: "svg"; caption: "SVG"; onClicked: root.exportSvgRequested() }
            RailButton { iconName: "smiles"; caption: "SMILES"; onClicked: root.exportSmilesRequested() }
            RailButton { iconName: "clean"; caption: "Clean"; onClicked: root.cleanupRequested() }
            RailButton { iconName: "rotate"; caption: "Rotate"; onClicked: root.rotateRequested() }
            RailButton { iconName: "flipH"; caption: "Flip H"; onClicked: root.mirrorRequested(true) }
            RailButton { iconName: "flipV"; caption: "Flip V"; onClicked: root.mirrorRequested(false) }
            RailButton { iconName: "alignH"; caption: "Align H"; onClicked: root.alignRequested("horizontal") }
            RailButton { iconName: "alignV"; caption: "Align V"; onClicked: root.alignRequested("vertical") }
            RailButton { iconName: "copy"; caption: "Copy"; onClicked: root.copyRequested() }
            RailButton { iconName: "paste"; caption: "Paste"; onClicked: root.pasteRequested() }
            RailButton { iconName: "duplicate"; caption: "Duplicate"; onClicked: root.duplicateRequested() }
            RailButton {
                iconName: "bold"
                caption: "Bold lines"
                selected: root.boldLines
                onClicked: root.boldToggled(!root.boldLines)
            }
            RailButton { iconName: "delete"; caption: "Delete"; onClicked: root.deleteSelectionRequested() }
            RailButton { iconName: "clear"; caption: "Clear"; onClicked: root.clearRequested() }
            RailButton { objectName: "exitButton"; iconName: "exit"; caption: "Exit"; onClicked: root.exitRequested() }
        }
    }
}
