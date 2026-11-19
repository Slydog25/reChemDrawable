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
        name: "Interaction"
        when: windowShown

        function initTestCase() { failOnWarning(/.*/) }

        function init() {
            document.newDocument()
            canvas.activeTool = "select"
            canvas.boldLines = false
            canvas.zoom = 1
            canvas.panX = 0
            canvas.panY = 0
            canvas.refresh()
        }

        function drag(points) {
            mousePress(canvas, points[0][0], points[0][1])
            for (let i = 1; i < points.length; ++i) mouseMove(canvas, points[i][0], points[i][1])
            mouseRelease(canvas, points[points.length - 1][0], points[points.length - 1][1])
        }

        function test_lassoSelectsAtomsInsideConcaveLoop() {
            const inLeft = molecule.addAtom(115, 120)
            const inRight = molecule.addAtom(285, 120)
            const inBottom = molecule.addAtom(200, 285)
            molecule.addAtom(200, 130)  // in the notch of the U
            molecule.addAtom(400, 300)
            drag([[100, 100], [130, 100], [130, 270], [270, 270], [270, 100], [300, 100], [300, 300], [100, 300], [100, 110]])
            compare(canvas.selectedAtoms.length, 3)
            verify(canvas.selectedAtoms.indexOf(inLeft) >= 0)
            verify(canvas.selectedAtoms.indexOf(inRight) >= 0)
            verify(canvas.selectedAtoms.indexOf(inBottom) >= 0)
            compare(canvas.lassoPoints.length, 0)
        }

        function test_lassoPathIsKeptWhileDragging() {
            mousePress(canvas, 100, 100)
            mouseMove(canvas, 150, 120)
            mouseMove(canvas, 200, 160)
            verify(canvas.lassoPoints.length >= 3)
            mouseRelease(canvas, 200, 160)
            compare(canvas.lassoPoints.length, 0)
        }

        function test_tapSelectsAndCtrlTapToggles() {
            const a = molecule.addAtom(200, 200)
            const b = molecule.addAtom(300, 200)
            mouseClick(canvas, 200, 200)
            compare(canvas.selectedAtoms, [a])
            mouseClick(canvas, 300, 200, Qt.LeftButton, Qt.ControlModifier)
            compare(canvas.selectedAtoms.length, 2)
            mouseClick(canvas, 200, 200, Qt.LeftButton, Qt.ControlModifier)
            compare(canvas.selectedAtoms, [b])
            mouseClick(canvas, 500, 500)
            compare(canvas.selectedAtoms.length, 0)
        }

        function test_duplicateSelectionIsOneUndoStepOffsetDownRight() {
            const a = molecule.addAtom(100, 100)
            const b = molecule.addAtom(150, 100)
            molecule.addBond(a, b, 1)
            canvas.selectedAtoms = [a, b]
            canvas.duplicateSelection()
            compare(molecule.atoms().length, 4)
            compare(molecule.bonds().length, 2)
            const copy = molecule.atoms()[2]
            compare(copy.x, 100 + 2 * canvas.bondLength)
            compare(copy.y, 100 + 2 * canvas.bondLength)
            compare(canvas.selectedAtoms.length, 2)
            verify(canvas.selectedAtoms.indexOf(a) < 0)
            verify(molecule.undo())
            compare(molecule.atoms().length, 2)
            compare(molecule.bonds().length, 1)
        }

        function test_styleToolCyclesDoubleBondSides() {
            const a = molecule.addAtom(100, 100)
            const b = molecule.addAtom(180, 100)
            molecule.addBond(a, b, 2)
            canvas.activeTool = "stereo"
            const sides = []
            for (let i = 0; i < 3; ++i) {
                mouseClick(canvas, 140, 100)
                sides.push(molecule.bonds()[0].doubleSide)
            }
            compare(sides, [1, -1, 0])
            compare(molecule.bonds()[0].style, "plain")
        }

        function test_panZoomBy() {
            canvas.panZoomBy(30, -10, 1, 0, 0)
            compare(canvas.panX, 30)
            compare(canvas.panY, -10)
            const before = canvas.modelPoint(200, 100)
            canvas.panZoomBy(0, 0, 2, 200, 100)
            compare(canvas.zoom, 2)
            const after = canvas.modelPoint(200, 100)
            fuzzyCompare(after.x, before.x, 1e-6)
            fuzzyCompare(after.y, before.y, 1e-6)
            canvas.panZoomBy(0, 0, 100, 200, 100)
            compare(canvas.zoom, canvas.maximumZoom)
            canvas.panZoomBy(0, 0, 0.0001, 200, 100)
            compare(canvas.zoom, canvas.minimumZoom)
        }

        function test_boldLinesIsAViewSetting() {
            compare(canvas.bondWidth, 3)
            canvas.boldLines = true
            compare(canvas.bondWidth, 4.5)
            verify(canvas.labelScale > 1.1)
            wait(50)
            canvas.boldLines = false
            compare(canvas.bondWidth, 3)
        }
    }
}
