#pragma once

#include <QString>

// Helpers for recording output paths.
//
// The functions centralize the default directory and unique filename generation
// so recording code does not need to know about filesystem policy.
namespace FileNameUtils {

QString defaultOutputDirectory();
bool ensureDirectory(const QString& path, QString* errorMessage = nullptr);
QString uniqueRecordingFilePath(const QString& outputDirectory);

} // namespace FileNameUtils
