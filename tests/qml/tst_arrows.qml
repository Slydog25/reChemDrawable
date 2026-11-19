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
        name: "Arrows"
        when: windowShown

        function initTestCase() { failOnWarning(/.*/) }

        function init() {
            document.newDocument()
            canvas.activeTool = "arrow"
            canvas.arrowKind = "forward"
            canvas.zoom = 1
            canvas.panX = 0
            canvas.panY = 0
            canvas.refresh()
        }

        function test_dragDrawsStraightArrowOfKind() {
            canvas.arrowKind = "equilibrium"
            mousePress(canvas, 100, 200)
            mouseMove(canvas, 160, 200)
            mouseMove(canvas, 220, 200)
            mouseRelease(canvas, 300, 200)
            const items = reactionLayer.items()
            compare(items.length, 1)
            compare(items[0].kind, "equilibrium")
            verify(editHistory.undo())
            compare(reactionLayer.items().length, 0)
        }

        function test_dragBendsCurvedArrowTowardPen() {
            canvas.arrowKind = "curved"
            mousePress(canvas, 100, 300)
            for (let i = 1; i <= 10; ++i)
                mouseMove(canvas, 100 + i * 20, 300 + 40 * (1 - Math.pow((i - 5) / 5, 2)))
            mouseRelease(canvas, 300, 300)
            const items = reactionLayer.items()
            compare(items.length, 1)
            compare(items[0].kind, "curved")
            verify(items[0].cy > 300 + 40)
            compare(reactionLayer.itemAt(200, 340, 4), items[0].id)
        }

        function test_tapStartTapEndFallback() {
            canvas.arrowKind = "dashed"
            mouseClick(canvas, 100, 100)
            compare(reactionLayer.items().length, 0)
            mouseClick(canvas, 300, 100)
            const items = reactionLayer.items()
            compare(items.length, 1)
            compare(items[0].kind, "dashed")
        }

        function test_textToolSnapsLabelAboveArrow() {
            reactionLayer.addArrow(100, 200, 300, 200)
            canvas.activeTool = "label"
            canvas.labelText = "THF"
            mouseClick(canvas, 140, 185)
            const items = reactionLayer.items()
            compare(items.length, 2)
            compare(items[1].x1, 200)
            verify(items[1].y1 < 200)
        }
    }
}
