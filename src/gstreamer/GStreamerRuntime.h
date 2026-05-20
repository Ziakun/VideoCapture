#pragma once

#include <QString>
#include <QStringList>

// Centralizes process-wide GStreamer initialization and small runtime helpers.
//
// Capture and recording can be constructed independently. This class keeps
// gst_init() single-shot, plugin validation consistent, and string escaping in
// one place so pipeline classes do not duplicate runtime policy.
class GStreamerRuntime
{
  public:
    // Initializes GStreamer once for the whole process; safe to call repeatedly.
    static void ensureInitialized();

    // Checks that all named element factories exist and writes a user-facing
    // install hint into errorMessage when the first missing plugin is found.
    static bool requireElements(const QStringList& elementNames, QString* errorMessage = nullptr);

    // Escapes a value for use inside a quoted gst_parse_launch() property.
    static QString escapedString(QString value);

  private:
    static QString pluginInstallHint(const QString& pluginName);
    static QString missingPluginMessage(const QString& pluginName);
    static bool hasElementFactory(const QString& name);
};
