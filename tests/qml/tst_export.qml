import QtQuick
import QtTest

TestCase {
    name: "ExportPopup"
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

    function test_exportButtonOpensPopupWithAllFormats() {
        const dialog = findChild(window, "exportDialog")
        verify(dialog)
        verify(!window.dialogOpen)
        findChild(window, "exportButton").clicked() // the rail scrolls, so the button may be off screen
        tryVerify(function() { return dialog.visible })
        verify(window.dialogOpen) // fingers may use it
        const keys = ["mol", "svg", "smiles", "png", "pdf", "cdxml", "library"]
        for (let i = 0; i < keys.length; ++i)
            verify(findChild(dialog.contentItem, "export_" + keys[i]), keys[i])
        mouseClick(findChild(dialog.contentItem, "export_cancel"))
        tryVerify(function() { return !dialog.visible })
        verify(!window.dialogOpen)
    }

    function test_pngButtonExportsAndCloses() {
        molecule.addAtom(0, 0, "C")
        molecule.addAtom(48, 0, "O")
        molecule.addBond(1, 2, 1)
        const dialog = findChild(window, "exportDialog")
        dialog.open()
        tryVerify(function() { return dialog.visible })
        mouseClick(findChild(dialog.contentItem, "export_png"))
        tryVerify(function() { return !dialog.visible })
        const status = window.contentItem.children
        // The status line reports the written file.
        let found = false
        function scan(item) {
            if (item.text !== undefined && String(item.text).indexOf("untitled.png") >= 0) found = true
            for (let i = 0; i < item.children.length; ++i) scan(item.children[i])
        }
        scan(window.contentItem)
        verify(found)
        molecule.clear()
    }
}
