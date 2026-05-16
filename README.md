# MeetVideoCapture

MeetVideoCapture is a Qt 6 / C++ / QML application for Ubuntu X11 that captures video already rendered by a meeting source window, crops the visible video area, previews it with low latency, and optionally records it to an MKV/H.264 file.

The app does not extract Google Meet or Zoom media internals. Google Meet in a browser and Zoom Desktop both already decode video and render it into a top-level X11 window. MeetVideoCapture captures those rendered pixels by XID, then crops the selected video area.

Primary path:

```text
Google Meet in browser
  or Zoom Desktop app
  -> app/browser decodes and renders video
  -> X11 window capture by XID
  -> crop meeting video area
  -> Qt preview
  -> optional MKV/H.264 recording
```

Fallback path:

```text
visible screen region capture
  -> Qt preview
  -> optional MKV/H.264 recording
```

The fallback captures visible screen pixels. If another window covers the selected video region, that covering window can be captured too.

## Why Not WebRTC Extraction

This is not a browser or Zoom hacking project. It does not use Chrome/Firefox extensions, Selenium, Chrome DevTools Protocol, Zoom SDK media extraction, process injection, LD_PRELOAD, graphics API hooks, WebRTC packet sniffing, or SRTP/DTLS decryption.

The intended production surface is the rendered source window. This keeps the capture backend independent from Google Meet internals, browser WebRTC implementation details, and Zoom Desktop internals.

## Recording Format

Default recording format:

- Container: Matroska
- Extension: `.mkv`
- Video codec: H.264
- Encoder: `x264enc`
- Default names: `zoom-recording-YYYYMMDD-HHMMSS.mkv`, `meet-recording-YYYYMMDD-HHMMSS.mkv`, `browser-recording-YYYYMMDD-HHMMSS.mkv`, or `window-recording-YYYYMMDD-HHMMSS.mkv` depending on the selected source profile.

MKV is the default because it is safer for live recording. MP4 needs a clean EOS/finalize step, and a crash can leave the file unusable. MKV has a better chance of remaining playable after an interrupted recording, while H.264 remains widely supported.

## Threading Model

- QML and controller state stay in the UI thread.
- X11 window enumeration runs in a dedicated `WindowRefreshWorker` thread and classifies likely `Zoom`, `Browser`, and generic `Window` sources.
- Capture lifecycle commands run in a dedicated `CapturePipelineWorker` thread.
- GStreamer capture uses its own streaming threads for frame delivery.
- Preview uses latest-frame delivery without an unbounded queue.
- Recording uses a separate worker thread and a bounded frame queue.

## Ubuntu Dependencies

```bash
sudo apt install \
    qt6-base-dev \
    qt6-declarative-dev \
    libgstreamer1.0-dev \
    libgstreamer-plugins-base1.0-dev \
    gstreamer1.0-plugins-base \
    gstreamer1.0-plugins-good \
    gstreamer1.0-plugins-bad \
    gstreamer1.0-plugins-ugly \
    libx11-dev \
    pkg-config \
    cmake \
    ninja-build
```

Runtime GStreamer elements used by the MVP include `ximagesrc`, `appsink`, `appsrc`, `videoconvert`, `videocrop`, `x264enc`, `h264parse`, `matroskamux`, and `filesink`.

## Build

```bash
cmake -S . -B build -G Ninja
cmake --build build
```

## Recorder Smoke Test

The recorder can be checked without Google Meet, Zoom, or X11 by generating synthetic
frames and writing them through the same C++ `VideoRecorder` path:

```bash
cmake -S . -B build-recorder-test -G Ninja -DMEETVIDEOCAPTURE_BUILD_TESTS=ON
cmake --build build-recorder-test --target RecorderSmokeTest
./build-recorder-test/bin/RecorderSmokeTest \
    ./build-recorder-test/recorder-smoke/recorder-smoke.mkv
gst-discoverer-1.0 ./build-recorder-test/recorder-smoke/recorder-smoke.mkv
ffprobe -v error -select_streams v:0 -count_frames \
    -show_entries stream=codec_name,width,height,avg_frame_rate,nb_read_frames \
    -show_entries format=format_name,duration,size \
    -of default=noprint_wrappers=1 \
    ./build-recorder-test/recorder-smoke/recorder-smoke.mkv
```

This does not replace a real X11 source-window capture test, but it verifies
that `appsrc -> x264enc -> h264parse -> matroskamux -> filesink` finalizes a
valid MKV/H.264 file through the application recorder code.

## Run

Run from an X11 session:

```bash
./build/bin/MeetVideoCapture
```

1. Open Google Meet in a browser or open a Zoom Desktop meeting first.
2. Click `Refresh Windows`.
3. Select the browser or Zoom top-level window. Zoom windows are sorted near the top, but any valid top-level window remains selectable.
4. Set `cropX`, `cropY`, `width`, and `height` relative to the selected source window.
5. Click `Start Capture`.
6. Click the green `▶` button to start recording.
7. Click the red `●` button to stop and finalize the MKV file.

The default output directory is:

```text
~/Videos/MeetVideoCapture/
```

## Known Limitations

- MVP is X11-only. Wayland/PipeWire portal support is intentionally not implemented yet.
- A minimized browser or Zoom window may not render.
- A hidden browser or Zoom window may not render.
- XID capture can return black or stale frames depending on compositor, GPU, source app, and driver behavior.
- The visible screen region fallback only works when the meeting video is actually visible on screen.
- In fallback mode, windows covering the selected video region can appear in the capture.
- Crop is numeric in the MVP. The code keeps crop control separate so an interactive overlay can replace it later.

## Troubleshooting

### Missing GStreamer Plugin

If the app reports a missing plugin, install the matching runtime package. For `x264enc`, install:

```bash
sudo apt install gstreamer1.0-plugins-ugly
```

### Black Preview

Try the visible screen region fallback. Some compositor/GPU/source-app combinations do not expose useful XID capture pixels.

### Stale Preview

Meeting video can be visually static, so stale detection is a warning, not a fatal error. If the preview is truly frozen, refresh the window list, restart capture, or use fallback mode.

### No Meeting Source Windows Listed

Confirm the app is running under X11, not Wayland. Also make sure the browser or Zoom window is not minimized.

### Output File Not Playable

Use the stop button and wait for finalization. MKV is chosen to reduce this risk, but a forced kill during finalization can still leave an incomplete file.
