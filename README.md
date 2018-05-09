# SurfaceStreams

Send background-subtracted depth camera video via GStreamer.

This tool records live video of a flat surface with a depth camera, automatically detects the background plane, subtracts that from the video, and sends the result to a GStreamer pipeline. Everything that is part of the background within a configurable distance will turn bright green so it can be used with other GStreamer filters, e.g. using `alpha method=green` and `videomix`.

# random notes
  * UDP ports:
    * 500x: surface stream, 1280x720, MJPEG in GStreamer buffers
    * 600x: front cam stream, 1280x720, MJPEG in GStreamer buffers
    * 700x: raw audio stream in GStreamer buffers
  * camera USB bandwidth allocation is a problem
    * needs uvcvideo quirks parameter (see uvcvideo.conf) and limited FPS
    * check with: $ cat /sys/kernel/debug/usb/devices | grep "B: "
  * inconsistent camera device naming is fixed by 99-camera-symlink.rules
  * table display size: 89x50cm
