#pragma once

#include <QtGlobal>

#if defined(Q_OS_LINUX) && defined(Q_PROCESSOR_ARM_64)
#include <asm/hwcap.h>
#include <sys/auxv.h>
#endif

// Qt 6.4's QML JIT crashes during garbage collection (SIGSEGV in
// QV4::MemoryManager::collectFromJSStack) on aarch64 CPUs with pointer
// authentication, e.g. arm64 Linux containers on Apple Silicon. Use the QML
// interpreter on those CPUs; this app's JavaScript is light enough that the
// difference is not noticeable. Call before creating any QML engine. An explicit
// QV4_FORCE_INTERPRETER setting in the environment is left alone.
inline void avoidQmlJitOnPointerAuthCpus() {
#if defined(Q_OS_LINUX) && defined(Q_PROCESSOR_ARM_64) && defined(HWCAP_PACA)
    if (qEnvironmentVariableIsSet("QV4_FORCE_INTERPRETER")) return;
    if (getauxval(AT_HWCAP) & HWCAP_PACA) qputenv("QV4_FORCE_INTERPRETER", "1");
#endif
}
