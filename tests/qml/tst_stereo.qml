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
        name: "StereoLabels"
        when: windowShown

        function initTestCase() { failOnWarning(/.*/) }

        function init() {
            document.newDocument()
            canvas.stereoLabelsVisible = false
            canvas.refresh()
        }

        function test_toggleRepaintsWithDescriptors() {
            const c = molecule.addAtom(300, 300, "C")
            const br = molecule.addAtom(300, 250, "Br")
            const cl = molecule.addAtom(255, 325, "Cl")
            const f = molecule.addAtom(345, 325, "F")
            const wedge = molecule.addBond(c, br, 1)
            molecule.addBond(c, cl, 1)
            molecule.addBond(c, f, 1)
            molecule.setBondStyle(wedge, "wedge")
            canvas.refresh()
            const labels = molecule.stereoDescriptors()
            compare(labels.length, 1)
            compare(labels[0].text, "(S)")
            canvas.stereoLabelsVisible = true
            wait(50)
            canvas.stereoLabelsVisible = false
        }
    }
}
