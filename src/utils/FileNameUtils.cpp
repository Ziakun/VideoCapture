#include "FileNameUtils.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

namespace FileNameUtils {

QString defaultOutputDirectory()
{
    QString base = QStandardPaths::writableLocation(QStandardPaths::MoviesLocation);
    if (base.isEmpty()) {
        base = QDir::homePath() + QStringLiteral("/Videos");
    }
    return QDir(base).filePath(QStringLiteral("MeetVideoCapture"));
}

bool ensureDirectory(const QString& path, QString* errorMessage)
{
    QDir dir(path);
    if (dir.exists()) {
        return true;
    }

    if (dir.mkpath(QStringLiteral("."))) {
        return true;
    }

    if (errorMessage) {
        *errorMessage = QStringLiteral("Could not create output directory: %1").arg(path);
    }
    return false;
}

QString uniqueRecordingFilePath(const QString& outputDirectory)
{
    const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss"));
    const QString baseName = QStringLiteral("meet-recording-%1").arg(stamp);
    QDir dir(outputDirectory);

    QString path = dir.filePath(baseName + QStringLiteral(".mkv"));
    if (!QFileInfo::exists(path)) {
        return path;
    }

    for (int i = 1; i < 1000; ++i) {
        path = dir.filePath(QStringLiteral("%1-%2.mkv").arg(baseName).arg(i, 2, 10, QLatin1Char('0')));
        if (!QFileInfo::exists(path)) {
            return path;
        }
    }

    return dir.filePath(baseName + QStringLiteral("-latest.mkv"));
}

} // namespace FileNameUtils
