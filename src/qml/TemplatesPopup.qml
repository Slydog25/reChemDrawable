import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Projections, amino acids, nucleobases, reagents and "My templates". Tapping a card asks
// the window to place it; "Save selection" stores the canvas selection as a new template.
Popup {
    id: popup
    objectName: "templatesPopup"
    parent: Overlay.overlay
    anchors.centerIn: Overlay.overlay
    modal: true
    padding: 18
    width: 960
    height: 900
    property int selectionCount: 0
    property string category: "projections"
    property var cards: []
    property string pendingDelete: ""
    property string pendingDeleteName: ""
    signal templateChosen(string id)
    signal saveSelectionRequested()
    readonly property bool anyOpen: visible || confirm.visible

    function refresh() { cards = templateLibrary.items(category) }
    onCategoryChanged: refresh()
    onAboutToShow: refresh()
    Connections {
        target: templateLibrary
        function onMineChanged() { popup.refresh() }
    }

    contentItem: ColumnLayout {
        spacing: 12
        RowLayout {
            Layout.fillWidth: true
            Label { text: "Templates"; font.pixelSize: 26; font.bold: true; Layout.fillWidth: true }
            Button {
                objectName: "saveTemplateButton"
                text: "Save selection as template"
                font.pixelSize: 19
                enabled: popup.selectionCount > 0
                onClicked: popup.saveSelectionRequested()
            }
            Button { objectName: "templatesCloseButton"; text: "Close"; font.pixelSize: 19; onClicked: popup.close() }
        }
        Row {
            spacing: 8
            Repeater {
                model: templateLibrary.categories()
                delegate: Button {
                    objectName: "templateCategory_" + modelData.id
                    text: modelData.name
                    font.pixelSize: 20
                    height: 58
                    checkable: true
                    checked: popup.category === modelData.id
                    onClicked: popup.category = modelData.id
                }
            }
        }
        Label {
            visible: popup.category === "mine" && popup.cards.length === 0
            text: "Nothing saved yet. Select atoms on the canvas, then tap \"Save selection as template\"."
            font.pixelSize: 19
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }
        Flickable {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: width
            contentHeight: grid.height
            Flow {
                id: grid
                width: parent.width
                spacing: 10
                Repeater {
                    model: popup.cards
                    delegate: Rectangle {
                        objectName: "templateCard_" + modelData.id
                        width: 214
                        height: 190
                        radius: 8
                        border.width: 2
                        border.color: "#171717"
                        color: tap.pressed ? "#d6d2c5" : "#ffffff"
                        TemplatePreview {
                            x: 6; y: 6
                            width: parent.width - 12
                            height: 118
                            newmanRadius: molecule.newmanCircleRadius
                            data: templateLibrary.preview(modelData.id)
                        }
                        Label {
                            x: 8; y: 126
                            width: parent.width - 16
                            text: modelData.name
                            font.pixelSize: 18
                            font.bold: true
                            elide: Text.ElideRight
                        }
                        Label {
                            x: 8; y: 152
                            width: parent.width - (modelData.deletable ? 80 : 16)
                            text: modelData.formula
                            font.pixelSize: 16
                            color: "#5a5a55"
                            elide: Text.ElideRight
                        }
                        MouseArea {
                            id: tap
                            anchors.fill: parent
                            onClicked: popup.templateChosen(modelData.id)
                        }
                        Button {
                            visible: modelData.deletable
                            objectName: "deleteTemplate_" + modelData.id
                            text: "Delete"
                            font.pixelSize: 14
                            width: 70; height: 34
                            x: parent.width - width - 6
                            y: parent.height - height - 6
                            onClicked: { popup.pendingDelete = modelData.id; popup.pendingDeleteName = modelData.name; confirm.open() }
                        }
                    }
                }
            }
        }
    }

    Popup {
        id: confirm
        objectName: "deleteTemplateConfirm"
        parent: Overlay.overlay
        anchors.centerIn: Overlay.overlay
        modal: true
        padding: 18
        contentItem: Column {
            spacing: 14
            Label { text: "Delete template " + popup.pendingDeleteName + "?"; font.pixelSize: 22 }
            Row {
                spacing: 10
                Button {
                    objectName: "confirmDeleteTemplate"
                    text: "Delete"; font.pixelSize: 19; height: 56
                    onClicked: { templateLibrary.deleteMine(popup.pendingDelete); confirm.close() }
                }
                Button { text: "Keep"; font.pixelSize: 19; height: 56; onClicked: confirm.close() }
            }
        }
    }
}
