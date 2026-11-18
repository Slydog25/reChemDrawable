import QtQuick
import QtTest

TestCase {
    name: "MainWindow"
    when: windowShown

    property var window: null

    function initTestCase() {
        // Any QML warning (a broken binding, an undefined name) fails the test.
        failOnWarning(/.*/)
        const component = Qt.createComponent("../../src/qml/Main.qml")
        compare(component.status, Component.Ready, component.errorString())
        window = component.createObject(null)
        verify(window)
        // Let the first frame lay out and paint.
        wait(50)
    }

    function cleanupTestCase() {
        if (window) window.destroy()
    }

    function init() {
        document.newDocument()
    }

    function test_cleanDocumentRunsActionImmediately() {
        let ran = false
        window.confirmDiscard(function() { ran = true })
        verify(ran)
    }

    function test_dirtyDocumentAsksBeforeDiscarding() {
        molecule.addAtom(10, 10)
        verify(document.dirty)
        const dialog = findChild(window, "discardDialog")
        verify(dialog)
        let ran = false
        window.confirmDiscard(function() { ran = true })
        verify(!ran)
        tryVerify(function() { return dialog.visible })
        window.runPendingAction()
        verify(ran)
        tryVerify(function() { return !dialog.visible })
    }

    function test_cancelKeepsTheDrawing() {
        molecule.addAtom(10, 10)
        const dialog = findChild(window, "discardDialog")
        let ran = false
        window.confirmDiscard(function() { ran = true })
        tryVerify(function() { return dialog.visible })
        dialog.close()
        tryVerify(function() { return !dialog.visible })
        compare(window.pendingAction, null)
        verify(!ran)
        compare(molecule.atoms().length, 1)
    }

    function test_openDialogListsSavedDrawings() {
        molecule.addAtom(10, 10)
        verify(document.save())
        const dialog = findChild(window, "openDialog")
        verify(dialog)
        dialog.open()
        tryVerify(function() { return dialog.visible })
        verify(dialog.entries.length >= 1)
        dialog.close()
    }

    function test_titleShowsUnsavedMarker() {
        const title = findChild(window, "documentTitle")
        verify(title)
        compare(title.text, "Untitled")
        molecule.addAtom(10, 10)
        compare(title.text, "Untitled •")
    }
}
