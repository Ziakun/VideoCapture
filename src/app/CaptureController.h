#pragma once

#include "capture/CaptureSettings.h"
#include "app/CapturePipelineWorker.h"
#include "app/WindowRefreshWorker.h"
#include "recording/VideoRecorder.h"
#include "ui/VideoFrameProvider.h"

#include <QObject>
#include <QThread>
#include <QTimer>
#include <QVariantList>

// QML-facing orchestration layer.
//
// This class keeps browser-window selection, crop settings, capture state,
// recording state, status messages, and timers in one Qt-friendly API. It
// deliberately does not implement X11, GStreamer, or recording internals; those
// stay in their dedicated backend classes.
class CaptureController : public QObject {
    Q_OBJECT

    Q_PROPERTY(QObject* frameProvider READ frameProvider CONSTANT)
    Q_PROPERTY(QVariantList windows READ windows NOTIFY windowsChanged)
    Q_PROPERTY(bool isCapturing READ isCapturing NOTIFY isCapturingChanged)
    Q_PROPERTY(bool isRecording READ isRecording NOTIFY isRecordingChanged)
    Q_PROPERTY(bool hasPreviewFrame READ hasPreviewFrame NOTIFY hasPreviewFrameChanged)
    Q_PROPERTY(QString selectedWindowTitle READ selectedWindowTitle NOTIFY selectedWindowChanged)
    Q_PROPERTY(qulonglong selectedWindowId READ selectedWindowId NOTIFY selectedWindowChanged)
    Q_PROPERTY(QString outputFilePath READ outputFilePath NOTIFY outputFilePathChanged)
    Q_PROPERTY(int recordingSeconds READ recordingSeconds NOTIFY recordingSecondsChanged)
    Q_PROPERTY(QString recordingTimeText READ recordingTimeText NOTIFY recordingSecondsChanged)
    Q_PROPERTY(double fps READ fps NOTIFY fpsChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)
    Q_PROPERTY(QString warningMessage READ warningMessage NOTIFY warningMessageChanged)
    Q_PROPERTY(QString previewMessage READ previewMessage NOTIFY previewMessageChanged)
    Q_PROPERTY(int cropX READ cropX NOTIFY cropChanged)
    Q_PROPERTY(int cropY READ cropY NOTIFY cropChanged)
    Q_PROPERTY(int cropWidth READ cropWidth NOTIFY cropChanged)
    Q_PROPERTY(int cropHeight READ cropHeight NOTIFY cropChanged)
    Q_PROPERTY(QString captureModeText READ captureModeText NOTIFY captureModeChanged)
    Q_PROPERTY(QString sourceGeometryText READ sourceGeometryText NOTIFY sourceGeometryChanged)
    Q_PROPERTY(int sourceWidth READ sourceWidth NOTIFY sourceGeometryChanged)
    Q_PROPERTY(int sourceHeight READ sourceHeight NOTIFY sourceGeometryChanged)

public:
    explicit CaptureController(QObject* parent = nullptr);
    ~CaptureController() override;

    QObject* frameProvider();
    QVariantList windows() const;
    bool isCapturing() const;
    bool isRecording() const;
    bool hasPreviewFrame() const;
    QString selectedWindowTitle() const;
    qulonglong selectedWindowId() const;
    QString outputFilePath() const;
    int recordingSeconds() const;
    QString recordingTimeText() const;
    double fps() const;
    QString statusMessage() const;
    QString warningMessage() const;
    QString previewMessage() const;
    int cropX() const;
    int cropY() const;
    int cropWidth() const;
    int cropHeight() const;
    QString captureModeText() const;
    QString sourceGeometryText() const;
    int sourceWidth() const;
    int sourceHeight() const;

    Q_INVOKABLE void refreshWindows();
    Q_INVOKABLE void selectWindow(qulonglong xid);
    Q_INVOKABLE void startCapture();
    Q_INVOKABLE void stopCapture();
    Q_INVOKABLE void toggleRecording();
    Q_INVOKABLE void startRecording();
    Q_INVOKABLE void stopRecording();
    Q_INVOKABLE void setCropRect(int x, int y, int width, int height);
    Q_INVOKABLE void chooseOutputDirectory(const QString& path);
    Q_INVOKABLE void switchToScreenRegionFallback();

signals:
    void windowsChanged();
    void isCapturingChanged();
    void isRecordingChanged();
    void hasPreviewFrameChanged();
    void selectedWindowChanged();
    void outputFilePathChanged();
    void recordingSecondsChanged();
    void fpsChanged();
    void statusMessageChanged();
    void warningMessageChanged();
    void previewMessageChanged();
    void cropChanged();
    void captureModeChanged();
    void sourceGeometryChanged();

private:
    const X11WindowInfo* findWindow(quint64 xid) const;
    void rebuildWindowVariantList();
    void setIsCapturing(bool value);
    void setIsRecording(bool value);
    void setHasPreviewFrame(bool value);
    void setStatusMessage(const QString& message);
    void setWarningMessage(const QString& message);
    void setPreviewMessage(const QString& message);
    void updatePreviewMessage();
    void handleWindowsReady(int requestId, const QVector<X11WindowInfo>& windows, const QString& error);
    void handleCaptureCommandFailed(quint64 commandId, const QString& error);
    void handleCaptureCommandSucceeded(quint64 commandId);
    void updateSourceFromSelectedWindow(const X11WindowInfo& info);
    QRect validatedCropRect(int x, int y, int width, int height, QString* warningMessage) const;
    void applyCropRect(const QRect& rect);
    QRect currentFallbackScreenRect() const;
    void restartCaptureForCurrentSettings();
    quint64 nextCaptureCommandId();
    void connectRecorderFrameFeed();
    void disconnectRecorderFrameFeed();
    static QString formatRecordingTime(int seconds);

    QVector<X11WindowInfo> windowInfos;
    QVariantList windowModel;

    VideoFrameProvider previewFrameProvider;
    QThread windowRefreshThread;
    WindowRefreshWorker* windowRefreshWorker = nullptr;
    QThread captureCommandThread;
    CapturePipelineWorker* captureWorker = nullptr;
    VideoRecorder recorder;
    QMetaObject::Connection recorderFrameConnection;

    CaptureSettings captureSettings;
    QTimer recordingTimer;

    bool capturingActive = false;
    bool recordingActive = false;
    bool previewFrameAvailable = false;
    bool captureStartPending = false;
    bool captureStopPending = false;
    int latestWindowRefreshRequestId = 0;
    quint64 nextCaptureCommandSequence = 0;

    QString currentSelectedWindowTitle;
    quint64 currentSelectedWindowId = 0;
    QString outputDirectory;
    QString currentOutputFilePath;
    int elapsedRecordingSeconds = 0;
    double currentFps = 0.0;
    QString currentStatusMessage = QStringLiteral("No video source selected");
    QString currentWarningMessage;
    QString currentPreviewMessage = QStringLiteral("No video source selected");
};
