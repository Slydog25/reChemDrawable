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

    // Close-up of drawing features: templates, double-bond sides, stereo labels.
    function test_drawing() {
        const component = Qt.createComponent("../../src/qml/Main.qml")
        const window = component.createObject(null, { width: 1620, height: 2160 })
        document.newDocument()
        molecule.stampRing("pyridine", 150, 150)
        molecule.stampRing("furan", 330, 150)
        molecule.stampRing("indole", 520, 150)
        molecule.stampRing("chair", 760, 150)
        molecule.stampRing("imidazole", 950, 150)
        // (R)/(S)-butan-2-ol: C1-C2(OH, wedge to methyl)-C3-C4
        const a = molecule.addAtom(150, 400)
        const b = molecule.extendFromAtom(a, molecule.suggestedBondEnd(a).x, molecule.suggestedBondEnd(a).y)
        const c = molecule.extendFromAtom(b, molecule.suggestedBondEnd(b).x, molecule.suggestedBondEnd(b).y)
        molecule.extendFromAtom(c, molecule.suggestedBondEnd(c).x, molecule.suggestedBondEnd(c).y)
        const oh = molecule.extendFromAtom(b, molecule.suggestedBondEnd(b).x, molecule.suggestedBondEnd(b).y, "O")
        const bonds = molecule.bonds()
        molecule.setBondStyle(bonds[bonds.length - 1].id, "wedge")
        // But-2-ene with an offset double bond
        const e1 = molecule.addAtom(450, 400)
        const e2 = molecule.extendFromAtom(e1, molecule.suggestedBondEnd(e1).x, molecule.suggestedBondEnd(e1).y)
        const e3 = molecule.extendFromAtom(e2, molecule.suggestedBondEnd(e2).x, molecule.suggestedBondEnd(e2).y)
        molecule.extendFromAtom(e3, molecule.suggestedBondEnd(e3).x, molecule.suggestedBondEnd(e3).y)
        const eb = molecule.bonds()
        molecule.cycleBondOrder(eb[eb.length - 2].id)
        molecule.cycleBondStyle(eb[eb.length - 2].id)
        // A radical and a fishhook
        const r = molecule.addAtom(750, 400)
        molecule.cycleRadical(r)
        reactionLayer.addArrowKind("fishhook", 800, 420, 900, 420, 850, 360)
        window.stereoLabelsVisible = true
        wait(400)
        const editor = findChild(window, "moleculeCanvas")
        grabImage(editor).save("/workspace/build-pc/screen-drawing.png")
        window.destroy()
    }
}
