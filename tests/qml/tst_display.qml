import QtQuick
import QtTest

// The "Clean screen" button must exist and be harmless where there is no e-ink
// display (desktop, tests): `eink` is only provided by the tablet build.
TestCase {
    name: "Display"
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

    function test_cleanScreenButtonDoesNothingWithoutEink() {
        const button = findChild(window, "cleanScreenButton")
        verify(button, "toolbar has a Clean screen button")
        compare(button.text, "Clean screen")
        button.clicked() // would fail the test via failOnWarning if `eink` were referenced unguarded
        wait(10)
    }
}
