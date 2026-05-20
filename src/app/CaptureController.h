#pragma once

#include "app/CropRectValidator.h"
#include "capture/CaptureSettings.h"
#include "capture/VideoContentDetector.h"
#include "app/CapturePipelineWorker.h"
#include "app/WindowListModelBuilder.h"
#include "app/WindowRefreshWorker.h"
#include "recording/VideoRecorder.h"
#include "ui/VideoFrameProvider.h"

#include <QObject>
#include <QThread>
#include <QTimer>
#include <QVariantList>

// QML-facing orchestration layer.
//
// This class keeps source-window selection, crop settings, capture state,
// recording state, status messages, and timers in one Qt-friendly API. It
// deliberately does not implement X11, GStreamer, or recording internals; those
// stay in their dedicated backend classes.
class CaptureController : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QObject* frameProvider MEMBER previewFrameProviderObject CONSTANT)
    Q_PROPERTY(QVariantList windows MEMBER windowModel NOTIFY windowsChanged)
    Q_PROPERTY(bool isCapturing MEMBER capturingActive NOTIFY isCapturingChanged)
    Q_PROPERTY(bool isRecording MEMBER recordingActive NOTIFY isRecordingChanged)
    Q_PROPERTY(bool hasPreviewFrame MEMBER previewFrameAvailable NOTIFY hasPreviewFrameChanged)
    Q_PROPERTY(QString selectedWindowTitle MEMBER currentSelectedWindowTitle NOTIFY selectedWindowChanged)
    Q_PROPERTY(qulonglong selectedWindowId MEMBER currentSelectedWindowId NOTIFY selectedWindowChanged)
    Q_PROPERTY(QString selectedSourceType MEMBER currentSelectedSourceType NOTIFY selectedWindowChanged)
    Q_PROPERTY(QString outputFilePath MEMBER currentOutputFilePath NOTIFY outputFilePathChanged)
    Q_PROPERTY(int recordingSeconds MEMBER elapsedRecordingSeconds NOTIFY recordingSecondsChanged)
    Q_PROPERTY(QString recordingTimeText MEMBER currentRecordingTimeText NOTIFY recordingSecondsChanged)
    Q_PROPERTY(double fps MEMBER currentFps NOTIFY fpsChanged)
    Q_PROPERTY(QString statusMessage MEMBER currentStatusMessage NOTIFY statusMessageChanged)
    Q_PROPERTY(QString warningMessage MEMBER currentWarningMessage NOTIFY warningMessageChanged)
    Q_PROPERTY(QString previewMessage MEMBER currentPreviewMessage NOTIFY previewMessageChanged)
    Q_PROPERTY(int cropX MEMBER currentCropX NOTIFY cropChanged)
    Q_PROPERTY(int cropY MEMBER currentCropY NOTIFY cropChanged)
    Q_PROPERTY(int cropWidth MEMBER currentCropWidth NOTIFY cropChanged)
    Q_PROPERTY(int cropHeight MEMBER currentCropHeight NOTIFY cropChanged)
    Q_PROPERTY(QString captureModeText MEMBER currentCaptureModeText NOTIFY captureModeChanged)
    Q_PROPERTY(QString sourceGeometryText MEMBER currentSourceGeometryText NOTIFY sourceGeometryChanged)
    Q_PROPERTY(int sourceWidth MEMBER currentSourceWidth NOTIFY sourceGeometryChanged)
    Q_PROPERTY(int sourceHeight MEMBER currentSourceHeight NOTIFY sourceGeometryChanged)

  public:
    // Creates workers, connects capture/recording signals, and starts initial window refresh.
    explicit CaptureController(QObject* parent = nullptr);
    // Stops recording/capture workers and joins their threads during shutdown.
    ~CaptureController() override;

    Q_INVOKABLE void refreshWindows();             // Asks the X11 worker to refresh the selectable window list.
    Q_INVOKABLE void selectWindow(qulonglong xid); // Selects a source by XID and resets capture/crop state.
    Q_INVOKABLE void startCapture();               // Validates crop/settings and starts capture on the worker thread.
    Q_INVOKABLE void stopCapture();                // Stops capture and clears preview/recording state.
    Q_INVOKABLE void toggleRecording();            // Starts or stops recording based on current recorder state.
    Q_INVOKABLE void startRecording();             // Creates a file path, starts recorder, and connects frame feed.
    Q_INVOKABLE void stopRecording();              // Disconnects frame feed and finalizes the recording asynchronously.
    Q_INVOKABLE void setCropRect(int x, int y, int width, int height); // Validates and applies a crop rectangle.
    Q_INVOKABLE void resetCropToAutoState();                     // Restores detected auto-crop or full source crop.
    Q_INVOKABLE void chooseOutputDirectory(const QString& path); // Stores a local output directory path from QML.
    Q_INVOKABLE void dismissCurrentMessage();                    // Clears visible warning/status/preview messages.
    Q_INVOKABLE void switchToScreenRegionFallback();             // Switches capture to visible screen-region fallback.

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
    void setIsCapturing(bool value);                // Updates capture state and emits only on change.
    void setIsRecording(bool value);                // Updates recording state and emits only on change.
    void setHasPreviewFrame(bool value);            // Updates preview availability and emits only on change.
    void setStatusMessage(const QString& message);  // Stores status text and emits only on change.
    void setWarningMessage(const QString& message); // Stores warning text and emits only on change.
    void setPreviewMessage(const QString& message); // Stores preview overlay text and emits only on change.
    void updatePreviewMessage(); // Derives preview overlay text from source/capture/frame/warning state.
    bool autoCropToVideoArea();  // Detects a browser video area from the latest preview frame and applies crop.
    void tryPendingAutoCrop();   // Runs bounded auto-crop attempts after capture frames arrive.
    void handleWindowsReady(int requestId, const QVector<X11WindowInfo>& windows,
                            const QString& error); // Applies worker result if it is the latest request.
    void handleCaptureCommandFailed(quint64 commandId,
                                    const QString& error); // Rolls back pending state after worker failure.
    void handleCaptureCommandSucceeded(quint64 commandId); // Clears completed pending capture command state.
    void updateSourceFromSelectedWindow(const X11WindowInfo& info); // Copies selected window geometry into settings.
    QRect validatedCropRect(int x, int y, int width, int height,
                            QString* warningMessage) const; // Clamps crop through CropRectValidator.
    void applyCropRect(const QRect& rect);                  // Stores crop and emits cropChanged when value differs.
    void syncCropProperties();                              // Copies cropRect into QML MEMBER fields.
    void syncSourceGeometryProperties();                    // Copies source geometry into QML MEMBER fields.
    QRect currentFallbackScreenRect() const; // Converts window-relative crop to absolute screen coordinates.
    void restartCaptureForCurrentSettings(); // Restarts a running capture pipeline with current settings.
    quint64 nextCaptureCommandId();          // Returns a monotonically increasing worker command id.
    void connectRecorderFrameFeed();    // Connects capture frames directly into the recorder queue while recording.
    void disconnectRecorderFrameFeed(); // Removes the recorder frame feed when recording stops.

    QVector<X11WindowInfo> windowInfos;
    QVariantList windowModel;
    WindowListModelBuilder windowListModelBuilder;

    VideoFrameProvider previewFrameProvider;
    QObject* previewFrameProviderObject = &previewFrameProvider;
    QThread windowRefreshThread;
    WindowRefreshWorker* windowRefreshWorker = nullptr;
    QThread captureCommandThread;
    CapturePipelineWorker* captureWorker = nullptr;
    VideoRecorder recorder;
    QMetaObject::Connection recorderFrameConnection;

    CaptureSettings captureSettings;
    CropRectValidator cropRectValidator;
    VideoContentDetector videoContentDetector;
    QRect automaticCropRect;
    QTimer recordingTimer;

    bool capturingActive = false;
    bool recordingActive = false;
    bool previewFrameAvailable = false;
    bool captureStartPending = false;
    bool captureStopPending = false;
    bool autoCropPending = false;
    int autoCropAttemptsRemaining = 0;
    int latestWindowRefreshRequestId = 0;
    quint64 nextCaptureCommandSequence = 0;

    QString currentSelectedWindowTitle;
    QString currentSelectedSourceType = QStringLiteral("Window");
    qulonglong currentSelectedWindowId = 0;
    QString outputDirectory;
    QString currentOutputFilePath;
    int elapsedRecordingSeconds = 0;
    QString currentRecordingTimeText = QStringLiteral("00:00:00");
    double currentFps = 0.0;
    QString currentStatusMessage = QStringLiteral("No video source selected");
    QString currentWarningMessage;
    QString currentPreviewMessage = QStringLiteral("No video source selected");
    int currentCropX = 0;
    int currentCropY = 0;
    int currentCropWidth = 0;
    int currentCropHeight = 0;
    QString currentCaptureModeText = QStringLiteral("X11WindowById");
    QString currentSourceGeometryText = QStringLiteral("-");
    int currentSourceWidth = 0;
    int currentSourceHeight = 0;
};
