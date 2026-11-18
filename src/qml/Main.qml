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
                touchDrawingEnabled: inputSettings.touchDrawingEnabled
                gridSnapEnabled: window.gridSnapEnabled
                onToolSelected: window.activeTool = tool
                onTouchDrawingToggled: function(enabled) { inputSettings.touchDrawingEnabled = enabled }
                onGridSnapToggled: function(enabled) { window.gridSnapEnabled = enabled }
                onUndoRequested: editHistory.undo()
                onRedoRequested: editHistory.redo()
                onClearRequested: { editHistory.clearAll(); window.selectedAtom = -1 }
                onSaveRequested: {
                    // A .mol file holds only the structure; say so instead of dropping annotations silently.
                    if (molecule.saveDefaultMolfile() && reactionLayer.items().length > 0)
                        statusLine.text = "Saved the structure only. Use Scheme + to keep arrows and labels."
                }
                onLoadRequested: { molecule.loadDefaultMolfile(); window.selectedAtom = -1 }
                onExportSvgRequested: reactionLayer.saveDefaultSvg()
                onCleanupRequested: molecule.cleanUpStructure()
                onRotateRequested: editor.rotateSelection()
                onDeleteSelectionRequested: editor.deleteSelection()
                onCopyRequested: editor.copySelection()
                onPasteRequested: editor.pasteSelection()
                onMirrorRequested: function(horizontal) { editor.mirrorSelection(horizontal) }
                onAlignRequested: function(alignment) { editor.alignSelection(alignment) }
                onSaveSchemeRequested: reactionLayer.saveDefaultScheme()
                onLoadSchemeRequested: { reactionLayer.loadDefaultScheme(); window.selectedAtom = -1 }
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
                    Item { Layout.fillWidth: true }
                    Label {
                        text: editor.selectedAtoms.length > 0 ? moleculeProperties.formula + "  •  MW " + Number(moleculeProperties.molecularWeight).toFixed(2) :
                              (window.selectedAtom < 0 ? "Tap an atom to begin a bond" : "Atom " + window.selectedAtom + " selected")
                        color: "#252525"
                        font.pixelSize: 19
                    }
                }

                ElementPalette {
                    Layout.fillWidth: true
                    selectedAtom: window.selectedAtom
                    onElementSelected: molecule.setAtomElement(window.selectedAtom, element)
                    onChargeRequested: molecule.setAtomCharge(window.selectedAtom, charge)
                    onGroupRequested: molecule.stampFunctionalGroup(group, window.selectedAtom)
                }
                RowLayout {
                    Layout.fillWidth: true
                    RingTemplatePicker {
                        selectedRing: window.activeRing
                        onRingSelected: { window.activeRing = ring; window.activeTool = "ring" }
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

                MoleculeCanvas {
                    id: editor
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    activeTool: window.activeTool
                    ringTemplate: window.activeRing
                    selectedAtom: window.selectedAtom
                    labelText: window.reactionLabel
                    gridSnapEnabled: window.gridSnapEnabled
                    onSelectedAtomChangedByCanvas: window.selectedAtom = atomId
                    onStatus: statusLine.text = text
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

    Component.onCompleted: moleculeProperties = molecule.properties()
}
