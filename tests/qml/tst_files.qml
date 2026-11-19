import QtQuick
import QtTest

TestCase {
    name: "OpenList"
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
        const old = document.documents()
        for (let i = 0; i < old.length; ++i) document.deleteDocument(old[i].path)
    }

    function test_listShowsEntriesWithThumbnailAndDeleteConfirmation() {
        molecule.addAtom(0, 0, "C")
        molecule.addAtom(40, 0, "O")
        verify(document.save())
        document.newDocument()
        const dialog = findChild(window, "openDialog")
        dialog.open()
        tryVerify(function() { return dialog.visible })
        compare(dialog.entries.length, 1)
        const entry = findChild(dialog.contentItem, "openEntry")
        verify(entry)
        const thumb = findChild(entry, "openThumbnail")
        verify(thumb && thumb.visible)

        // The first click only asks; No cancels and keeps the file.
        mouseClick(findChild(entry, "deleteButton"))
        verify(entry.confirming)
        wait(100)
        mouseClick(findChild(entry, "deleteNo"))
        verify(!entry.confirming)
        compare(document.documents().length, 1)

        mouseClick(findChild(entry, "deleteButton"))
        wait(100)
        mouseClick(findChild(entry, "deleteYes"))
        compare(document.documents().length, 0)
        compare(dialog.entries.length, 0)
        dialog.close()
        tryVerify(function() { return !dialog.visible })
    }
}
