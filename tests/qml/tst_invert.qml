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
        name: "InvertTool"
        when: windowShown

        function initTestCase() { failOnWarning(/.*/) }

        function init() {
            document.newDocument()
            canvas.activeTool = "invert"
            canvas.zoom = 1
            canvas.panX = 0
            canvas.panY = 0
            canvas.refresh()
        }

        function label() {
            const d = molecule.stereoDescriptors()
            return d.length ? d[0].label : ""
        }

        function test_tapStereocentreFlipsRS() {
            verify(molecule.importSmiles("N[C@@H](C)C(=O)O", 400, 300))
            compare(label(), "S")
            const centre = molecule.atoms()[1]
            mouseClick(canvas, centre.x, centre.y)
            compare(label(), "R")
            verify(molecule.undo())
            compare(label(), "S")
        }

        function test_tapDoubleBondFlipsEZ() {
            verify(molecule.importSmiles("C/C=C/C", 400, 300))
            compare(label(), "E")
            const a = molecule.atoms()[1], b = molecule.atoms()[2]
            mouseClick(canvas, (a.x + b.x) / 2, (a.y + b.y) / 2)
            compare(label(), "Z")
        }
    }
}
