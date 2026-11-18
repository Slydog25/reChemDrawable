import QtQuick
import QtTest
import "../../src/qml"

Item {
    width: 800
    height: 600

    MoleculeCanvas {
        id: canvas
        anchors.fill: parent
    }

    TestCase {
        name: "MoleculeCanvas"
        when: windowShown

        function initTestCase() {
            // Any QML warning (a broken binding, an undefined name) fails the test.
            failOnWarning(/.*/)
        }

        function init() {
            document.newDocument()
            canvas.activeTool = "draw"
            canvas.zoom = 1
            canvas.panX = 0
            canvas.panY = 0
            canvas.refresh()
        }

        // MouseArea reports a click after a drag; Pan used to turn that into a new atom.
        function test_panDoesNotAddAtoms() {
            canvas.activeTool = "pan"
            mousePress(canvas, 200, 200)
            mouseMove(canvas, 260, 230)
            mouseMove(canvas, 300, 260)
            mouseRelease(canvas, 300, 260)
            compare(molecule.atoms().length, 0)
            verify(canvas.panX > 0)
        }

        function test_tapPlacesThenExtendsInOneUndoStep() {
            mouseClick(canvas, 200, 200)
            compare(molecule.atoms().length, 1)
            mouseClick(canvas, 200, 200)
            compare(molecule.atoms().length, 2)
            compare(molecule.bonds().length, 1)
            verify(molecule.undo())
            compare(molecule.atoms().length, 1)
        }

        function test_refusedExtensionLeavesNothingToUndo() {
            molecule.addAtom(200, 200, "F")
            mouseClick(canvas, 200, 200)
            mouseClick(canvas, 200, 200)
            compare(molecule.atoms().length, 2)
            verify(molecule.undo())
            compare(molecule.atoms().length, 1)
            verify(molecule.undo())
            compare(molecule.atoms().length, 0)
            verify(!molecule.undo())
        }

        function test_zoomButtonsAndFit() {
            canvas.zoomIn()
            verify(canvas.zoom > 1)
            canvas.zoomOut()
            fuzzyCompare(canvas.zoom, 1, 1e-9)
            molecule.addAtom(1000, 1000)
            molecule.addAtom(3000, 1000)
            canvas.fitToContent()
            verify(canvas.zoom < 1)
            const left = 1000 * canvas.zoom + canvas.panX
            const right = 3000 * canvas.zoom + canvas.panX
            verify(left >= 0)
            verify(right <= canvas.width)
        }

        function test_pendingArrowResetsOnToolChange() {
            const input = findChild(canvas, "canvasInput")
            canvas.activeTool = "arrow"
            mouseClick(canvas, 100, 100)
            verify(input.awaitingArrowEnd)
            canvas.activeTool = "draw"
            verify(!input.awaitingArrowEnd)
        }

        function test_selectionIsPrunedWhenAtomsDisappear() {
            const atom = molecule.addAtom(200, 200)
            canvas.activeTool = "select"
            mouseClick(canvas, 200, 200)
            compare(canvas.selectedAtoms.length, 1)
            molecule.removeAtom(atom)
            compare(canvas.selectedAtoms.length, 0)
        }
    }
}
