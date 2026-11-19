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
        // Property panel for a caffeine drawing.
        document.newDocument()
        molecule.importSmiles("Cn1cnc2c1c(=O)n(C)c(=O)n2C", 400, 400)
        findChild(window, "propertiesButton").clicked()
        wait(400)
        grabImage(window.contentItem).save("/workspace/build-pc/screen-properties.png")
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

    // SMILES import layouts.
    function test_smilesImport() {
        const component = Qt.createComponent("../../src/qml/Main.qml")
        const window = component.createObject(null, { width: 1620, height: 2160 })
        document.newDocument()
        molecule.importSmiles("Cn1cnc2c1c(=O)n(C)c(=O)n2C", 250, 250)          // caffeine
        molecule.importSmiles("CC(C)Cc1ccc(cc1)C(C)C(=O)O", 750, 250)         // ibuprofen
        molecule.importSmiles("CC(=O)Oc1ccccc1C(=O)O", 1150, 250)             // aspirin
        molecule.importSmiles("CC12CCC3C(CCc4ccccc34)C1CCC2O", 350, 650)      // steroid-like
        molecule.importSmiles("C1CCC2(CC1)CCCC2", 800, 650)                   // spiro[4.5]decane
        molecule.importSmiles("c1ccc2ccccc2c1", 1150, 650)                    // naphthalene
        findChild(window, "moleculeCanvas").fitToContent()
        wait(400)
        grabImage(findChild(window, "moleculeCanvas")).save("/workspace/build-pc/screen-smiles.png")
        window.destroy()
    }

    // Landscape (AppLoad rotated): the window is 2160x1620 and the layout must still fit.
    function test_landscape() {
        const component = Qt.createComponent("../../src/qml/Main.qml")
        const window = component.createObject(null, { width: 2160, height: 1620 })
        document.newDocument()
        molecule.importSmiles("CC(=O)Oc1ccccc1C(=O)O", 700, 300)
        wait(400)
        grabImage(window.contentItem).save("/workspace/build-pc/screen-landscape.png")
        window.destroy()
    }

    // Mechanism marks, a transition-state bracket and the tidy/check buttons.
    function test_mechanism() {
        const component = Qt.createComponent("../../src/qml/Main.qml")
        const window = component.createObject(null, { width: 1620, height: 2160 })
        document.newDocument()
        molecule.importSmiles("C=C", 250, 300)
        molecule.importSmiles("BrBr", 600, 420)
        molecule.importSmiles("BrCCBr", 1250, 330)
        reactionLayer.addLabel("+", 430, 360)
        reactionLayer.addArrowKind("forward", 780, 400, 1000, 380)
        reactionLayer.addLabelSnapped("CH2Cl2", 890, 360)
        findChild(window, "moleculeCanvas").fitToContent()
        wait(200)
        grabImage(window.contentItem).save("/workspace/build-pc/screen-mechanism-before.png")
        editHistory.tidyScheme()
        window.runBalanceCheck()
        const a = molecule.addAtom(300, 800), b = molecule.addAtom(348, 800, "Br"), c = molecule.addAtom(252, 800)
        molecule.addBond(a, b); molecule.setBondStyle(molecule.bonds()[molecule.bonds().length - 1].id, "partial")
        molecule.addBond(a, c)
        reactionLayer.addBracket(200, 740, 420, 860, true)
        reactionLayer.addLabel("δ+", 300, 770)
        reactionLayer.addLabel("δ−", 355, 770)
        reactionLayer.addDagger(700, 800)
        reactionLayer.addOrbital(800, 800, 0, -1, true)
        reactionLayer.addOrbital(900, 800, 1, 0.3, false)
        findChild(window, "moleculeCanvas").fitToContent()
        wait(400)
        grabImage(window.contentItem).save("/workspace/build-pc/screen-mechanism-after.png")
        window.destroy()
    }
}
