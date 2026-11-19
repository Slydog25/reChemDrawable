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
        name: "ChainTool"
        when: windowShown

        function initTestCase() { failOnWarning(/.*/) }

        function init() {
            document.newDocument()
            canvas.activeTool = "chain"
            canvas.gridSnapEnabled = false
            canvas.zoom = 1
            canvas.panX = 0
            canvas.panY = 0
            canvas.refresh()
        }

        function drag(x1, y1, x2, y2) {
            mousePress(canvas, x1, y1)
            mouseMove(canvas, (x1 + x2) / 2, (y1 + y2) / 2)
            mouseMove(canvas, x2, y2)
            mouseRelease(canvas, x2, y2)
        }

        function test_lengthFollowsDrag() {
            const step = 48 * Math.cos(Math.PI / 6)
            drag(100, 300, 100 + 5 * step, 300)
            compare(molecule.atoms().length, 6) // 5 bonds, start atom included from empty canvas
            compare(molecule.bonds().length, 5)
        }

        function test_bondLengths() {
            drag(100, 300, 400, 300)
            const atoms = molecule.atoms(), bonds = molecule.bonds()
            verify(bonds.length >= 5)
            for (let i = 0; i < bonds.length; ++i) {
                const a = atoms[i], b = atoms[i + 1]
                fuzzyCompare(Math.hypot(b.x - a.x, b.y - a.y), 48, 0.001)
            }
        }

        function test_singleUndoStep() {
            drag(100, 300, 400, 300)
            verify(molecule.atoms().length > 3)
            editHistory.undo()
            compare(molecule.atoms().length, 0)
        }

        function test_fromAtomCountsNewCarbons() {
            molecule.addAtom(200, 300)
            canvas.refresh()
            const step = 48 * Math.cos(Math.PI / 6)
            drag(200, 300, 200 + 3 * step, 300)
            compare(molecule.atoms().length, 4)
        }

        function test_refusesFullAtom() {
            const o = molecule.addAtom(200, 300, "O")
            molecule.extendFromAtom(o, 248, 300)
            molecule.extendFromAtom(o, 152, 300)
            canvas.refresh()
            drag(200, 300, 200, 100)
            compare(molecule.atoms().length, 3)
        }

        function test_chainOnGrid() {
            canvas.gridSnapEnabled = true
            drag(103, 297, 400, 250)
            const atoms = molecule.atoms()
            verify(atoms.length > 3)
            for (let i = 0; i < atoms.length; ++i) {
                const g = canvas.snapToHexGrid(atoms[i].x, atoms[i].y)
                fuzzyCompare(g.x, atoms[i].x, 0.001)
                fuzzyCompare(g.y, atoms[i].y, 0.001)
            }
        }

        function test_tapDoesNothing() {
            mouseClick(canvas, 300, 300)
            compare(molecule.atoms().length, 0)
        }
    }
}
