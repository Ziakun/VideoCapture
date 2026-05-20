#pragma once

#include <QString>

// Helpers for recording output paths.
//
// The functions centralize the default directory and unique filename generation
// so recording code does not need to know about filesystem policy.
namespace FileNameUtils
{

QString defaultOutputDirectory(); // Returns the default directory for generated recordings.
bool ensureDirectory(const QString& path, QString* errorMessage = nullptr); // Creates the directory when missing.
QString uniqueRecordingFilePath(
    const QString& outputDirectory,
    const QString& filePrefix = QStringLiteral("meeting-recording")); // Returns a non-existing MKV path.

} // namespace FileNameUtils
