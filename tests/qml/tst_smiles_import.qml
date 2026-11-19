import QtQuick
import QtTest

TestCase {
    name: "SmilesImportButton"
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
    }

    function test_buttonImportsFieldTextAsOneUndoStep() {
        const button = findChild(window, "importSmilesButton")
        verify(button)
        const field = findChild(window, "reactionLabelField")
        verify(field)
        field.text = ""
        verify(!button.enabled)
        field.text = "CCO"
        verify(button.enabled)
        button.clicked()
        compare(molecule.formula(), "C2H6O")
        verify(molecule.undo())
        compare(molecule.atoms().length, 0)
    }
}
