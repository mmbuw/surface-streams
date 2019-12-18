gst-launch-1.0 -ve mpegtsmux name=mux ! udpsink host=localhost port=5000 \
  v4l2src      ! video/x-raw,width=1280,height=720,framerate=11/1 ! videoconvert ! x264enc noise-reduction=10000 tune=zerolatency byte-stream=true threads=2 key-int-max=15 intra-refresh=true ! mux. \
  videotestsrc ! video/x-raw,                      framerate=15/1 ! videoconvert ! x264enc noise-reduction=10000 tune=zerolatency byte-stream=true threads=2 key-int-max=15 intra-refresh=true ! mux. \
	pulsesrc     ! audio/x-raw,channels=1,rate=8000                 ! opusenc bitrate=8000 ! mux.  
