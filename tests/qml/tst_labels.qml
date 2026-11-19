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

    ElementPalette {
        id: palette
        width: 800
        selectedAtom: 1
    }

    TestCase {
        name: "AtomLabels"
        when: windowShown

        function initTestCase() { failOnWarning(/.*/) }

        function init() {
            document.newDocument()
            molecule.circledCharges = false
            canvas.refresh()
        }

        function test_labelsRenderWithoutWarnings() {
            const carbon = molecule.addAtom(200, 200)
            const oxygen = molecule.extendFromAtom(carbon, 240, 200, "O")
            molecule.setAtomCharge(oxygen, -1)
            molecule.cycleLonePairs(oxygen)
            molecule.cycleRadical(carbon)
            molecule.applyAbbreviation(carbon, "Boc")
            molecule.addAtom(300, 300, "Xx")
            molecule.circledCharges = true
            canvas.refresh()
            wait(50)
            const atoms = molecule.atoms()
            compare(atoms[1].labelText, "O")
            compare(atoms[2].problem, true)
        }

        function test_paletteListsAbbreviations() {
            const names = molecule.abbreviationButtons()
            verify(names.length > 20)
            compare(names[0].name, "Me")
        }
    }
}
