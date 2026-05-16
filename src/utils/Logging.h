#pragma once

#include <QLoggingCategory>

// Logging categories kept separate by subsystem. Hot per-frame paths should not
// log in normal operation.
Q_DECLARE_LOGGING_CATEGORY(logCapture)
Q_DECLARE_LOGGING_CATEGORY(logRecording)
Q_DECLARE_LOGGING_CATEGORY(logX11)
