#include "gstreamer/GStreamerRuntime.h"

#include <QByteArray>

#include <mutex>

#include <gst/gst.h>

void GStreamerRuntime::ensureInitialized()
{
    static std::once_flag once;
    std::call_once(once,
                   []()
                   {
                       int argc = 0;
                       char** argv = nullptr;

                       gst_init(&argc, &argv);
                   });
}

bool GStreamerRuntime::requireElements(const QStringList& elementNames, QString* errorMessage)
{
    ensureInitialized();

    for (const QString& plugin : elementNames)
    {
        if (!hasElementFactory(plugin))
        {
            if (errorMessage)
            {
                *errorMessage = missingPluginMessage(plugin);
            }

            return false;
        }
    }

    return true;
}

QString GStreamerRuntime::escapedString(QString value)
{
    value.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
    value.replace(QLatin1Char('"'), QStringLiteral("\\\""));

    return value;
}

QString GStreamerRuntime::pluginInstallHint(const QString& pluginName)
{
    if (pluginName == QLatin1String("x264enc"))
    {
        return QStringLiteral("Install gstreamer1.0-plugins-ugly.");
    }

    if (pluginName == QLatin1String("ximagesrc") || pluginName == QLatin1String("matroskamux"))
    {
        return QStringLiteral("Install gstreamer1.0-plugins-good.");
    }

    if (pluginName == QLatin1String("h264parse"))
    {
        return QStringLiteral(
            "Install gstreamer1.0-plugins-bad or gstreamer1.0-plugins-good, depending on your Ubuntu release.");
    }

    return QStringLiteral("Install the missing GStreamer runtime plugin package.");
}

QString GStreamerRuntime::missingPluginMessage(const QString& pluginName)
{
    return QStringLiteral("Missing GStreamer plugin: %1. %2").arg(pluginName, pluginInstallHint(pluginName));
}

bool GStreamerRuntime::hasElementFactory(const QString& name)
{
    const QByteArray elementName = name.toUtf8();
    GstElementFactory* const factory = gst_element_factory_find(elementName.constData());

    if (!factory)
    {
        return false;
    }

    gst_object_unref(factory);

    return true;
}
