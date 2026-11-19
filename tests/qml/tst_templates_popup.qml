import QtQuick
import QtTest

TestCase {
    name: "TemplatesPopup"
    when: windowShown

    property var window: null

    function initTestCase() {
        failOnWarning(/.*/)
        const component = Qt.createComponent("../../src/qml/Main.qml")
        compare(component.status, Component.Ready, component.errorString())
        window = component.createObject(null)
        verify(window)
        wait(50)
    }

    function cleanupTestCase() {
        if (window) window.destroy()
    }

    function init() {
        document.newDocument()
        editHistory.clearAll()
    }

    function test_chooseAminoAcidPlacesItAsOneUndoStep() {
        const button = findChild(window, "templatesButton")
        verify(button)
        button.clicked()
        const popup = findChild(window, "templatesPopup")
        verify(popup)
        tryCompare(popup, "visible", true)
        verify(window.dialogOpen)
        popup.category = "amino"
        wait(20)
        verify(popup.cards.length === 20)
        popup.templateChosen("amino:ala")
        compare(molecule.formula(), "C3H7NO2")
        tryCompare(popup, "visible", false)
        verify(editHistory.undo())
        compare(molecule.atoms().length, 0)
    }

    function test_saveSelectionAndPlaceFromMine() {
        molecule.importSmiles("CCO", 0, 0)
        const ids = molecule.atoms().map(function(a) { return a.id })
        const before = templateLibrary.mineCount
        templateLibrary.saveSelection(ids)
        compare(templateLibrary.mineCount, before + 1)
        const popup = findChild(window, "templatesPopup")
        popup.open()
        popup.category = "mine"
        wait(20)
        verify(popup.cards.length === templateLibrary.mineCount)
        const card = popup.cards[popup.cards.length - 1]
        compare(card.formula, "C2H6O")
        verify(templateLibrary.deleteMine(card.id))
        compare(templateLibrary.mineCount, before)
        popup.close()
    }
}
