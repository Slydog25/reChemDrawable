import QtQuick
import QtTest

TestCase {
    name: "PropertiesPanel"
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

    function cleanupTestCase() { if (window) window.destroy() }

    function init() { document.newDocument() }

    function test_opensAndUpdates() {
        const button = findChild(window, "propertiesButton")
        verify(button)
        const panel = findChild(window, "propertiesPanel")
        verify(panel)
        verify(!window.dialogOpen)
        button.clicked()
        wait(50)
        verify(panel.visible)
        verify(window.dialogOpen)
        // Draw while open: the panel follows.
        verify(molecule.importSmiles("CCO", 200, 200))
        tryCompare(findChild(window, "propFormula"), "text", "C2H6O")
        compare(findChild(window, "propHbond").text, "1 / 1")
        compare(findChild(window, "propTpsa").text, "20.2 Å²")
        compare(findChild(window, "propertiesScope").text, "whole drawing")
        findChild(window, "propertiesClose").clicked()
        wait(50)
        verify(!panel.visible)
        verify(!window.dialogOpen)
    }

    function test_selectionScope() {
        verify(molecule.importSmiles("CCO", 200, 200))
        verify(molecule.importSmiles("c1ccccc1", 600, 200))
        const ring = molecule.atoms()[molecule.atoms().length - 1].id
        findChild(window, "moleculeCanvas").selectedAtoms = [ring]
        findChild(window, "propertiesButton").clicked()
        wait(50)
        tryCompare(findChild(window, "propFormula"), "text", "C6H6")
        verify(findChild(window, "propertiesScope").text.indexOf("selection") === 0)
        findChild(window, "propertiesClose").clicked()
    }
}
