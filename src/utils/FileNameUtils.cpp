#include "FileNameUtils.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>

namespace FileNameUtils
{

namespace
{

constexpr int maximumUniqueSuffixAttempts = 1000;
constexpr int uniqueSuffixFieldWidth = 2;
constexpr int decimalBase = 10;

QString normalizedFilePrefix(const QString& filePrefix)
{
    QString normalized;

    normalized.reserve(filePrefix.size());

    for (const QChar character : filePrefix.toLower())
    {
        if (character.isLetterOrNumber() || character == QLatin1Char('-') || character == QLatin1Char('_'))
        {
            normalized.append(character);
        }
        else if (!normalized.endsWith(QLatin1Char('-')))
        {
            normalized.append(QLatin1Char('-'));
        }
    }

    while (normalized.endsWith(QLatin1Char('-')) || normalized.endsWith(QLatin1Char('_')))
    {
        normalized.chop(1);
    }

    return normalized.isEmpty() ? QStringLiteral("meeting-recording") : normalized;
}

} // namespace

QString defaultOutputDirectory()
{
    return QDir::home().filePath(QStringLiteral("Video Capture"));
}

bool ensureDirectory(const QString& path, QString* errorMessage)
{
    QDir dir(path);

    if (dir.exists())
    {
        return true;
    }

    if (dir.mkpath(QStringLiteral(".")))
    {
        return true;
    }

    if (errorMessage)
    {
        *errorMessage = QStringLiteral("Could not create output directory: %1").arg(path);
    }

    return false;
}

QString uniqueRecordingFilePath(const QString& outputDirectory, const QString& filePrefix)
{
    const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss"));
    const QString baseName = QStringLiteral("%1-%2").arg(normalizedFilePrefix(filePrefix), stamp);

    QDir dir(outputDirectory);

    QString path = dir.filePath(baseName + QStringLiteral(".mkv"));

    if (!QFileInfo::exists(path))
    {
        return path;
    }

    for (int i = 1; i < maximumUniqueSuffixAttempts; ++i)
    {
        path = dir.filePath(
            QStringLiteral("%1-%2.mkv").arg(baseName).arg(i, uniqueSuffixFieldWidth, decimalBase, QLatin1Char('0')));

        if (!QFileInfo::exists(path))
        {
            return path;
        }
    }

    return dir.filePath(baseName + QStringLiteral("-latest.mkv"));
}

} // namespace FileNameUtils
