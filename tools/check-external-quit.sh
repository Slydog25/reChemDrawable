#!/bin/sh
# Regression check for closing the app from outside (AppLoad sends SIGTERM):
# with unsaved changes it must still quit, and save a recovered-*.rcd copy.
# Run in the desktop image:
#   docker run --rm -v "$PWD:/workspace:ro" rechemdraw-qt /workspace/tools/check-external-quit.sh
set -u
cmake -S /workspace -B /tmp/b -DBUILD_TESTING=OFF -DRECHEMDRAW_EXTERNAL_QML=ON >/dev/null && cmake --build /tmp/b --parallel >/dev/null 2>&1 || { echo build failed; exit 1; }
rm -rf /tmp/b/qml && cp -r /workspace/src/qml /tmp/b/qml
sed -i 's|^    Component.onCompleted: moleculeProperties = molecule.properties()|    Component.onCompleted: { moleculeProperties = molecule.properties(); molecule.addAtom(10, 10) }|' /tmp/b/qml/Main.qml
grep -q "molecule.addAtom(10, 10)" /tmp/b/qml/Main.qml || { echo "harness patch failed"; exit 1; }
rm -rf "$HOME/Documents"
QT_QPA_PLATFORM=offscreen /tmp/b/reChemDrawable >/tmp/app.log 2>&1 &
pid=$!
sleep 2
kill -TERM $pid
for i in 1 2 3 4 5 6; do kill -0 $pid 2>/dev/null || break; sleep 0.5; done
if kill -0 $pid 2>/dev/null; then echo "FAIL: still running 3s after SIGTERM"; kill -KILL $pid; exit 1; fi
wait $pid; status=$?
[ "$status" -eq 0 ] || { echo "FAIL: exit status $status"; exit 1; }
ls "$HOME"/Documents/reChemDrawable/recovered-*.rcd >/dev/null 2>&1 || { echo "FAIL: no recovery copy"; exit 1; }
echo "OK: quit on SIGTERM with unsaved changes and saved a recovery copy"
