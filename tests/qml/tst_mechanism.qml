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
        name: "Mechanism"
        when: windowShown

        function initTestCase() { failOnWarning(/.*/) }

        function init() {
            document.newDocument()
            canvas.activeTool = "mechanism"
            canvas.mechanismKind = "dplus"
            canvas.zoom = 1
            canvas.panX = 0
            canvas.panY = 0
            canvas.refresh()
        }

        function test_tapPlacesPartialChargesAndDagger() {
            mouseClick(canvas, 100, 100)
            canvas.mechanismKind = "dminus"
            mouseClick(canvas, 200, 100)
            canvas.mechanismKind = "dagger"
            mouseClick(canvas, 300, 100)
            const items = reactionLayer.items()
            compare(items.length, 3)
            compare(items[0].text, "δ+")
            compare(items[1].text, "δ−")
            compare(items[2].type, "dagger")
            verify(editHistory.undo())
            compare(reactionLayer.items().length, 2)
        }

        function test_dragDrawsBracketAndEraserRemovesIt() {
            canvas.mechanismKind = "bracketDagger"
            mousePress(canvas, 100, 100)
            mouseMove(canvas, 200, 150)
            mouseMove(canvas, 300, 220)
            mouseRelease(canvas, 300, 220)
            const items = reactionLayer.items()
            compare(items.length, 1)
            compare(items[0].type, "bracket")
            compare(items[0].kind, "dagger")
            canvas.activeTool = "erase"
            mouseClick(canvas, 100, 160)
            compare(reactionLayer.items().length, 0)
            verify(editHistory.undo())
            compare(reactionLayer.items().length, 1)
        }

        function test_orbitalFollowsDragDirectionAndMoveToolMovesIt() {
            canvas.mechanismKind = "orbitalShaded"
            mousePress(canvas, 300, 300)
            mouseMove(canvas, 340, 300)
            mouseRelease(canvas, 360, 300)
            let items = reactionLayer.items()
            compare(items.length, 1)
            compare(items[0].type, "orbital")
            compare(items[0].kind, "shaded")
            verify(items[0].x2 > items[0].x1)
            compare(Math.round(items[0].y2), 300)
            canvas.activeTool = "move"
            mousePress(canvas, 320, 300)
            mouseMove(canvas, 340, 340)
            mouseRelease(canvas, 340, 340)
            items = reactionLayer.items()
            compare(Math.round(items[0].x1), 320)
            compare(Math.round(items[0].y1), 340)
        }

        function test_partialBondToolTogglesDashedBond() {
            const a = molecule.addAtom(100, 400)
            const b = molecule.addAtom(148, 400)
            molecule.addBond(a, b)
            canvas.refresh()
            canvas.mechanismKind = "partial"
            mouseClick(canvas, 124, 400)
            compare(molecule.bonds()[0].style, "partial")
            mouseClick(canvas, 124, 400)
            compare(molecule.bonds()[0].style, "plain")
        }
    }
}
