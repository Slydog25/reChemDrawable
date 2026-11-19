import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: window
    visible: true
    width: 1400
    height: 1000
    minimumWidth: 800
    minimumHeight: 600
    title: "reChemDrawable"
    color: "#fbfaf5"

    property string activeTool: "draw"
    property string activeRing: "cyclohexane"
    property int selectedAtom: -1
    property bool gridSnapEnabled: false
    property var moleculeProperties: ({})
    property string reactionLabel: "conditions"
    // Read by main.cpp: while a dialog covers the canvas, a finger may use it.
    readonly property bool dialogOpen: openDialog.visible || discardDialog.visible

    Shortcut { sequences: [StandardKey.Undo]; onActivated: editHistory.undo() }
    Shortcut { sequences: [StandardKey.Redo]; onActivated: editHistory.redo() }
    Shortcut { sequences: [StandardKey.Copy]; onActivated: editor.copySelection() }
    Shortcut { sequences: [StandardKey.Paste]; onActivated: editor.pasteSelection() }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 16

            Toolbar {
                Layout.preferredWidth: 252
                Layout.fillHeight: true
                activeTool: window.activeTool
                gridSnapEnabled: window.gridSnapEnabled
                onToolSelected: function(tool) { window.activeTool = tool }
                onGridSnapToggled: function(enabled) { window.gridSnapEnabled = enabled }
                onUndoRequested: editHistory.undo()
                onRedoRequested: editHistory.redo()
                onClearRequested: { editHistory.clearAll(); window.selectedAtom = -1 }
                onNewRequested: window.confirmDiscard(function() { document.newDocument(); window.selectedAtom = -1 })
                onOpenRequested: openDialog.open()
                onSaveRequested: document.save()
                onExportMolRequested: document.exportMolfile()
                onExportSvgRequested: document.exportSvg()
                onExportSmilesRequested: document.exportSmiles()
                onZoomInRequested: editor.zoomIn()
                onZoomOutRequested: editor.zoomOut()
                onFitRequested: editor.fitToContent()
                onCleanupRequested: molecule.cleanUpStructure()
                onRotateRequested: editor.rotateSelection()
                onDeleteSelectionRequested: editor.deleteSelection()
                onCopyRequested: editor.copySelection()
                onPasteRequested: editor.pasteSelection()
                onMirrorRequested: function(horizontal) { editor.mirrorSelection(horizontal) }
                onAlignRequested: function(alignment) { editor.alignSelection(alignment) }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 10

                RowLayout {
                    Layout.fillWidth: true
                    Label {
                        text: "reChemDrawable"
                        font.pixelSize: 34
                        font.bold: true
                        color: "#111111"
                    }
                    Label {
                        objectName: "documentTitle"
                        // "•" marks unsaved changes.
                        text: document.title + (document.dirty ? " •" : "")
                        color: "#5a5a55"
                        font.pixelSize: 22
                        Layout.leftMargin: 12
                    }
                    Item { Layout.fillWidth: true }
                    Label {
                        text: moleculeProperties.atomCount > 0 ? moleculeProperties.formula + "  •  MW " + Number(moleculeProperties.molecularWeight).toFixed(2) :
                              "Tap the canvas to place an atom"
                        color: "#252525"
                        font.pixelSize: 19
                    }
                }

                ElementPalette {
                    Layout.fillWidth: true
                    selectedAtom: window.selectedAtom
                    onElementSelected: function(element) { molecule.setAtomElement(window.selectedAtom, element) }
                    onChargeRequested: function(charge) { molecule.setAtomCharge(window.selectedAtom, charge) }
                    onGroupRequested: function(group) { molecule.stampFunctionalGroup(group, window.selectedAtom) }
                }
                RowLayout {
                    Layout.fillWidth: true
                    RingTemplatePicker {
                        selectedRing: window.activeRing
                        onRingSelected: function(ring) { window.activeRing = ring; window.activeTool = "ring" }
                    }
                    Label { text: "Text"; font.pixelSize: 19; font.bold: true }
                    TextField {
                        id: reactionLabelField
                        Layout.preferredWidth: 230
                        Layout.preferredHeight: 52
                        text: window.reactionLabel
                        placeholderText: "conditions"
                        font.pixelSize: 19
                        onTextChanged: window.reactionLabel = text
                    }
                    Item { Layout.fillWidth: true }
                }
                // Tap-only label choices, since the tablet build has no on-screen keyboard.
                Flow {
                    Layout.fillWidth: true
                    spacing: 8
                    Label { text: "Labels"; font.pixelSize: 19; font.bold: true; height: 52; verticalAlignment: Text.AlignVCenter }
                    Repeater {
                        model: ["Δ", "hν", "rt", "0 °C", "reflux", "H₂O", "H₃O⁺", "NaOH", "HCl", "H₂, Pd/C",
                                "NaBH₄", "LiAlH₄", "THF", "EtOH", "cat."]
                        delegate: Button {
                            text: modelData
                            height: 52
                            font.pixelSize: 18
                            checkable: true
                            checked: window.activeTool === "label" && window.reactionLabel === modelData
                            onClicked: { reactionLabelField.text = modelData; window.activeTool = "label" }
                        }
                    }
                }

                MoleculeCanvas {
                    id: editor
                    // main.cpp looks this up to keep finger touches off the canvas.
                    objectName: "moleculeCanvas"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    activeTool: window.activeTool
                    ringTemplate: window.activeRing
                    selectedAtom: window.selectedAtom
                    labelText: window.reactionLabel
                    gridSnapEnabled: window.gridSnapEnabled
                    onSelectedAtomChangedByCanvas: function(atomId) { window.selectedAtom = atomId }
                    onStatus: function(text) { statusLine.text = text }
                }
            }
        }

        Label {
            id: statusLine
            Layout.fillWidth: true
            color: "#6a5a00"
            font.pixelSize: 18
            text: window.gridSnapEnabled ? "Hex grid snap is on. Tap a carbon to extend it; drag from one to place a bond endpoint." :
                                          "Tap a carbon to extend it; drag from one to place a bond endpoint."
            elide: Text.ElideMiddle
        }
    }

    Connections {
        target: molecule
        function onMessage(text) { statusLine.text = text }
        function onChanged() { window.moleculeProperties = molecule.properties() }
    }

    Connections {
        target: reactionLayer
        function onMessage(text) { statusLine.text = text }
    }

    Connections {
        target: document
        function onMessage(text) { statusLine.text = text }
    }

    // Runs `action` now, or after the user agrees to drop unsaved changes.
    property var pendingAction: null
    function confirmDiscard(action) {
        if (!document.dirty) { action(); return }
        pendingAction = action
        discardDialog.open()
    }
    function runPendingAction() {
        const action = pendingAction
        pendingAction = null
        discardDialog.close()
        if (action) action()
    }

    onClosing: function(close) {
        if (!document.dirty) return
        close.accepted = false
        // Discard means discard: no recovery copy on the way out.
        confirmDiscard(function() { document.discardChanges(); Qt.quit() })
    }

    Popup {
        id: discardDialog
        objectName: "discardDialog"
        modal: true
        anchors.centerIn: Overlay.overlay
        width: 560
        padding: 24
        onClosed: window.pendingAction = null
        contentItem: ColumnLayout {
            spacing: 18
            Label {
                Layout.fillWidth: true
                text: "“" + document.title + "” has unsaved changes."
                font.pixelSize: 22
                wrapMode: Text.WordWrap
            }
            RowLayout {
                spacing: 12
                Button { text: "Save"; font.pixelSize: 20; Layout.preferredHeight: 60; onClicked: if (document.save()) window.runPendingAction() }
                Button { text: "Discard"; font.pixelSize: 20; Layout.preferredHeight: 60; onClicked: window.runPendingAction() }
                Button { text: "Cancel"; font.pixelSize: 20; Layout.preferredHeight: 60; onClicked: discardDialog.close() }
            }
        }
    }

    Popup {
        id: openDialog
        objectName: "openDialog"
        modal: true
        anchors.centerIn: Overlay.overlay
        width: Math.min(window.width - 80, 720)
        height: Math.min(window.height - 80, 760)
        padding: 20
        property var entries: []
        onAboutToShow: entries = document.documents()
        contentItem: ColumnLayout {
            spacing: 12
            Label { text: "Open a drawing"; font.pixelSize: 26; font.bold: true }
            Label {
                Layout.fillWidth: true
                visible: openDialog.entries.length === 0
                text: "No saved drawings yet in " + document.directory
                font.pixelSize: 18
                wrapMode: Text.WrapAnywhere
            }
            ListView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                spacing: 8
                model: openDialog.entries
                delegate: Button {
                    width: ListView.view.width
                    height: 72
                    font.pixelSize: 20
                    text: modelData.name + "    " + modelData.modified + (modelData.kind === "molfile" ? "    .mol" : "")
                    onClicked: {
                        const path = modelData.path
                        openDialog.close()
                        window.confirmDiscard(function() {
                            if (document.open(path)) { window.selectedAtom = -1; editor.fitToContent() }
                        })
                    }
                }
            }
            Button { text: "Cancel"; font.pixelSize: 20; Layout.preferredHeight: 60; onClicked: openDialog.close() }
        }
    }

    Component.onCompleted: moleculeProperties = molecule.properties()
}
