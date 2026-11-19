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

        function test_gridSnapKeepsZigZagOnTheLattice() {
            canvas.gridSnapEnabled = true
            mouseClick(canvas, 200, 200)
            for (let step = 0; step < 4; ++step) {
                const atoms = molecule.atoms()
                const last = atoms[atoms.length - 1]
                mouseClick(canvas, last.x, last.y)
            }
            const atoms = molecule.atoms()
            compare(atoms.length, 5)
            for (let i = 0; i < atoms.length; ++i) {
                const snapped = canvas.snapToHexGrid(atoms[i].x, atoms[i].y)
                fuzzyCompare(atoms[i].x, snapped.x, 1e-6)
                fuzzyCompare(atoms[i].y, snapped.y, 1e-6)
            }
            for (let i = 0; i + 1 < atoms.length; ++i) {
                const dx = atoms[i + 1].x - atoms[i].x, dy = atoms[i + 1].y - atoms[i].y
                fuzzyCompare(Math.sqrt(dx * dx + dy * dy), canvas.bondLength, 1e-6)
            }
            for (let i = 0; i + 2 < atoms.length; ++i) fuzzyCompare(atoms[i].y, atoms[i + 2].y, 1e-6)
            canvas.gridSnapEnabled = false
        }

        function test_eraserStrokeErasesInOneUndoStep() {
            // A three-atom chain plus an arrow, all crossed by one right-button
            // (eraser-end) stroke.
            const a = molecule.addAtom(150, 150)
            const b = molecule.extendFromAtom(a, 198, 150)
            molecule.extendFromAtom(b, 246, 150)
            reactionLayer.addArrow(300, 120, 300, 200)
            mousePress(canvas, 120, 150, Qt.RightButton)
            mouseMove(canvas, 200, 152, -1, Qt.RightButton)
            mouseMove(canvas, 330, 150, -1, Qt.RightButton)
            mouseRelease(canvas, 330, 150, Qt.RightButton)
            compare(molecule.atoms().length, 0)
            compare(reactionLayer.items().length, 0)
            verify(editHistory.undo())
            compare(molecule.atoms().length, 3)
            compare(molecule.bonds().length, 2)
            compare(reactionLayer.items().length, 1)
        }

        function test_eraseToolErasesByDragging() {
            const a = molecule.addAtom(150, 300)
            molecule.extendFromAtom(a, 198, 300)
            canvas.activeTool = "erase"
            mousePress(canvas, 140, 300)
            mouseMove(canvas, 210, 300, -1, Qt.LeftButton)
            mouseRelease(canvas, 210, 300)
            compare(molecule.atoms().length, 0)
            verify(editHistory.undo())
            compare(molecule.atoms().length, 2)
        }

        // Tapping just inside a single bond of a benzene with the benzene ring tool
        // used to do nothing (or, on a double bond, stack a ring inside the old one).
        function test_ringToolFusesBenzeneOnTheOutside() {
            molecule.stampRing("benzene", 300, 300)
            const atoms = molecule.atoms()
            const bonds = molecule.bonds()
            let shared = null
            for (let i = 0; i < bonds.length; ++i) if (bonds[i].order === 1) { shared = bonds[i]; break }
            let a, b
            for (let i = 0; i < atoms.length; ++i) { if (atoms[i].id === shared.atomA) a = atoms[i]; if (atoms[i].id === shared.atomB) b = atoms[i] }
            const mx = (a.x + b.x) / 2, my = (a.y + b.y) / 2
            const toward = Math.sqrt((300 - mx) * (300 - mx) + (300 - my) * (300 - my))
            canvas.ringTemplate = "benzene"
            canvas.activeTool = "ring"
            mouseClick(canvas, mx + (300 - mx) / toward * 8, my + (300 - my) / toward * 8)
            compare(molecule.formula(), "C10H8")
            const all = molecule.atoms()
            for (let i = 0; i < all.length; ++i)
                for (let j = i + 1; j < all.length; ++j)
                    verify(Math.abs(all[i].x - all[j].x) + Math.abs(all[i].y - all[j].y) > 20)
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
