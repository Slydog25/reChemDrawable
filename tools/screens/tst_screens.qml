// Renders the whole app at the Paper Pro's resolution with a sample scheme, for
// visual review. Not part of the test suite:
//   QT_QPA_PLATFORM=offscreen build-pc/qml_tests -input tools/screens
// writes build-pc/screen-main.png.
import QtQuick
import QtTest

TestCase {
    name: "Screens"
    when: windowShown

    function test_main() {
        const component = Qt.createComponent("../../src/qml/Main.qml")
        compare(component.status, Component.Ready, component.errorString())
        const window = component.createObject(null, { width: 1620, height: 2160 })
        verify(window)
        document.newDocument()

        // Ethanol with the OH on the end, a lone pair, and a charged amine.
        const c1 = molecule.addAtom(120, 160)
        const c2 = molecule.extendFromAtom(c1, molecule.suggestedBondEnd(c1).x, molecule.suggestedBondEnd(c1).y)
        const end = molecule.suggestedBondEnd(c2)
        const o = molecule.extendFromAtom(c2, end.x, end.y, "O")
        molecule.cycleLonePairs(o); molecule.cycleLonePairs(o)
        // Benzene with an OMe abbreviation and a stereocentre on a chain.
        molecule.stampRing("benzene", 520, 170)
        const ringAtom = molecule.atoms()[3].id
        molecule.applyAbbreviation(ringAtom, "OMe")
        // Arrow kinds and labels.
        reactionLayer.addArrowKind("forward", 250, 170, 400, 170)
        reactionLayer.addLabelSnapped("NaBH₄", 325, 160)
        reactionLayer.addLabelSnapped("MeOH", 325, 182)
        reactionLayer.addArrowKind("equilibrium", 680, 170, 820, 170)
        reactionLayer.addArrowKind("curved", 100, 330, 220, 330, 160, 260)
        reactionLayer.addLabel("1", 140, 230)
        const n = molecule.addAtom(900, 160, "N")
        molecule.setAtomCharge(n, 1)
        wait(400)
        const image = grabImage(window.contentItem)
        image.save("/workspace/build-pc/screen-main.png")
        window.destroy()
    }
}
