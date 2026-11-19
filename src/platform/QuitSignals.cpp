#include "platform/QuitSignals.h"

#include <csignal>

namespace {
volatile std::sig_atomic_t quitRequested = 0;

extern "C" void requestQuit(int) { quitRequested = 1; }
}

void installQuitSignalHandlers() {
    // Only an async-signal-safe flag is set here; the event loop polls it.
    std::signal(SIGTERM, requestQuit);
    std::signal(SIGINT, requestQuit);
#ifdef SIGHUP
    std::signal(SIGHUP, requestQuit);
#endif
}

bool quitSignalReceived() { return quitRequested != 0; }
