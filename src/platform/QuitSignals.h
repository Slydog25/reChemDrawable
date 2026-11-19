#pragma once

// Turns SIGTERM, SIGINT and SIGHUP into an ordinary application quit, so
// shutdown code (such as saving a recovery copy) runs instead of the process
// dying mid-edit. AppLoad stops apps with a signal.
void installQuitSignalHandlers();
bool quitSignalReceived();
