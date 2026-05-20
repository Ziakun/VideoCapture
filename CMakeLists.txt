cmake_minimum_required(VERSION 3.21)

project(MeetVideoCapture
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2)
find_package(PkgConfig REQUIRED)

if (COMMAND qt_policy)
    qt_policy(SET QTP0001 NEW)
    qt_policy(SET QTP0004 NEW)
endif()

pkg_check_modules(GSTREAMER REQUIRED IMPORTED_TARGET gstreamer-1.0)
pkg_check_modules(GSTREAMER_APP REQUIRED IMPORTED_TARGET gstreamer-app-1.0)
pkg_check_modules(GSTREAMER_VIDEO REQUIRED IMPORTED_TARGET gstreamer-video-1.0)
pkg_check_modules(X11 REQUIRED IMPORTED_TARGET x11)

qt_add_executable(MeetVideoCapture
    src/main.cpp
    src/app/CaptureControllerCommands.cpp
    src/app/CapturePipelineWorker.cpp
    src/app/CapturePipelineWorker.h
    src/app/CaptureController.cpp
    src/app/CaptureController.h
    src/app/CaptureControllerCrop.cpp
    src/app/CaptureControllerState.cpp
    src/app/CaptureFormatting.cpp
    src/app/CaptureFormatting.h
    src/app/CropRectValidator.cpp
    src/app/CropRectValidator.h
    src/app/WindowListModelBuilder.cpp
    src/app/WindowListModelBuilder.h
    src/app/WindowRefreshWorker.cpp
    src/app/WindowRefreshWorker.h
    src/capture/CaptureSettings.h
    src/capture/CaptureFrameMonitor.cpp
    src/capture/CaptureFrameMonitor.h
    src/capture/FrameStats.h
    src/capture/GStreamerCapturePipelineBuilder.cpp
    src/capture/GStreamerCapturePipelineBuilder.h
    src/capture/GStreamerCapturePipeline.cpp
    src/capture/GStreamerCapturePipelineBus.cpp
    src/capture/GStreamerCapturePipelineLifecycle.cpp
    src/capture/GStreamerCapturePipeline.h
    src/capture/VideoContentDetector.cpp
    src/capture/VideoContentDetector.h
    src/capture/VideoFrame.h
    src/config/VideoCaptureConstants.h
    src/capture/x11/X11WindowClassifier.cpp
    src/capture/x11/X11WindowClassifier.h
    src/capture/x11/X11WindowFinder.cpp
    src/capture/x11/X11WindowFinder.h
    src/capture/x11/X11WindowInfo.h
    src/capture/x11/X11WindowPropertyReader.cpp
    src/capture/x11/X11WindowPropertyReader.h
    src/gstreamer/GStreamerRuntime.cpp
    src/gstreamer/GStreamerRuntime.h
    src/recording/GStreamerFrameWriter.cpp
    src/recording/GStreamerFrameWriter.h
    src/recording/GStreamerRecordingFinalizer.cpp
    src/recording/GStreamerRecordingFinalizer.h
    src/recording/GStreamerRecordingPipelineBuilder.cpp
    src/recording/GStreamerRecordingPipelineBuilder.h
    src/recording/RecordingSettings.h
    src/recording/VideoRecorder.cpp
    src/recording/VideoRecorderLifecycle.cpp
    src/recording/VideoRecorder.h
    src/ui/VideoFrameProvider.cpp
    src/ui/VideoFrameProvider.h
    src/ui/VideoPreviewItem.cpp
    src/ui/VideoPreviewItem.h
    src/utils/FileNameUtils.cpp
    src/utils/FileNameUtils.h
)

set_target_properties(MeetVideoCapture PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    QT_QML_MODULE_NO_IMPORT_SCAN TRUE
)

qt_add_resources(MeetVideoCapture qml_resources
    PREFIX "/"
    FILES
    qml/Main.qml
    qml/components/CropControls.qml
    qml/components/CropEdgeSlider.qml
    qml/components/CropMap.qml
    qml/components/ControlButton.qml
    qml/components/NumericSpinBox.qml
    qml/components/RecordButton.qml
    qml/components/RecordingTimerPill.qml
    qml/components/StatusChip.qml
    qml/components/StatusBar.qml
    qml/components/VideoPanel.qml
    qml/components/WindowSelector.qml
)

target_include_directories(MeetVideoCapture
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(MeetVideoCapture
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
        PkgConfig::GSTREAMER
        PkgConfig::GSTREAMER_APP
        PkgConfig::GSTREAMER_VIDEO
        PkgConfig::X11
)

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(MeetVideoCapture PRIVATE -Wall -Wextra -Wpedantic)
endif()

option(MEETVIDEOCAPTURE_BUILD_TESTS "Build local smoke test utilities" OFF)

if (MEETVIDEOCAPTURE_BUILD_TESTS)
    qt_add_executable(RecorderSmokeTest
        tests/RecorderSmokeTest.cpp
        src/gstreamer/GStreamerRuntime.cpp
        src/gstreamer/GStreamerRuntime.h
        src/recording/GStreamerFrameWriter.cpp
        src/recording/GStreamerFrameWriter.h
        src/recording/GStreamerRecordingFinalizer.cpp
        src/recording/GStreamerRecordingFinalizer.h
        src/recording/GStreamerRecordingPipelineBuilder.cpp
        src/recording/GStreamerRecordingPipelineBuilder.h
        src/config/VideoCaptureConstants.h
        src/recording/VideoRecorder.cpp
        src/recording/VideoRecorderLifecycle.cpp
        src/recording/VideoRecorder.h
        src/recording/RecordingSettings.h
        src/capture/VideoFrame.h
    )

    set_target_properties(RecorderSmokeTest PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )

    target_include_directories(RecorderSmokeTest
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    target_link_libraries(RecorderSmokeTest
        PRIVATE
            Qt6::Core
            Qt6::Gui
            PkgConfig::GSTREAMER
            PkgConfig::GSTREAMER_APP
    )

    if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(RecorderSmokeTest PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endif()
